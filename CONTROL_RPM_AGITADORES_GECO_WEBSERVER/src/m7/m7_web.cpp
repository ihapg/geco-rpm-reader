// === Dependencias ===
#include "m7_web.h"

#include <Arduino.h>
#include <RPC.h>
#include <PortentaEthernet.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <NTPClient.h>

// === Variables ===
// Configuracion server
// (prod)
// const IPAddress ip(192, 168, 1, 254);
// const IPAddress dns(192, 168, 1, 1);
// const IPAddress gateway(192, 168, 1, 1);
// const IPAddress subnet(255, 255, 255, 0);

// (dev)
const IPAddress ip(169, 254, 1, 2);
const IPAddress dns(8, 8, 8, 8);
const IPAddress gateway(169, 254, 112, 33);
const IPAddress subnet(255, 255, 0, 0);

// Puerto del servidor
EthernetServer server(80);

// Sistema de almacenamiento (SD)
SDMMCBlockDevice block_device;
mbed::FATFileSystem fs("sd");

// Tiempo refresco de datos en ms (RPC)
const uint32_t RPC_UPDATE_INTERVAL = 1000;
static uint32_t lastRPC = 0;

// Ajuste y sincronización de fecha y hora (NTP)
EthernetUDP ntpUDP;
// (prod)
// NTP IH: 193.144.213.176 ntp.ihcantabria.com
// (dev)
// NTP PC: 169.254.112.33 adaptador Ethernet del PC
NTPClient timeClient(ntpUDP, "169.254.112.33", 0);

const uint32_t NTP_UPDATE_INTERVAL = 43200000; // 12h en ms
static int syncDSTOffset = 0;

// Log de datos
static LoggingSession loggingSession;

// Export for api_handlers
LoggingSession& getLoggingSession()
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
  server.begin();
  Serial.println("[HTTP] Servidor arrancado.");
}

void m7_loop()
{
  digitalWrite(LEDG, (millis() / 1000) % 2);

  // Sincronización de datos con M4
  if (millis() - lastRPC > RPC_UPDATE_INTERVAL)
  {
    sensorsM7 = RPC.call("get_data").as<SensorData>();
    lastRPC = millis();
  }

  // Log de datos (polling RTC)
  time_t now = (time_t)timeClient.getEpochTime() + syncDSTOffset;

  if (now > lastLoggedTime)
  {
    if (lastLoggedTime == 0)
      lastLoggedTime = now - 1;

    while (lastLoggedTime < now)
    {
      lastLoggedTime++;

      // Write log through logging session
      loggingSession.writeData(sensorsM7, lastLoggedTime);
    }
  }

  // Sincronización de tiempo (no bloqueante, usa intervalo interno de NTPClient)
  if (timeClient.update())
  {
    syncDSTOffset = getDSTOffset(timeClient.getEpochTime());
    Serial.println("[NTP] Fecha y hora sincronizadas con éxito.");
  }

  // Actualizar sesión de logging
  loggingSession.update(sensorsM7, sd_check, now);

  // Gestión individual de clientes
  EthernetClient client = server.accept();

  if (client)
  {
    unsigned long timeout = millis();
    while (client.connected() && !client.available() && (millis() - timeout) < 200)
      ;

    if (client.available())
      processRequest(client);

    client.flush();
    delay(50);
    client.stop();
  }
}