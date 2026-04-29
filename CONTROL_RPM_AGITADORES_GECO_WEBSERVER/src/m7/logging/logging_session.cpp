#include "logging_session.h"
#include "sensor_log_writer.h"

LoggingSession::LoggingSession()
    : isLogging(false), logFile(nullptr), logRoute(""), lastLogFile(""), stopTimer(0)
{
}

void LoggingSession::update(const SensorData &sensorsData, bool sdAvailable, time_t currentTimestamp)
{
  // Detect if any sensor is running
  bool isRunning = false;
  for (const auto &sensor : sensorsData.sensors)
  {
    if (sensor.rpm > 0.0f)
    {
      isRunning = true;
      break;
    }
  }

  if (!sdAvailable)
  {
    // If SD is not available, stop logging if active
    if (isLogging && logFile)
    {
      fclose(logFile);
      logFile = nullptr;
      isLogging = false;
      logRoute = "";
      Serial.print("<<< Grabación finalizada (SD no disponible) -> ");
      Serial.println(lastLogFile);
    }
    return;
  }

  if (isRunning)
  {
    if (!isLogging)
    {
      // Start recording
      isLogging = true;
      lastLogFile = getLogFileName(currentTimestamp);
      logRoute = "/sd/" + lastLogFile;
      logFile = fopen(logRoute.c_str(), "a+");

      Serial.print(">>> Iniciando grabación -> ");
      Serial.println(lastLogFile);
    }
    // Keep recording
    stopTimer = millis();
  }
  else
  {
    if (isLogging && (millis() - stopTimer > STOP_DELAY))
    {
      // Stop recording
      isLogging = false;
      logRoute = "";
      if (logFile)
        fclose(logFile);
      logFile = nullptr;

      Serial.print("<<< Grabación finalizada -> ");
      Serial.println(lastLogFile);
    }
  }
}

void LoggingSession::writeData(const SensorData &sensorsData, time_t timestamp)
{
  if (isLogging && logFile)
  {
    logToSD(logFile, sensorsData, timestamp);
  }
}

bool LoggingSession::bootstrapLastLogFileFromSD(const char *dirPath)
{
  lastLogFile = "";

  DIR *dir = opendir(dirPath);
  if (!dir)
    return false;

  struct dirent *ent;
  String newest = "";

  while ((ent = readdir(dir)) != nullptr)
  {
    String name = ent->d_name;
    if (name.endsWith(".log"))
    {
      if (newest.isEmpty() || name > newest)
        newest = name;
    }
  }

  closedir(dir);

  if (!newest.isEmpty())
  {
    lastLogFile = newest;
    return true;
  }

  return false;
}

bool LoggingSession::isActive() const
{
  return isLogging;
}

String LoggingSession::getActiveFileName() const
{
  return lastLogFile;
}

FILE *LoggingSession::getFile() const
{
  return logFile;
}

void LoggingSession::close()
{
  if (logFile)
  {
    fclose(logFile);
    logFile = nullptr;
  }
  isLogging = false;
  logRoute = "";
  lastLogFile = "";
}
