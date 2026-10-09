#include "vending_serial.h"
#include "config.h"
#include <atomic>
#include <cstring>
#include "time_sync.h"
#include "auto_rinse_policy.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_task_wdt.h>

// config.h redirects application Serial logs to a silent proxy. Only this
// transport module accesses the real UART0 object.
#undef Serial

static HardwareSerial g_dedicatedUart(1);
static HardwareSerial* const g_uart = &g_dedicatedUart;
static constexpr int g_rxPin = VENDING_RX_PIN;
static constexpr int g_txPin = VENDING_TX_PIN;

static bool g_ready = false;
static std::atomic<bool> g_requested{false};
static bool g_waiting = false;
static bool g_resultPending = false;
static uint32_t g_sentAt = 0;
static String g_raw;
static String g_status;
static String g_resultRaw;
static String g_bleStatus;
static String g_bleRaw;
static uint32_t g_echoCount = 0;
static uint32_t g_resultEchoCount = 0;
static std::atomic<bool> g_pollingEnabled{false};
static std::atomic<bool> g_watchdogRequested{false};
static SemaphoreHandle_t g_mutex = nullptr;
static time_t g_capturedEpoch = 0;
static uint32_t g_lastStartedAt = 0;
static bool g_hasStarted = false;
static CounterProtocol::Reading g_latest;
static bool g_latestValid = false;
static uint32_t g_capturedAt = 0;
static uint32_t g_readingSequence = 0;
static uint32_t g_attemptSequence = 0;
static uint32_t g_finishedAt = 0;
static time_t g_finishedEpoch = 0;
// Temporary diagnostic window to rule out delayed vending-machine replies.
static constexpr uint32_t RESPONSE_TIMEOUT_MS = 5000;
static constexpr size_t MAX_RESPONSE_BYTES = 512;
static constexpr char COUNTER_REQUEST[] = "*1,C,r,2,3,0,75\r";
static constexpr char STATUS_REQUEST[] = "*1,E,r,1,0,5B\r";
static constexpr char RINSE_REQUEST[] = "*1,D,w,1,100,5E\r";
static bool g_isStatus = MACHINE_STATUS_PROBE;
static bool g_resultIsStatus = false;
static bool g_statusDue = false;
static uint32_t g_counterFinishedAt = 0;
static bool g_rinseLatched = false;
static bool g_hasRinsed = false;
static uint32_t g_lastRinseAt = 0;
static String g_rinseAction;
static const char* activeRequest() {
  return g_isStatus ? STATUS_REQUEST : COUNTER_REQUEST;
}

