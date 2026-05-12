// === Dependencias ===
#include "m7_web.h"

#include <Arduino.h>
#include <RPC.h>
#include <PortentaEthernet.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <mbed.h>

// === Variables ===
// Configuracion de red
// (prod)
const IPAddress ip(192, 168, 1, 254);
const IPAddress dns(192, 168, 1, 1);
const IPAddress gateway(192, 168, 1, 1);
const IPAddress subnet(255, 255, 255, 0);

// (dev)
// const IPAddress ip(169, 254, 1, 2);
// const IPAddress dns(8, 8, 8, 8);
// const IPAddress gateway(169, 254, 112, 33);
// const IPAddress subnet(255, 255, 0, 0);

// Puerto del servidor
EthernetServer server(80);

// Sistema de almacenamiento (SD)
SDMMCBlockDevice block_device;
mbed::FATFileSystem fs("sd");

// Tiempo refresco de datos en ms (RPC)
const uint32_t RPC_UPDATE_INTERVAL = 1000;
static uint32_t lastRPC = 0;

// Gestión multi-slot de clientes HTTP
static const uint8_t MAX_CLIENTS = 5;
static const uint32_t CLIENT_TIMEOUT_MS = 500;
static EthernetClient clients[MAX_CLIENTS];
static uint32_t clientTimestamp[MAX_CLIENTS] = {0};
static const uint32_t ETHERNET_CHECK_INTERVAL = 5000;
static uint32_t lastEthernetCheck = 0;

// Ajuste y sincronización de fecha y hora (NTP)
EthernetUDP ntpUDP;
// (prod)
// NTP IH: 193.144.213.176 ntp.ihcantabria.com
// (dev)
// NTP PC: 169.254.112.33 adaptador Ethernet del PC
NTPClient timeClient(ntpUDP, "ntp.ihcantabria.com", 0);

const uint32_t NTP_UPDATE_INTERVAL = 43200000; // 12h en ms
const uint32_t WATCHDOG_TIMEOUT_MS = 15000;
static int syncDSTOffset = 0;

// Variables de tiempo en caso de desconexión
static unsigned long epochBase = 0;
static unsigned long millisBase = 0;

// Máquina de estados NTP (reconexión no bloqueante en runtime)
enum NtpSyncState : uint8_t
{
  NTP_UNSYNC = 0,
  NTP_SYNCING = 1,
  NTP_RETRY_WAIT = 2
};
static NtpSyncState ntpSyncState = NTP_UNSYNC;
static uint8_t ntpRetryCount = 0;
static uint32_t ntpRetryDeadline = 0;
static const uint8_t NTP_MAX_RETRIES = 3;
static const uint32_t NTP_RETRY_INTERVAL_MS = 3000;

// Log de datos
static LoggingSession loggingSession;

// Export for api_handlers
LoggingSession &getLoggingSession()
{
  return loggingSession;
}

// Polling de tiempo (RTC)
static time_t lastLoggedTime = 0;

// Comprobaciones de servicios
bool sd_check = false;
bool ethernet_check = false;
bool rpc_check = false;
bool ntp_check = false;

// Estructura de datos
SensorData sensorsM7;

// === Funciones ===
// Función para comprobar el estado de la SD
bool checkSD()
{
  Serial.println("Mounting SDCARD...");

  int err = fs.mount(&block_device);
  if (err == 0)
  {
    Serial.println("SDCard OK.");
    return true;
  }

  Serial.println("No filesystem found, formatting... ");
  err = fs.reformat(&block_device);
  if (err == 0)
  {
    Serial.println("SDCard formatted and mounted.");
    return true;
  }

  Serial.println("Error: SDCard mount/format failed!");
  return false;
}

