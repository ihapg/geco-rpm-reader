#pragma once

#include <Arduino.h>
#include <NTPClient.h>

// NTP time synchronization and DST offset calculation
int syncTimeNTP(NTPClient& timeClient);
int getDSTOffset(time_t utc);

// Time formatting utilities
String getDateTimeString(time_t seconds, String temp);
String getTimeMillis(time_t seconds, long ms);