static void finish(const char* status) {
  g_waiting = false;
  g_status = status;
  g_resultRaw = g_raw;
  g_resultEchoCount = g_echoCount;
  ++g_attemptSequence;
  g_finishedAt = millis();
  g_finishedEpoch = TimeSync::now();
  g_resultIsStatus = g_isStatus;
  g_rinseAction = "none";
  if (g_isStatus) {
    uint32_t level = 0, number = 0;
    const bool valid = g_status == "complete" &&
        CounterProtocol::parseMachineStatus(g_raw.c_str(), g_raw.length(),
                                           level, number) == nullptr;
    const auto action = AutoRinsePolicy::decide(valid, level, number,
        g_pollingEnabled.load(), g_rinseLatched, g_hasRinsed,
        millis() - g_lastRinseAt);
    if (action != AutoRinsePolicy::Action::None) {
      if (action == AutoRinsePolicy::Action::Clear) {
        g_rinseLatched = false;
      } else if (action == AutoRinsePolicy::Action::Paused) {
        g_rinseAction = "maintenance_paused";
      } else if (action == AutoRinsePolicy::Action::AlreadyAttempted) {
        g_rinseAction = "already_attempted";
      } else if (action == AutoRinsePolicy::Action::Cooldown) {
        g_rinseAction = "cooldown";
      } else {
        // Latch even on write failure: never blindly repeat an actuator command.
        g_rinseLatched = true;
        g_hasRinsed = true;
        g_lastRinseAt = millis();
        const size_t length = sizeof(RINSE_REQUEST) - 1;
        g_rinseAction = g_uart->write(
            reinterpret_cast<const uint8_t*>(RINSE_REQUEST), length) == length
                ? "sent" : "write_failed";
        g_uart->flush();
      }
    }
  } else {
    g_latestValid = g_status == "complete" &&
        CounterProtocol::parse(g_raw.c_str(), g_raw.length(), g_latest) == nullptr;
    if (g_latestValid) {
      g_capturedAt = millis();
      g_capturedEpoch = TimeSync::now();
      ++g_readingSequence;
    } else {
      g_latest = CounterProtocol::Reading{};
      g_capturedEpoch = 0;
    }
    g_resultPending = true;
#if !COUNTER_SERIAL_DIAGNOSTICS
    g_statusDue = true;
    g_counterFinishedAt = millis();
#endif
  }
  g_isStatus = MACHINE_STATUS_PROBE;
  if (!g_resultIsStatus || MACHINE_STATUS_PROBE) {
    g_bleStatus = g_status;
    g_bleRaw = g_resultRaw;
    g_resultPending = true;
  }
  g_requested.store(false);
#if COUNTER_SERIAL_DIAGNOSTICS
  // Capture is finished before printing. This output also reaches VMC RX.
  // Escape controls so diagnostics cannot inject a raw captured command.
  CounterProtocol::Reading decoded;
  const char* error = g_status == "complete"
      ? CounterProtocol::parse(g_raw.c_str(), g_raw.length(), decoded)
      : "no complete frame";
  Serial.printf("\r\n[COUNTER RX] status=%s bytes=%u valid=%s uptime_ms=%lu\r\n",
                status, static_cast<unsigned>(g_raw.length()),
                g_latestValid ? "true" : "false",
                static_cast<unsigned long>(millis()));
  Serial.print("[COUNTER RAW] ");
  for (size_t i = 0; i < g_raw.length(); ++i) {
    const uint8_t value = static_cast<uint8_t>(g_raw[i]);
    if (value >= 32 && value <= 126) Serial.write(value);
    else Serial.printf("\\x%02X", value);
  }
  Serial.println();
  Serial.printf("[COUNTER ECHO] skipped=%lu\r\n",
                static_cast<unsigned long>(g_resultEchoCount));
  if (error) Serial.printf("[COUNTER ERROR] %s\r\n", error);
  Serial.println("[COUNTER END]");
  Serial.flush();
#endif
}

class StateLock {
 public:
  StateLock() { xSemaphoreTake(g_mutex, portMAX_DELAY); }
  ~StateLock() { xSemaphoreGive(g_mutex); }
};