// Función para imprimir la información de la SD
void printSDInfo()
{
  DIR *dir;
  struct dirent *ent;
  int dirIndex = 0;

  Serial.println("List SDCARD content: ");
  if ((dir = opendir("/sd")) != NULL)
  {
    while ((ent = readdir(dir)) != NULL)
    {
      Serial.println(ent->d_name);
      dirIndex++;
    }
    closedir(dir);
  }
  else
  {
    Serial.println("Error opening SDCARD\n");
    return;
  }
  if (dirIndex == 0)
  {
    Serial.println("Empty SDCARD");
  }
}

// === Ejecucion ===
void m7_setup()
{
  Serial.begin(115200);
  delay(2000);

  // (local-dev)
  // while (!Serial)
  //   ;

  // Forzar arranque limpio de CM4
  LL_RCC_ForceCM4Boot();

  // LED control M7
  pinMode(LEDG, OUTPUT);
  digitalWrite(LEDG, HIGH);

  // RPC
  if (RPC.begin())
  {
    rpc_check = true;
    Serial.println("[RPC] conectado con éxito.");
  }
  else
    Serial.println("[RPC] Error: RPC no conectado!");

  // SD
  if (checkSD())
  {
    sd_check = true;
    Serial.println("[SD] conectada.");

    printSDInfo();

    if (getLoggingSession().bootstrapLastLogFileFromSD("/sd"))
    {
      Serial.print("[SD] Last log file: ");
      Serial.println(getLoggingSession().getActiveFileName());
    }
  }
  else
    Serial.println("[SD] no disponible.");

  // Ethernet
  Ethernet.begin(ip, dns, gateway, subnet);
  delay(1500);

  if (Ethernet.linkStatus() == LinkON)
  {
    ethernet_check = true;
    Serial.print("[Ethernet] OK, IP: ");
    Serial.println(Ethernet.localIP());

    // NTP
    ntpUDP.begin(8888);
    timeClient.begin();
    timeClient.setUpdateInterval(NTP_UPDATE_INTERVAL);
    int dstResult = syncTimeNTP(timeClient);
    if (dstResult >= 0)
    {
      syncDSTOffset = dstResult;
      ntp_check = true;
    }
  }
  else
    Serial.println("[Ethernet] no conectado");

  // Servidor
  if (ethernet_check)
  {
    server.begin();
    Serial.println("[HTTP] Servidor arrancado.");
  }

  // Watchdog M7
  mbed::Watchdog::get_instance().start(WATCHDOG_TIMEOUT_MS);
}

