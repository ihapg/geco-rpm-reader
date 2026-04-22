#pragma once

#include "shared.h"
#include <cstdio>

// Log file naming convention
String getLogFileName(time_t timestamp);

// CSV write operation
void logToSD(FILE* file, const SensorData& sensorsData, time_t timestamp);
