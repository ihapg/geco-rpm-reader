#include "sensor_log_writer.h"
#include "../time/time_service.h"

String getLogFileName(time_t timestamp)
{
  // Formato: GECO_THRUSTER_YYYYMMDD_HHMMSS.log
  String fileName = getDateTimeString(timestamp, "GECO_THRUSTER_%Y%m%d_%H%M%S.log");
  return fileName;
}

void logToSD(FILE* file, const SensorData &sensorsData, time_t timestamp)
{
  // "a" = "append" adición de escritura y "+" permite lectura (read, write, append)
  // FILE *file = fopen(path, "a+");
  if (!file) return;

  fseek(file, 0, SEEK_END);

  // --- 1. CABECERA (Si el archivo es nuevo) ---
  if (ftell(file) == 0)
  {
    // Cabecera de inicio con fecha y hora
    String startDate = getDateTimeString(timestamp, "%d/%m/%Y %H:%M:%S");

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
  fprintf(file, "%s", getDateTimeString(timestamp, "%H:%M:%S").c_str());

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
  // fflush(file);
  // fclose(file);
}
