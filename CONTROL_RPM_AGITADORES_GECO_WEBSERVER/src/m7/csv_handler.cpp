#include "csv_handler.h"

#include <Arduino.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <NTPClient.h>
#include <mbed.h>

String getTimeStamp()
{
  time_t seconds = time(NULL);
  struct tm *t = localtime(&seconds);
  char buffer[32];

  strftime(buffer, sizeof(buffer), "%H:%M:%S", t);
  return String(buffer);
}

String getDate()
{
  time_t seconds = time(NULL);
  struct tm *t = localtime(&seconds);
  char buffer[32];

  strftime(buffer, sizeof(buffer), "%d/%m/%Y", t);
  return String(buffer);  
}

String getLogFileName()
{
  time_t seconds = time(NULL);
  struct tm *t = localtime(&seconds);
  char buffer[64];

  // Formato: GECO_THRUSTER_YYYYMMDD_HHMMSS.log
  strftime(buffer, sizeof(buffer), "GECO_THRUSTER_%Y%m%d_%H%M%S.log", t);
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

void logToSD(FILE* file, SensorData &sensorsData)
{
  // "a" = "append" adición de escritura y "+" permite lectura (read, write, append)
  // FILE *file = fopen(path, "a+");
  if (!file) return;

  fseek(file, 0, SEEK_END);

  // --- 1. CABECERA (Si el archivo es nuevo) ---
  if (ftell(file) == 0)
  {
    // Cabecera de inicio con fecha y hora
    String startDate = getDate() + " " + getTimeStamp();

    fprintf(file, "%s", "START RECORD:");
    fprintf(file, "\t%s", startDate.c_str());
    fprintf(file, "\r\n");

    // Cabecera datos
    fprintf(file, "%s", "Time");

    for (auto &sensor : sensorsData.sensors)
    {
      std::string headerName = sensor.id + "(rpm)";
      fprintf(file, "\t%s", headerName.c_str());
    }
    
    for (auto &sensor : sensorsData.sensors)
    {
      std::string headerName = sensor.id + "(hz)";
      fprintf(file, "\t%s", headerName.c_str());
    }
    fprintf(file, "\r\n");
  }

  // --- 2. DATOS (Una sola fila por marca de tiempo) ---
  fprintf(file, "%s", getTimeStamp().c_str());

  // Valores RPM
  for (auto &sensor : sensorsData.sensors)
  {
    fprintf(file, "\t%.2f", sensor.rpm);
  }

  // Valores HZ
  for (auto &sensor : sensorsData.sensors)
  {
    fprintf(file, "\t%.2f", sensor.hz);
  }

  fprintf(file, "\r\n");
  fflush(file);
  // fclose(file);
}