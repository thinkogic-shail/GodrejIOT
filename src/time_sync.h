#pragma once
#include <Arduino.h>
#include <time.h>

namespace TimeSync {
void begin();
void startWifiSync();
bool isValid();
bool setFromModem(int year, int month, int day, int hour, int minute,
                  int second, float timezoneHours);
time_t now();
String format(time_t epoch, const char* pattern = "%Y-%m-%d %H:%M:%S");
}
