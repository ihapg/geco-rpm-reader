#pragma once

#include "shared.h"
#include <NTPClient.h>

// String getTimeStamp();
String getLogFileName();
bool syncTimeNTP(NTPClient& timeClient);
void logToSD(FILE* file, SensorData& sensorsData);