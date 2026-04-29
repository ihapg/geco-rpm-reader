#pragma once

#include "shared.h"
#include <cstdio>
#include <Arduino.h>
#include <mbed.h>

// Logging session management
class LoggingSession
{
private:
  bool isLogging;
  FILE *logFile;
  String logRoute;
  String lastLogFile;
  uint32_t stopTimer;
  static const uint32_t STOP_DELAY = 3000;

public:
  LoggingSession();

  // Update session state based on sensor activity and SD availability
  void update(const SensorData &sensorsData, bool sdAvailable, time_t currentTimestamp);

  // Write data to the log file
  void writeData(const SensorData &sensorsData, time_t timestamp);

  // Get last file's name from the registered logs in SD
  bool bootstrapLastLogFileFromSD(const char *dirPath = "/sd");

  // Query session state
  bool isActive() const;
  String getActiveFileName() const;
  FILE *getFile() const;

  // Cleanup
  void close();
};
