// Servicio Web

#include <Arduino.h>
#include <RPC.h>
#include <PortentaEthernet.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

#include "shared.h"
#include "SDMMCBlockDevice.h"
#include "FATFileSystem.h"

// === Variables ===
// Configuracion server
// IP (produccion)
// IPAddress ip(192, 168, 1, 254);

// IP-dev (desarrollo)
IPAddress ip(169, 254, 1, 2);

// Puerto del servidor
EthernetServer server(80);

// Clientes simultaneos
const unsigned int MAX_CLIENTS = 5;
EthernetClient clients[MAX_CLIENTS];

// SD
SDMMCBlockDevice block_device;
mbed::FATFileSystem fs("sd");

// Checks
bool sd_check = false;
bool ethernet_check = false;
bool rpc_check = false;

// Estructura con datos
SensorData sensorsM7;

// === Funciones ===
// Función que devuelve el archivo solicitado de la SD
void serveFile(EthernetClient &client, const char *path, const char *mime)
{
  FILE *file = fopen(path, "r");
  if (!file)
  {
    client.println("HTTP/1.1 404 Not Found\r\n\r\n");
    return;
  }

  client.println("HTTP/1.1 200 OK");
  client.print("Content-Type: ");
  client.println(mime);
  client.println();

  char buf[512];
  size_t n;

  while ((n = fread(buf, 1, sizeof(buf), file)) > 0)
  {
    client.write((uint8_t *)buf, n);
  }

  fclose(file);
}

// Función que devuelve los datos de los sensores en formato JSON
void serveJSON(EthernetClient &client)
{

  // Creacion documento JSON que se enviará
  JsonDocument doc;

  try
  {
    // auto result = RPC.call("get_data").as<SensorData>();
    // sensorsM7 = result;
    sensorsM7 = RPC.call("get_data").as<SensorData>();
  }
  catch (__exception ex)
  {
    Serial.print("ERROR: Get data failed: ");
    Serial.println(ex.name);
  }

  /* client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Access-Control-Allow-Origin: *"); // evitar bloqueos del navegador
  client.println("Connection: close");
  client.println();

  // Metodo con client.print()
  client.print("[");
  for (size_t i = 0; i < (sizeof(sensorsM7.sensors) / sizeof(sensorsM7.sensors[0])); i++)
  {
    client.print("\t{\"sensor\":");
    client.print(sensorsM7.sensors[i].id);
    client.print(",\"rpm\":");
    client.print(sensorsM7.sensors[i].rpm, 2);
    client.print(",\"hz\":");
    client.print(sensorsM7.sensors[i].hz, 2);
    client.print("}");

    if (i < 11)
      client.print(",");
  }
  client.print("]"); */

  // Metodo con ArduinoJson
  for (auto &sensor : sensorsM7.sensors)
  {
    JsonObject obj = doc.add<JsonObject>();
    obj["sensor"] = sensor.id;
    obj["rpm"] = sensor.rpm;
    obj["hz"] = sensor.hz;
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Access-Control-Allow-Origin: *"); // evitar bloqueos del navegador
  client.println("Connection: close");
  client.println();

  // Serializar directamente al cliente el JsonDocument
  serializeJson(doc, client);  
}

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
  // dev>prod
  while (!Serial)
    ;

  // Forzar arranque limpio de CM4
  LL_RCC_ForceCM4Boot();

  // LED
  pinMode(LEDG, OUTPUT);
  digitalWrite(LEDG, HIGH);

  pinMode(LEDR, OUTPUT);
  digitalWrite(LEDR, HIGH);

  // RPC
  if (RPC.begin())
  {
    rpc_check = true;
    Serial.println("[RPC] conectado con éxito.");
  }
  else
  {
    rpc_check = false;
    Serial.println("[RPC] Error: RPC no conectado!");
  }

  // SD
  if (!checkSD())
  {
    Serial.println("[SD] no disponible.");
  }
  else
  {
    Serial.println("[SD] conectada.");
    printSDInfo();
  }

  // Red
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

  /* // Comprobar si hay nuevos clientes
  EthernetClient newClient = server.accept();
  if (newClient)
  {
    bool added = false;
    for (size_t i = 0; i < (sizeof(clients) / sizeof(clients[0])); i++)
    {
      if (!clients[i] || !clients[i].connected())
      {
        clients[i] = newClient;
        added = true;

        Serial.println("Nuevo cliente añadido.");
        break;
      }
    }

    if (!added)
    {
      Serial.println("Máximo de clientes alcanzado. Rechazando nuevo cliente.");
      newClient.stop();
    }
  }

  // Recibir peticiones de clientes
  for (size_t i = 0; i < (sizeof(clients) / sizeof(clients[0])); i++)
  {
    if (clients[i] && clients[i].connected())
    {
      // Recepción de peticiones
      String request = clients[i].readStringUntil('\r');
      clients[i].read(); // \n

      Serial.print("Request: ");
      Serial.println(request);

      String path = "/";
      if (request.startsWith("GET "))
      {
        int start = 4;
        int end = request.indexOf(' ', start);

        path = request.substring(start, end);
      }

      Serial.print("Ruta: ");
      Serial.println(path);

      //Gestión de respuestas API
      if (path == "/" || path == "/index.html")
      {
        serveFile(clients[i], "/sd/index.html", "text/html");
      }
      else if (path == "/script.js")
      {
        serveFile(clients[i], "/sd/script.js", "application/javascript");
      }
      else if (path == "/data.json")
      {
        serveJSON(clients[i]);
      }
      else
      {
        clients[i].println("HTTP/1.1 404 Not Found\r\n\r\n");
      }

      delay(50);

      clients[i].stop();
      clients[i] = EthernetClient(); // limpiar cliente
      Serial.println("Cliente desconectado");
    }
  } */

  // Gestión individual de clientes
  EthernetClient client = server.accept();

  if (client)
  {
    Serial.println("Cliente conectado.");

    unsigned long timeout = millis();
    while (client.connected() && !client.available() && (millis() - timeout) < 1000)
      ;

    if (client.available())
    {
      String request = client.readStringUntil('\r');
      client.flush(); // limpiar resto de la peticion (headers)

      int start = request.indexOf(' ') + 1;
      int end = request.indexOf(' ', start);
      String path = request.substring(start, end);

      if (path == "/" || path == "/index.html")
        serveFile(client, "/sd/index.html", "text/html");
      else if (path == "/script.js")
        serveFile(client, "/sd/script.js", "application/javascript");
      else if (path == "/data.json")
        serveJSON(client);
      else
        client.println("HTTP/1.1 404 Not Found \r\n\r\n");
    }

    delay(10);
    client.stop();
    Serial.println("Cliente cerrado.");
  }

  // Comprobacion visual RPC
  digitalWrite(LEDR, rpc_check ? LOW : HIGH);
}