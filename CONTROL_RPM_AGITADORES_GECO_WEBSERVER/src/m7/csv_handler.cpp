#include "csv_handler.h"

#include <Arduino.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <NTPClient.h>
#include <mbed.h>

// EthernetUDP ntpUDP;
// NTPClient timeClient(ntpUDP, "169.254.112.33", 3600);

String getTimeStamp()
{
  time_t seconds = time(NULL);
  struct tm *t = localtime(&seconds);
  char buffer[32];

  strftime(buffer, sizeof(buffer), "%H:%M:%S", t);
  return String(buffer);
}

String getLogFileName()
{
  time_t seconds = time(NULL);
  struct tm *t = localtime(&seconds);
  char buffer[32];

  // Formato: GECO_THRUSTER_YYYYMMDD_HHMMSS.log
  strftime(buffer, sizeof(buffer), "GECO_THRUSTER_%Y%m%d.log", t);
  return String(buffer);
}

// Sincronización del reloj
bool syncTimeNTP(NTPClient &timeClient)
{
  for (int i = 0; i < 3; i++)
  {
    Serial.println("[NTP] Intento de sincronización NTP...");
    if (timeClient.forceUpdate())
    {
      set_time(timeClient.getEpochTime());
      Serial.println("[NTP] Sincronizado con éxito!");
      return true;
    }
    Serial.println("[NTP] Error en la sincronización.");
    delay(1500);
  }
  return false;
}

void logToSD(SensorData &sensorsData)
{
  // arreglar error nombre hora, aislando variable de nombre
  String dynamicPath = "/sd/" + getLogFileName();

  // "a" = "append" adición de escritura y "+" permite lectura (read, write, append)
  FILE *file = fopen(dynamicPath.c_str(), "a+");
  if (!file)
    return;

  fseek(file, 0, SEEK_END);

  // --- 1. CABECERA (Si el archivo es nuevo) ---
  if (ftell(file) == 0)
  {
    fprintf(file, "%-10s", "Time(s)");
    // Cabeceras RPM
    for (auto &sensor : sensorsData.sensors)
    {
      std::string headerName = sensor.id + "(rpm)";
      fprintf(file, "\t%-10s", headerName.c_str());
    }
    // Cabeceras HZ
    for (auto &sensor : sensorsData.sensors)
    {
      std::string headerName = sensor.id + "(hz)";
      fprintf(file, "\t%-10s", headerName.c_str());
    }
    fprintf(file, "\r\n");
  }

  // --- 2. DATOS (Una sola fila por marca de tiempo) ---
  fprintf(file, "%-10s", getTimeStamp().c_str());

  // Valores RPM
  for (auto &sensor : sensorsData.sensors)
  {
    fprintf(file, "\t%-10.2f", sensor.rpm);
  }

  // Valores HZ
  for (auto &sensor : sensorsData.sensors)
  {
    fprintf(file, "\t%-10.2f", sensor.hz);
  }

  fprintf(file, "\r\n");
  fflush(file);
  fclose(file);
}

/* bool setupNTP()
{
    ntpUDP.begin(8888);
    timeClient.begin();

    bool syncronized = syncTimeNTP();
    return syncronized;
} */