static void serialTask(void*) {
  bool watched = false;
  for (;;) {
    if (!watched && g_watchdogRequested.load()) {
      watched = esp_task_wdt_add(nullptr) == ESP_OK;
    }
    VendingSerial::loop();
    if (watched) esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

namespace VendingSerial {
bool begin() {
  if (g_ready) return true;
  Serial.setDebugOutput(false);
  Serial.begin(VENDING_BAUD_RATE);
  if (g_uart->setRxBufferSize(VENDING_RX_BUFFER_SIZE) == 0) return false;
  g_uart->begin(VENDING_BAUD_RATE, SERIAL_8N1, g_rxPin, g_txPin);
  if (!*g_uart) return false;
  if (!g_uart->setHwFlowCtrlMode(UART_HW_FLOWCTRL_DISABLE)) {
    g_uart->end();
    return false;
  }
#if COUNTER_SERIAL_DIAGNOSTICS
  Serial.println("\r\n[COUNTER DIAGNOSTIC] MQTT/network disabled; polling every 10s at 9600 8N1; output also reaches machine RX.");
  Serial.flush();
#endif
  g_mutex = xSemaphoreCreateMutex();
  if (!g_mutex) { g_uart->end(); return false; }
  g_ready = true;
  g_lastStartedAt = millis();
  g_hasStarted = true; // First automatic read waits one interval after init.
  if (xTaskCreatePinnedToCore(serialTask, "vending", 4096, nullptr, 2,
                               nullptr, 1) != pdPASS) {
    g_ready = false;
    g_uart->end();
    return false;
  }
  return true;
}

bool isReady() {
  return g_ready;
}
int activeRxPin() { return g_rxPin; }
int activeTxPin() { return g_txPin; }

bool requestCounterRead() {
  if (!g_ready) return false;
  bool expected = false;
  return g_requested.compare_exchange_strong(expected, true);
}

void loop() {
  StateLock lock;
  const uint32_t now = millis();
  if (!g_waiting && g_hasRinsed && now - g_lastRinseAt < 3000) return;
  if (g_ready && g_pollingEnabled && !g_requested.load() &&
      (!g_hasStarted || now - g_lastStartedAt >= COUNTER_POLL_INTERVAL_MS)) {
    g_isStatus = MACHINE_STATUS_PROBE;
    g_statusDue = false;
    requestCounterRead();
  }
  if (g_statusDue && g_pollingEnabled && !g_requested.load() &&
      now - g_counterFinishedAt >= 1500) {
    g_statusDue = false;
    g_isStatus = true;
    requestCounterRead();
  }
  if (!g_requested.load()) return;
  if (!g_waiting) {
    // Explicit BLE reads remain counter reads in the normal environment.
    if (!g_isStatus || MACHINE_STATUS_PROBE) {
      g_lastStartedAt = now;
      g_hasStarted = true;
    }
    g_raw = "";
    g_echoCount = 0;
    if (!g_ready) {
      finish("uart_not_ready");
      return;
    }
    // Bounded drain: do not wait indefinitely on unsolicited input.
    const int staleBytes = g_uart->available();
    for (int i = 0; i < staleBytes; ++i) g_uart->read();
    const char* request = activeRequest();
    const size_t requestLength = strlen(request);
    if (g_uart->write(reinterpret_cast<const uint8_t*>(request),
                     requestLength) != requestLength) {
      finish("write_failed");
      return;
    }
    g_sentAt = millis();
    g_waiting = true;
  }

  // Bound the work per iteration so BLE and networking continue to run.
  for (size_t i = 0; i < 128 && g_uart->available() > 0; ++i) {
    int value = g_uart->read();
    if (value < 0) break;
    char c = static_cast<char>(value);
    if (c == '\r' || c == '\n') {
      if (g_raw.isEmpty()) continue;
      // Some interfaces echo the request before returning the machine frame.
      // Match the entire frame exactly, excluding the request's final CR.
      // Keep the original deadline: echoes must not extend the timeout.
      if (g_raw.length() == strlen(activeRequest()) - 1 &&
          memcmp(g_raw.c_str(), activeRequest(), g_raw.length()) == 0) {
        ++g_echoCount;
        g_raw = "";
        continue;
      }
      finish("complete");
      return;
    }
    if (g_raw.length() >= MAX_RESPONSE_BYTES) {
      finish("overflow");
      return;
    }
    g_raw += c;
  }
  if (millis() - g_sentAt >= RESPONSE_TIMEOUT_MS) finish("timeout");
}

bool takeResult(String& status, String& raw) {
  if (!g_mutex) return false;
  StateLock lock;
  if (!g_resultPending) return false;
  status = g_bleStatus;
  raw = g_bleRaw;
  g_resultPending = false;
  return true;
}

bool latestResult(String& status, String& raw, uint32_t& attempt,
                  uint32_t& finishedAt, time_t& finishedEpoch,
                  uint32_t* echoCount, String* echoRaw,
                  bool* machineStatus, String* rinseAction) {
  if (!g_mutex) return false;
  StateLock lock;
  if (!g_attemptSequence) return false;
  status = g_status;
  raw = g_resultRaw;
  attempt = g_attemptSequence;
  finishedAt = g_finishedAt;
  finishedEpoch = g_finishedEpoch;
  if (echoCount) *echoCount = g_resultEchoCount;
  if (machineStatus) *machineStatus = g_resultIsStatus;
  if (rinseAction) *rinseAction = g_rinseAction;
  if (echoRaw) {
    const String request(g_resultIsStatus ? STATUS_REQUEST : COUNTER_REQUEST);
    *echoRaw = g_resultEchoCount ? request.substring(0, request.length() - 1) : String();
  }
  return true;
}

void setPollingEnabled(bool enabled) {
  g_pollingEnabled = enabled;
}

void enableWatchdog() {
  g_watchdogRequested.store(true);
}

bool latestReading(CounterProtocol::Reading& output, uint32_t& capturedAt,
                   uint32_t* sequence, time_t* capturedEpoch) {
  if (!g_mutex) { output = {}; capturedAt = 0; return false; }
  StateLock lock;
  output = g_latest;
  capturedAt = g_latestValid ? g_capturedAt : 0;
  if (sequence) *sequence = g_readingSequence;
  if (capturedEpoch) *capturedEpoch = g_capturedEpoch;
  return g_latestValid;
}

uint32_t readingSequence() {
  if (!g_mutex) return 0;
  StateLock lock;
  return g_readingSequence;
}
}
