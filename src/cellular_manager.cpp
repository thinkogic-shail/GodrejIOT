#define TINY_GSM_MODEM_A7672X
#define TINY_GSM_RX_BUFFER 2048
#include <TinyGsmClient.h>
#include "cellular_manager.h"
#include "config.h"
#include "storage.h"
#include "time_sync.h"
#include <atomic>

static TinyGsm modem(Serial2);

// TinyGSM 0.12.0's A7672X plain TCP send waits for a TLS CCHSEND reply.
// Use the documented CIPSEND acknowledgement for our non-TLS MQTT transport.
// Keep the upstream receive/URC implementation; do not patch installed libraries.
class A7672TcpClient : public TinyGsmClient {
 public:
  explicit A7672TcpClient(TinyGsm& device) : TinyGsmClient(device, 0) {}
  using TinyGsmClient::write;
  int connect(const char* host, uint16_t port) override { return connect(host, port, 15); }
  int connect(IPAddress ip, uint16_t port) override {
    return connect(ip.toString().c_str(), port, 15);
  }
  int connect(const char* host, uint16_t port, int timeoutSeconds) override {
    stop();
    // NETOPEN is idempotent here: querying first avoids failing reconnects
    // because the modem's packet service is already open.
    at->sendAT("+NETOPEN?");
    String response;
    if (at->waitResponse(2000L, response) != 1) return 0;
    if (response.indexOf("+NETOPEN: 1") < 0) {
      at->sendAT("+NETOPEN");
      if (at->waitResponse(15000L, "+NETOPEN: 0") != 1) return 0;
    }
    at->sendAT("+CIPOPEN=0,\"TCP\",\"", host, "\",", port);
    sock_connected = at->waitResponse(timeoutSeconds * 1000UL,
                                      "+CIPOPEN: 0,0", "+CIPOPEN: 0,1", "ERROR") == 1;
    return sock_connected;
  }
  void stop() override {
    rx.clear();
    sock_available = 0;
    got_data = false;
    sock_connected = false;
    at->sendAT("+CIPCLOSE=0");
    at->waitResponse(1000L);
  }
  size_t write(const uint8_t* buffer, size_t size) override {
    if (!sock_connected) return 0;
    size_t sent = 0;
    while (sent < size) {
      const size_t chunk = min(size - sent, static_cast<size_t>(1500));
      at->sendAT("+CIPSEND=0,", chunk);
      if (at->waitResponse(2000L, ">") != 1) break;
      Serial2.write(buffer + sent, chunk);
      Serial2.flush();
      if (at->waitResponse(15000L, "+CIPSEND:") != 1) break;
      String acknowledgement = Serial2.readStringUntil('\n');
      acknowledgement.trim();
      int socket, requested, confirmed;
      char extra;
      if (sscanf(acknowledgement.c_str(), "%d,%d,%d%c", &socket,
                 &requested, &confirmed, &extra) != 3) break;
      if (socket != 0 || requested != static_cast<int>(chunk) || confirmed != requested) break;
      sent += chunk;
    }
    if (sent != size) sock_connected = false;
    return sent;
  }
};

static A7672TcpClient tcpClient(modem);
static String apn, apnUser, apnPass, simPin;
static bool started = false, initialized = false, connected = false;
static bool pinAttempted = false;
static uint32_t lastAttempt = 0, powerStarted = 0, lastTimeAttempt = 0;
static bool attempted = false, timeAttempted = false;
static std::atomic<const char*> currentStatus{"disabled"};

Client& CellularManager::client() { return tcpClient; }
bool CellularManager::isConnected() { return connected; }
const char* CellularManager::status() { return currentStatus.load(); }