void m7_loop()
{
  digitalWrite(LEDG, (millis() / 1000) % 2);

  // Re-detección periódica de red Ethernet
  if (millis() - lastEthernetCheck >= ETHERNET_CHECK_INTERVAL)
  {
    lastEthernetCheck = millis();
    bool linkUp = (Ethernet.linkStatus() == LinkON);

    if (linkUp && !ethernet_check)
    {
      ethernet_check = true;
      Serial.print("[Ethernet] RECUPERADO, IP: ");
      Serial.println(Ethernet.localIP());

      ntpUDP.begin(8888);
      timeClient.begin();
      timeClient.setUpdateInterval(NTP_UPDATE_INTERVAL);

      // NTP: sync diferida y no bloqueante para no interrumpir grabación activa.
      // La máquina de estados NTP intentará sincronizar fuera de sesión de log.
      ntpSyncState = NTP_SYNCING;
      ntpRetryCount = 0;

      server.begin();
      Serial.println("[HTTP] Servidor reactivado.");
    }
    else if (!linkUp && ethernet_check)
    {
      millisBase = millis();
      epochBase = timeClient.getEpochTime() + syncDSTOffset;
      ethernet_check = false;
      ntp_check = false;
      ntpSyncState = NTP_UNSYNC;
      Serial.println("[Ethernet] LINK perdido.");

      for (uint8_t i = 0; i < MAX_CLIENTS; i++)
      {
        if (clients[i])
          clients[i].stop();
      }
    }
  }

  // Máquina de estados NTP (no bloqueante en reconexión runtime)
  // Solo intenta sincronizar cuando no hay grabación activa para evitar gaps en el log.
  if (ntpSyncState == NTP_SYNCING && !loggingSession.isActive())
  {
    int dstResult = tryNTPSyncOnce(timeClient);
    if (dstResult >= 0)
    {
      syncDSTOffset = dstResult;
      ntp_check = true;
      ntpSyncState = NTP_UNSYNC;

      // Ancla lastLoggedTime al tiempo real para evitar catch-up masivo.
      time_t recoveredNow = (time_t)timeClient.getEpochTime() + syncDSTOffset;
      if (recoveredNow > 0)
        lastLoggedTime = recoveredNow - 1;

      Serial.println("[NTP] Sincronizado tras recuperación de red.");
    }
    else
    {
      ntpRetryCount++;
      if (ntpRetryCount >= NTP_MAX_RETRIES)
      {
        ntp_check = false;
        ntpSyncState = NTP_UNSYNC;
        Serial.println("[NTP] No disponible tras recuperación de red.");
      }
      else
      {
        ntpRetryDeadline = millis() + NTP_RETRY_INTERVAL_MS;
        ntpSyncState = NTP_RETRY_WAIT;
      }
    }
  }
  else if (ntpSyncState == NTP_RETRY_WAIT && millis() >= ntpRetryDeadline)
  {
    ntpSyncState = NTP_SYNCING;
  }

  // Sincronización de datos con M4 (RPC)
  if (rpc_check && (millis() - lastRPC > RPC_UPDATE_INTERVAL))
  {
    sensorsM7 = RPC.call("get_data").as<SensorData>();
    lastRPC = millis();
  }

  if (ntp_check)
  {
    epochBase = timeClient.getEpochTime() + syncDSTOffset;
  }

  // Calculate time: NTP synchronized or fallback to internal clock
  time_t now = ntp_check
                   ? (time_t)timeClient.getEpochTime() + syncDSTOffset
                   : (time_t)(epochBase + ((millis() - millisBase) / 1000));

  if (now > lastLoggedTime)
  {
    if (lastLoggedTime == 0)
      lastLoggedTime = now - 1;

    while (lastLoggedTime < now)
    {
      lastLoggedTime++;
      loggingSession.writeData(sensorsM7, lastLoggedTime);
    }
  }

  // Periodic NTP time update
  if (ntp_check && timeClient.update())
  {
    syncDSTOffset = getDSTOffset(timeClient.getEpochTime());
    Serial.println("[NTP] Fecha y hora sincronizadas con éxito.");
  }

  loggingSession.update(sensorsM7, sd_check, now);

  // Gestión multi-slot de clientes HTTP

  if (ethernet_check)
  {
    EthernetClient incoming = server.accept();
    if (incoming)
    {
      bool placed = false;
      for (uint8_t i = 0; i < MAX_CLIENTS; i++)
      {
        if (!clients[i])
        {
          clients[i] = incoming;
          clientTimestamp[i] = millis();
          placed = true;
          break;
        }
      }
      if (!placed)
      {
        incoming.println("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n");
        incoming.flush();
        incoming.stop();
      }
    }
    for (uint8_t i = 0; i < MAX_CLIENTS; i++)
    {
      if (!clients[i])
        continue;

      if (clients[i].available())
      {
        processRequest(clients[i]);
        clients[i].flush();
        clients[i].stop();
      }
      else if (!clients[i].connected() || (millis() - clientTimestamp[i]) > CLIENT_TIMEOUT_MS)
      {
        if (clients[i].connected())
        {
          clients[i].println("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n");
          clients[i].flush();
        }
        clients[i].stop();
      }
      // else: slot activo dentro del timeout → esperar siguiente iteración
    }
  }

  // Kick watchdog al final del ciclo completo
  mbed::Watchdog::get_instance().kick();
}
