// Servicio Web

#include <Arduino.h>
#include <RPC.h>
#include <PortentaEthernet.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

#include "shared.h"
#include "web_router.h"

#include "SDMMCBlockDevice.h"
#include "FATFileSystem.h"

// === Variables ===
// Configuracion server
// IP (produccion)
// const IPAddress ip(192, 168, 1, 254);

// IP-dev (desarrollo)
const IPAddress ip(169, 254, 1, 2);

// Puerto del servidor
EthernetServer server(80);

// SD
SDMMCBlockDevice block_device;
mbed::FATFileSystem fs("sd");

// Checks
bool sd_check = false;
bool ethernet_check = false;
bool rpc_check = false;

// Tiempo refresco de datos
const uint32_t RPC_UPDATE_INTERVAL = 1000; // ms

// Estructura con datos
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
  {
    Serial.println("[RPC] Error: RPC no conectado!");
  }

  // SD
  if (checkSD())
  {
    sd_check = true;
    Serial.println("[SD] conectada.");
    printSDInfo();
  }
  else
  {
    Serial.println("[SD] no disponible.");
  }

  // Ethernet
  Ethernet.begin(ip);
  delay(500);

  if (Ethernet.linkStatus() == LinkON)
  {
    ethernet_check = true;
    Serial.print("[Ethernet] OK, IP: ");
    Serial.println(Ethernet.localIP());
  }
  else
  {
    Serial.println("[Ethernet] no conectado");
  }

  // Servidor
  server.begin();
  Serial.println("[HTTP] Server arrancado.");

  // Inicialización de variables
  for (auto &sensor : sensorsM7.sensors)
  {
    sensor.id = 0;
    sensor.rpm = 0;
    sensor.hz = 0;
  }
}

void m7_loop()
{
  digitalWrite(LEDG, (millis() / 1000) % 2);

  // Sincronización de datos con M4
  static uint32_t lastRPC = 0;
  if (millis() - lastRPC > RPC_UPDATE_INTERVAL)
  {
    sensorsM7 = RPC.call("get_data").as<SensorData>();
    lastRPC = millis();
  }

  // Gestión individual de clientes
  EthernetClient client = server.accept();

  if (client)
  {
    unsigned long timeout = millis();
    while (client.connected() && !client.available() && (millis() - timeout) < 1000)
      ;

    if (client.available())
    {
      processRequest(client);
    }

    delay(10);
    client.stop();
  }
}