#include "time_sync.h"
#include <sys/time.h>
#include <math.h>

void TimeSync::begin() {
  setenv("TZ", "IST-5:30", 1);
  tzset();
}

void TimeSync::startWifiSync() {
  // SNTP runs asynchronously and retries/resynchronizes in the network stack.
  configTzTime("IST-5:30", "pool.ntp.org", "time.nist.gov");
}

bool TimeSync::isValid() {
  time_t epoch = time(nullptr);
  return epoch >= 1735689600LL && epoch < 4102444800LL; // 2025 .. 2099
}

time_t TimeSync::now() {
  return isValid() ? time(nullptr) : 0;
}

bool TimeSync::setFromModem(int year, int month, int day, int hour,
                            int minute, int second, float timezoneHours) {
  if (year < 2025 || year > 2099 || month < 1 || month > 12 || day < 1 ||
      day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
      second < 0 || second > 59 || !isfinite(timezoneHours) ||
      timezoneHours < -12 || timezoneHours > 14) return false;
  tm local = {};
  local.tm_year = year - 1900;
  local.tm_mon = month - 1;
  local.tm_mday = day;
  local.tm_hour = hour;
  local.tm_min = minute;
  local.tm_sec = second;
  local.tm_isdst = 0;
  // mktime interprets the fields in our fixed IST timezone. Correct from
  // modem local timezone to UTC without applying the offset twice.
  time_t epoch = mktime(&local);
  if (local.tm_year != year - 1900 || local.tm_mon != month - 1 ||
      local.tm_mday != day) return false; // reject normalized invalid dates
  epoch += static_cast<time_t>(19800 - lroundf(timezoneHours * 3600));
  if (epoch < 1735689600LL || epoch >= 4102444800LL) return false;
  timeval tv = {epoch, 0};
  return settimeofday(&tv, nullptr) == 0;
}

String TimeSync::format(time_t epoch, const char* pattern) {
  if (!epoch) return "";
  tm local;
  localtime_r(&epoch, &local);
  char buffer[32];
  return strftime(buffer, sizeof(buffer), pattern, &local) ? String(buffer) : String();
}
