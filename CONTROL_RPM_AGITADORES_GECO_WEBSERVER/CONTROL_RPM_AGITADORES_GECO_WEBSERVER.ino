// AUTORES: MIGUEL RODRÍGUEZ LÓPEZ y ADRIÁN PINEY GUTIÉRREZ
// Programa diseñado para medir las RPM y Hz de los agitadores del GeCo
// Versión: 7 -- Tests SD (HTML + JS)

#include <PortentaEthernet.h>
#include <Ethernet.h>

#include "SDMMCBlockDevice.h"
#include "FATFileSystem.h"

//################### VARIABLES ###################

// Array con definición de los pines para los sensores
const uint8_t SENSOR_PINS[12] = { D0, D1, D2, D4, D5, D6, D7, D8, A2, D13, D14, D21 };

// Arrays con tiempos de interrupciones para cada sensor
volatile unsigned long lastInterrupt[12] = { 0 };
volatile unsigned long interval[12] = { 0 };
volatile bool newData[12] = { false };

// Arrays con los datos de cada sensor
float hz[12] = { 0 };
float rpm[12] = { 0 };
unsigned long lastUpdate[12] = { 0 };

// Tiempo límite para resetear variables
const unsigned long TIMEOUT = 12000;

// Configuración de IP y puerto del servidor (produccion)
// IPAddress ip(192, 168, 1, 254);

// IP para pruebas (desarrollo)
IPAddress ip(169, 254, 1, 2);

// Servidor
EthernetServer server(80);

// Clientes simultáneos
const unsigned int MAX_CLIENTS = 5;
EthernetClient clients[MAX_CLIENTS];

//SD
SDMMCBlockDevice block_device;
mbed::FATFileSystem fs("sd");

// Checks
bool sd_check = false;
bool ethernet_check = false;

//################### FUNCTIONS ###################

// Función base para gestionar interrupciones y asignación a cada sensor
void baseInterruptionHandler(uint8_t id) {
  unsigned long now = micros();
  interval[id] = now - lastInterrupt[id];
  lastInterrupt[id] = now;
  newData[id] = true;
}

void handle0() {
  baseInterruptionHandler(0);
}
void handle1() {
  baseInterruptionHandler(1);
}
void handle2() {
  baseInterruptionHandler(2);
}
void handle3() {
  baseInterruptionHandler(3);
}
void handle4() {
  baseInterruptionHandler(4);
}
void handle5() {
  baseInterruptionHandler(5);
}
void handle6() {
  baseInterruptionHandler(6);
}
void handle7() {
  baseInterruptionHandler(7);
}
void handle8() {
  baseInterruptionHandler(8);
}
void handle9() {
  baseInterruptionHandler(9);
}
void handle10() {
  baseInterruptionHandler(10);
}
void handle11() {
  baseInterruptionHandler(11);
}

void (*HANDLERS_LIST[12])() = {
  handle0, handle1, handle2, handle3, handle4, handle5,
  handle6, handle7, handle8, handle9, handle10, handle11
};

// Función para actualizar los datos de los sensores
void updateSensors() {
  for (int i = 0; i < (sizeof(newData) / sizeof(newData[0])); i++) {
    if (newData[i]) {
      newData[i] = false;
      if (interval[i] > 0) {
        float sec = interval[i] * 1e-6;
        hz[i] = (1 / sec) * 10;
        rpm[i] = hz[i] * 6;
        lastUpdate[i] = millis();
      }
    }
  }
}

// Función para comprobar y resetear los datos tras inactividad
void checkTimeouts() {
  unsigned long now = millis();
  for (int i = 0; i < (sizeof(rpm) / sizeof(rpm[0])); i++) {
    if (rpm[i] > 0 && (now - lastUpdate[i] > TIMEOUT)) {
      rpm[i] = 0;
      hz[i] = 0;
      Serial.print("Sensor ");
      Serial.print(i + 1);
      Serial.println(" reset");
    }
  }
}

// Abrir archivo solicitado
void serveFile(EthernetClient &client, const char *path, const char *mime) {
  FILE *file = fopen(path, "r");
  if (!file) {
    client.println("HTTP/1.1 404 Not Found\r\n\r\n");
    return;
  }

  client.println("HTTP/1.1 200 OK");
  client.print("Content-Type: ");
  client.println(mime);
  client.println();

  char buf[128];
  size_t n;

  while ((n = fread(buf, 1, sizeof(buf), file)) > 0) {
    client.write((uint8_t *)buf, n);
  }
  fclose(file);
}