void CellularManager::begin() {
  apn = SIM_APN; apnUser = SIM_APN_USER; apnPass = SIM_APN_PASS; simPin = SIM_PIN;
  Storage::loadSimSettings(apn, apnUser, apnPass, simPin);
  Serial2.setRxBufferSize(4096);
  Serial2.begin(MODEM_BAUD_RATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  Serial2.setHwFlowCtrlMode(UART_HW_FLOWCTRL_DISABLE);
  // Probe before pulsing PWRKEY: a blind pulse can turn off a running modem.
  started = true;
  currentStatus = "starting";
}

void CellularManager::disconnect() {
  tcpClient.stop();
  connected = false;
  started = false;
  currentStatus = "maintenance";
}

void CellularManager::ensureConnected() {
  if (!started) return;
  uint32_t now = millis();
  if (attempted && now - lastAttempt < 15000) return;
  attempted = true;
  lastAttempt = now;
  if (!initialized) {
    if (!modem.testAT(1000)) {
      connected = false;
      currentStatus = "modem_not_responding";
      if (!powerStarted || now - powerStarted >= 120000) {
        digitalWrite(MODEM_POWER_KEY_PIN, HIGH);
        pinMode(MODEM_POWER_KEY_PIN, OUTPUT);
        digitalWrite(MODEM_POWER_KEY_PIN, LOW);
        delay(1000);
        digitalWrite(MODEM_POWER_KEY_PIN, HIGH);
        powerStarted = now ? now : 1;
      }
      return;
    }
    if (!modem.init()) { currentStatus = "modem_init_failed"; return; }
    initialized = true;
    modem.sendAT("+CTZU=1"); modem.waitResponse(1000L);
  }
  auto sim = modem.getSimStatus(1000);
  if (sim != SIM_READY) {
    connected = false;
    currentStatus = sim == SIM_LOCKED ? "sim_locked" : "sim_missing_or_error";
    // Never repeatedly submit a possibly incorrect PIN and exhaust attempts.
    if (sim == SIM_LOCKED && simPin.length() && !pinAttempted) {
      pinAttempted = true;
      modem.sendAT("+CPIN?");
      String pinStatus;
      if (modem.waitResponse(2000L, pinStatus) == 1) {
        if (pinStatus.indexOf("SIM PIN") >= 0) modem.simUnlock(simPin.c_str());
        else if (pinStatus.indexOf("SIM PUK") >= 0) currentStatus = "sim_puk_required";
      }
    }
    if (sim == SIM_ERROR && !modem.testAT(1000)) initialized = false;
    return;
  }
  if (!modem.isNetworkConnected()) {
    connected = false;
    currentStatus = "registering";
    if (!modem.testAT(1000)) initialized = false;
    return;
  }
  if (!connected || !modem.isGprsConnected()) {
    connected = false;
    currentStatus = "activating_data";
    if (apn.length()) {
      modem.sendAT("+CGDCONT=1,\"IP\",\"", apn, "\"");
      if (modem.waitResponse(2000L) != 1) { currentStatus = "apn_rejected"; return; }
    }
    if (apnUser.length()) {
      modem.sendAT("+CGAUTH=1,1,\"", apnUser, "\",\"", apnPass, "\"");
      if (modem.waitResponse(2000L) != 1) { currentStatus = "apn_auth_rejected"; return; }
    } else {
      modem.sendAT("+CGAUTH=1,0");
      modem.waitResponse(2000L); // Optional on firmware without PDP auth support.
    }
    modem.sendAT("+CGATT=1");
    if (modem.waitResponse(15000L) != 1) { currentStatus = "attach_failed"; return; }
    modem.sendAT("+CGACT=1,1");
    if (modem.waitResponse(15000L) != 1) { currentStatus = "data_activation_failed"; return; }
    modem.sendAT("+CIPRXGET=1");
    if (modem.waitResponse(2000L) != 1) { currentStatus = "receive_mode_failed"; return; }
    connected = modem.isGprsConnected();
    if (!connected) { currentStatus = "data_not_ready"; return; }
    currentStatus = "connected";
    return; // Keep activation and time synchronization in separate attempts.
  }
  currentStatus = "connected";
  // NITZ/CCLK does not depend on ESP32's Wi-Fi SNTP stack.
  now = millis();
  uint32_t interval = TimeSync::isValid() ? 21600000UL : 60000UL;
  if (!timeAttempted || now - lastTimeAttempt >= interval) {
    timeAttempted = true;
    lastTimeAttempt = now;
    int year, month, day, hour, minute, second;
    float zone;
    // NTP is optional; if unsupported or blocked, try the carrier's CCLK/NITZ.
    modem.sendAT("+CNTP=\"pool.ntp.org\",0");
    if (modem.waitResponse(2000L) == 1) {
      modem.sendAT("+CNTP");
      if (modem.waitResponse(15000L, "+CNTP:") == 1) {
        Serial2.readStringUntil('\n');
      }
    }
    if (modem.getNetworkTime(&year, &month, &day, &hour, &minute, &second, &zone)) {
      TimeSync::setFromModem(year, month, day, hour, minute, second, zone);
    }
  }
}
