#include "time_service.h"
#include <cstring>
#include <cstdio>
#include <mbed.h>

// Función para devolver la fecha con el formato que se pase (plantilla String)
String getDateTimeString(time_t seconds, String temp)
{
  struct tm *t = localtime(&seconds);
  char buffer[64];

  strftime(buffer, sizeof(buffer), temp.c_str(), t);
  return String(buffer);
}

String getTimeMillis(time_t seconds, long ms)
{
  String time = getDateTimeString(seconds, "%H:%M:%S");
  char fullTime[64];
  snprintf(fullTime, sizeof(fullTime), "%s.%03ld", time.c_str(), ms);

  return String(fullTime);
}

// Función para calcular horario verano/invierno europeo
// Reglas: último domingo de Marzo a las 02:00 -> UTC+2 ; último domingo de Octubre a las 03:00 -> UTC+1
int getDSTOffset(time_t utc)
{
  struct tm *t = gmtime(&utc);
  int month = t->tm_mon + 1;
  int day = t->tm_mday;
  int wday = t->tm_wday; // Domingo = 0
  int hour = t->tm_hour;

  int summerOffset = 7200;
  int winterOffset = 3600;

  // Comprobación de último domingo del mes
  bool isLastSunday = (wday == 0 && day >= 25);
  // Cambio a verano UTC+2
  bool summerStart = (month == 3 && isLastSunday && hour >= 1);
  // Cambio a invierno
  bool winterStart = (month == 10 && isLastSunday && hour >= 1);

  if (month == 3 && (day > 25 || summerStart))
    return summerOffset;
  if (month == 10 && !(day > 25 || winterStart))
    return summerOffset;
  if (month > 3 && month < 10)
    return summerOffset;
  return winterOffset;
}

// Sincronización del reloj (bloqueante, solo para uso en boot)
int syncTimeNTP(NTPClient &timeClient)
{
  for (int i = 0; i < 3; i++)
  {
    Serial.println("[NTP] Intento de sincronización NTP...");
    if (timeClient.forceUpdate())
    {
      time_t utc = timeClient.getEpochTime();
      set_time(utc);
      Serial.println("[NTP] Sincronizado con éxito!");
      return getDSTOffset(utc);
    }
    Serial.println("[NTP] Error en la sincronización.");
    delay(1500);
  }
  return -1;
}

// Intento único de sincronización NTP (no bloqueante en llamada: sin retry ni delay)
// Devuelve el offset DST en segundos si tiene éxito, -1 si falla.
int tryNTPSyncOnce(NTPClient &timeClient)
{
  Serial.println("[NTP] Intento de sincronización NTP...");
  if (timeClient.forceUpdate())
  {
    time_t utc = timeClient.getEpochTime();
    set_time(utc);
    Serial.println("[NTP] Sincronizado con éxito!");
    return getDSTOffset(utc);
  }
  Serial.println("[NTP] Error en la sincronización.");
  return -1;
}