void serveJSON(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.print("[");
  for (int i = 0; i < 12; i++) {
    client.print("\t{\"sensor\":");
    client.print(i + 1);
    client.print(",\"rpm\":");
    client.print(rpm[i], 2);
    client.print(",\"hz\":");
    client.print(hz[i], 2);
    client.print("}");
    if (i < 11) client.print(",");
  }
  client.print("]");
}

bool checkSD() {
  Serial.println("Mounting SDCARD...");

  int err = fs.mount(&block_device);
  if (err == 0) {
    Serial.println("SDCard OK.");
    return true;
  }

  Serial.println("No filesystem found, formatting... ");
  err = fs.reformat(&block_device);
  if (err == 0) {
    Serial.println("SDCard formatted and mounted.");
    return true;
  }

  Serial.println("Error: SDCard mount/format failed!");
  return false;
}

void printSDInfo() {
  DIR *dir;
  struct dirent *ent;
  int dirIndex = 0;

  Serial.println("List SDCARD content: ");
  if ((dir = opendir("/sd")) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      Serial.println(ent->d_name);
      dirIndex++;
    }
    closedir(dir);
  } else {
    Serial.println("Error opening SDCARD\n");
    while (1)
      ;
  }
  if (dirIndex == 0) {
    Serial.println("Empty SDCARD");
  }
}


//################### RUN ###################

void setup() {
  Serial.begin(115200);
  delay(500);

  // === Sensors
  for (int i = 0; i < (sizeof(SENSOR_PINS) / sizeof(SENSOR_PINS[0])); i++) {
    pinMode(SENSOR_PINS[i], INPUT_PULLUP);
    int irq = digitalPinToInterrupt(SENSOR_PINS[i]);
    if (irq != NOT_AN_INTERRUPT) {
      attachInterrupt(irq, HANDLERS_LIST[i], RISING);
    } else {
      Serial.print("Pin ");
      Serial.print(SENSOR_PINS[i]);
      Serial.println(" interruption not supported.");
    }
  }

  // === Ethernet
  Ethernet.begin(ip);
  delay(1000);

  if (Ethernet.linkStatus() == LinkON) {
    ethernet_check = true;
    Serial.print("Ethernet OK, IP: ");
    Serial.println(Ethernet.localIP());
  } else {
    Serial.println("Ethernet without link.");
  }

  // === Server
  server.begin();
  Serial.println("HTTP Server started.");

  // === SD
  sd_check = checkSD();
  if (!sd_check) {
    Serial.println("SD not available.");
  } else {
    printSDInfo();
  }
}

void loop() {
  // Actualización de sensores y comprobación de tiempos de reset
  updateSensors();
  checkTimeouts();

  // Revisar si hay un nuevo cliente
  EthernetClient newClient = server.available();
  if (newClient) {
    bool added = false;
    for (int i = 0; i < (sizeof(clients) / sizeof(clients[0])); i++) {
      if (!clients[i] || !clients[i].connected()) {
        clients[i] = newClient;
        added = true;
        Serial.println("Client added");
        break;
      }
    }
    if (!added) {
      Serial.println("Máximo de clientes alcanzado, rechazando nuevo cliente");
      newClient.stop();
    }
  }

  // Recibir peticiones de los clientes conectados y devolver archivos requeridos
  for (int i = 0; i < (sizeof(clients) / sizeof(clients[0])); i++) {
    if (clients[i] && clients[i].connected()) {

      //Implementar adq de peticiones y respuestas API
      String request = clients[i].readStringUntil('\r');
      clients[i].read();  // \n

      Serial.println(request);

      String path = "/";
      if (request.startsWith("GET ")) {
        int start = 4;
        int end = request.indexOf(' ', start);
        path = request.substring(start, end);
      }

      Serial.print("Ruta: ");
      Serial.println(path);

      if (path == "/" || path == "/index.html") {
        serveFile(clients[i], "/sd/index.html", "text/html");
      } else if (path == "/script.js") {
        serveFile(clients[i], "/sd/script.js", "application/javascript");
      } else if (path == "/data.json") {
        serveJSON(clients[i]);
      } else {
        clients[i].println("HTTP/1.1 404 Not Found\r\n\r\n");
      }

      delay(50);
      clients[i].stop();
      clients[i] = EthernetClient();  //limpiar cliente
      Serial.println("Client disconnected.");
    }
  }
}
