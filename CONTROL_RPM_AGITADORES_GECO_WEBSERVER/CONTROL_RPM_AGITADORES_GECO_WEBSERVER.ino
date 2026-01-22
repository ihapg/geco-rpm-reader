// AUTORES: MIGUEL RODRÍGUEZ LÓPEZ y ADRIÁN PINEY GUTIÉRREZ
// Programa diseñado para medir las RPM y Hz de los agitadores del GeCo
// Versión: 7 -- Tests SD (HTML + JS)

#include <PortentaEthernet.h>
#include <Ethernet.h>

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

// Configuración de IP y puerto del servidor
IPAddress ip(192, 168, 1, 254);
// IP para pruebas (comentar linea anterior y descomentar siguiente)
// IPAddress ip(169, 254, 1, 2);
EthernetServer server(80);

// Función base para gestionar interrupciones y asignación a cada sensor
void baseInterruptionHandler(uint8_t id) {
  unsigned long now = micros();
  interval[id] = now - lastInterrupt[id];
  lastInterrupt[id] = now;
  newData[id] = true;
}

void handle0() { baseInterruptionHandler(0); }
void handle1() { baseInterruptionHandler(1); }
void handle2() { baseInterruptionHandler(2); }
void handle3() { baseInterruptionHandler(3); }
void handle4() { baseInterruptionHandler(4); }
void handle5() { baseInterruptionHandler(5); }
void handle6() { baseInterruptionHandler(6); }
void handle7() { baseInterruptionHandler(7); }
void handle8() { baseInterruptionHandler(8); }
void handle9() { baseInterruptionHandler(9); }
void handle10() { baseInterruptionHandler(10); }
void handle11() { baseInterruptionHandler(11); }

void (*HANDLERS_LIST[12])() = {
  handle0, handle1, handle2, handle3, handle4, handle5,
  handle6, handle7, handle8, handle9, handle10, handle11
};

// --- Clientes simultáneos ---
const unsigned int MAX_CLIENTS = 5;
EthernetClient clients[MAX_CLIENTS];

void setup() {
  Serial.begin(115200);
  delay(500);

  for (int i = 0; i < (sizeof(SENSOR_PINS) / sizeof(SENSOR_PINS[0])); i++) {
    pinMode(SENSOR_PINS[i], INPUT_PULLUP);
    int irq = digitalPinToInterrupt(SENSOR_PINS[i]);
    if (irq != NOT_AN_INTERRUPT) {
      attachInterrupt(irq, HANDLERS_LIST[i], RISING);
    } else {
      Serial.print("Pin ");
      Serial.print(SENSOR_PINS[i]);
      Serial.println(" no soporta interrupción");
    }
  }

  Ethernet.begin(ip);
  delay(500);

  Serial.print("IP asignada: ");
  Serial.println(Ethernet.localIP());

  server.begin();
}

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
        Serial.println("Cliente agregado");
        break;
      }
    }
    if (!added) {
      Serial.println("Máximo de clientes alcanzado, rechazando nuevo cliente");
      newClient.stop();
    }
  }

  // Generar y enviar HTML a los clientes conectados
  for (int i = 0; i < (sizeof(clients) / sizeof(clients[0])); i++) {
    if (clients[i] && clients[i].connected()) {

      clients[i].println("HTTP/1.1 200 OK");
      clients[i].println("Content-Type: text/html");
      clients[i].println("Connection: close");
      clients[i].println();
      clients[i].println("<!DOCTYPE HTML>");
      clients[i].println("<html>");
      clients[i].println("<head>");
      clients[i].println("<meta http-equiv='refresh' content='1'>");
      clients[i].println("<title>Tests GeCo</title>");
      clients[i].println("<style>");
      clients[i].println("table { width: 80%; border-collapse: collapse; text-align: center; font-family: Arial; margin: auto; }");
      clients[i].println("th, td { padding: 10px; border: 1px solid #000; text-align: center; font-size: 20px; }");
      clients[i].println("h1 { text-align: center; font-family: Arial; }");
      clients[i].println("</style>");
      clients[i].println("</head>");
      clients[i].println("<body>");

      clients[i].println("<h1>Tests GeCo</h1>");
      clients[i].println("<table>");
      clients[i].println("<tr><th>Agitador</th><th>RPM</th><th>Hz</th></tr>");

      // Array con colores para filas
      const char* colors[] = { "#D3D3D3", "#9B9B9B" };

      // Bucle para generar filas por cada sensor
      for (int j = 0; j < 12; j++) {
        clients[i].print("<tr style='background:");
        clients[i].print(colors[(j % 2)]);
        clients[i].print(";'>");

        clients[i].print("<td>Agitador ");
        clients[i].print(j + 1);
        clients[i].print("</td>");

        clients[i].print("<td>");
        clients[i].print(rpm[j], 1);
        clients[i].print("</td>");

        clients[i].print("<td>");
        clients[i].print(hz[j], 2);
        clients[i].print("</td>");

        clients[i].println("</tr>");
      }
      clients[i].println("</table>");
      clients[i].println("</body></html>");

      clients[i].stop();
      clients[i] = EthernetClient();  //limpiar cliente
      Serial.println("Cliente desconectado");
    }
  }
}
