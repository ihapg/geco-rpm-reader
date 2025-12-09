

//AUTOR:MIGUEL RODRÍGUEZ LOPEZ y ADRIÁN PINEY GUTIÉRREZ//
//ESTE PROGRAMA SE UTILIZA PARA MEDIR LAS RPM DE LOS AGITADORES DEL GECO//
// Definición de pines para los 12 sensores
#define SENSOR1_PIN D0
#define SENSOR2_PIN D1
#define SENSOR3_PIN D2
#define SENSOR4_PIN D4
#define SENSOR5_PIN D5
#define SENSOR6_PIN D6
#define SENSOR7_PIN D7
#define SENSOR8_PIN D8
#define SENSOR9_PIN A2
#define SENSOR10_PIN D13
#define SENSOR11_PIN D14
#define SENSOR12_PIN D21

#include <PortentaEthernet.h>
#include <Ethernet.h>

// The IP address will be dependent on your local network:
IPAddress ip(169, 254, 1, 2);
EthernetServer server(80);
// Variables para manejar el tiempo entre interrupciones
volatile unsigned long lastInterruptTime1 = 0;
volatile unsigned long lastInterruptTime2 = 0;
volatile unsigned long lastInterruptTime3 = 0;
volatile unsigned long lastInterruptTime4 = 0;
volatile unsigned long lastInterruptTime5 = 0;
volatile unsigned long lastInterruptTime6 = 0;
volatile unsigned long lastInterruptTime7 = 0;
volatile unsigned long lastInterruptTime8 = 0;
volatile unsigned long lastInterruptTime9 = 0;
volatile unsigned long lastInterruptTime10 = 0;
volatile unsigned long lastInterruptTime11 = 0;
volatile unsigned long lastInterruptTime12 = 0;

volatile unsigned long timeBetweenInterrupts1 = 0;
volatile unsigned long timeBetweenInterrupts2 = 0;
volatile unsigned long timeBetweenInterrupts3 = 0;
volatile unsigned long timeBetweenInterrupts4 = 0;
volatile unsigned long timeBetweenInterrupts5 = 0;
volatile unsigned long timeBetweenInterrupts6 = 0;
volatile unsigned long timeBetweenInterrupts7 = 0;
volatile unsigned long timeBetweenInterrupts8 = 0;
volatile unsigned long timeBetweenInterrupts9 = 0;
volatile unsigned long timeBetweenInterrupts10 = 0;
volatile unsigned long timeBetweenInterrupts11 = 0;
volatile unsigned long timeBetweenInterrupts12 = 0;

volatile bool newData1 = false;
volatile bool newData2 = false;
volatile bool newData3 = false;
volatile bool newData4 = false;
volatile bool newData5 = false;
volatile bool newData6 = false;
volatile bool newData7 = false;
volatile bool newData8 = false;
volatile bool newData9 = false;
volatile bool newData10 = false;
volatile bool newData11 = false;
volatile bool newData12 = false;

// Variables para calcular las RPM
unsigned int rpm1 = 0;
unsigned int rpm2 = 0;
unsigned int rpm3 = 0;
unsigned int rpm4 = 0;
unsigned int rpm5 = 0;
unsigned int rpm6 = 0;
unsigned int rpm7 = 0;
unsigned int rpm8 = 0;
unsigned int rpm9 = 0;
unsigned int rpm10 = 0;
unsigned int rpm11 = 0;
unsigned int rpm12 = 0;

unsigned long currentTime = 0;

// Variables para probar contadores 0rpm
volatile unsigned long lastUpdateTime = 0;
void updateTimeLapse() {
  if (((currentTime - lastUpdateTime) / 1000000) > 12) {
    rpm1 = 0;
    lastUpdateTime = 0;
  }
}

// Funciones de interrupción para cada sensor
void handleInterrupt1() {
  currentTime = micros();
  timeBetweenInterrupts1 = currentTime - lastInterruptTime1;
  lastInterruptTime1 = currentTime;
  newData1 = true;
}

void handleInterrupt2() {
  currentTime = micros();
  timeBetweenInterrupts2 = currentTime - lastInterruptTime2;
  lastInterruptTime2 = currentTime;
  newData2 = true;
}

void handleInterrupt3() {
  currentTime = micros();
  timeBetweenInterrupts3 = currentTime - lastInterruptTime3;
  lastInterruptTime3 = currentTime;
  newData3 = true;
}
void handleInterrupt4() {
  currentTime = micros();
  timeBetweenInterrupts4 = currentTime - lastInterruptTime4;
  lastInterruptTime4 = currentTime;
  newData4 = true;
}
void handleInterrupt5() {
  currentTime = micros();
  timeBetweenInterrupts5 = currentTime - lastInterruptTime5;
  lastInterruptTime5 = currentTime;
  newData5 = true;
}
void handleInterrupt6() {
  currentTime = micros();
  timeBetweenInterrupts6 = currentTime - lastInterruptTime6;
  lastInterruptTime6 = currentTime;
  newData6 = true;
}
void handleInterrupt7() {
  currentTime = micros();
  timeBetweenInterrupts7 = currentTime - lastInterruptTime7;
  lastInterruptTime7 = currentTime;
  newData7 = true;
}
void handleInterrupt8() {
  currentTime = micros();
  timeBetweenInterrupts8 = currentTime - lastInterruptTime8;
  lastInterruptTime8 = currentTime;
  newData8 = true;
}
void handleInterrupt9() {
  currentTime = micros();
  timeBetweenInterrupts9 = currentTime - lastInterruptTime9;
  lastInterruptTime9 = currentTime;
  newData9 = true;
}
void handleInterrupt10() {
  currentTime = micros();
  timeBetweenInterrupts10 = currentTime - lastInterruptTime10;
  lastInterruptTime10 = currentTime;
  newData10 = true;
}
void handleInterrupt11() {
  currentTime = micros();
  timeBetweenInterrupts11 = currentTime - lastInterruptTime11;
  lastInterruptTime11 = currentTime;
  newData11 = true;
}
void handleInterrupt12() {
  currentTime = micros();
  timeBetweenInterrupts12 = currentTime - lastInterruptTime12;
  lastInterruptTime12 = currentTime;
  newData12 = true;
}
void setup() {
  Serial.begin(115200);  // Inicia la comunicación Serial

  // Configura los pines de los sensores como entradas con pull-up interno
  pinMode(SENSOR1_PIN, INPUT_PULLUP);
  pinMode(SENSOR2_PIN, INPUT_PULLUP);
  pinMode(SENSOR3_PIN, INPUT_PULLUP);
  pinMode(SENSOR4_PIN, INPUT_PULLUP);
  pinMode(SENSOR5_PIN, INPUT_PULLUP);
  pinMode(SENSOR6_PIN, INPUT_PULLUP);
  pinMode(SENSOR7_PIN, INPUT_PULLUP);
  pinMode(SENSOR8_PIN, INPUT_PULLUP);
  pinMode(SENSOR9_PIN, INPUT_PULLUP);
  pinMode(SENSOR10_PIN, INPUT_PULLUP);
  pinMode(SENSOR11_PIN, INPUT_PULLUP);
  pinMode(SENSOR12_PIN, INPUT_PULLUP);
  // Configura las interrupciones para cada sensor
  attachInterrupt(digitalPinToInterrupt(SENSOR1_PIN), handleInterrupt1, RISING);
  attachInterrupt(digitalPinToInterrupt(SENSOR2_PIN), handleInterrupt2, RISING);
  attachInterrupt(digitalPinToInterrupt(SENSOR3_PIN), handleInterrupt3, RISING);
  attachInterrupt(digitalPinToInterrupt(SENSOR4_PIN), handleInterrupt4, RISING);
  attachInterrupt(digitalPinToInterrupt(SENSOR5_PIN), handleInterrupt5, RISING);
  attachInterrupt(digitalPinToInterrupt(SENSOR6_PIN), handleInterrupt6, RISING);
  attachInterrupt(digitalPinToInterrupt(SENSOR7_PIN), handleInterrupt7, RISING);
  attachInterrupt(digitalPinToInterrupt(SENSOR8_PIN), handleInterrupt8, RISING);
  attachInterrupt(digitalPinToInterrupt(SENSOR9_PIN), handleInterrupt9, RISING);
  attachInterrupt(digitalPinToInterrupt(SENSOR10_PIN), handleInterrupt10, RISING);
  attachInterrupt(digitalPinToInterrupt(SENSOR11_PIN), handleInterrupt11, RISING);
  attachInterrupt(digitalPinToInterrupt(SENSOR12_PIN), handleInterrupt12, RISING);
  // start the Ethernet connection and the server:
  Ethernet.begin(ip);
  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true) {
      delay(1);
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }

  // start the server
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  // Procesa los datos de Sensor 1
  if (newData1) {
    rpm1 = 60 / (timeBetweenInterrupts1 * 1E-6);  // Calcular RPM
    newData1 = false;
    Serial.print("Sensor 1 RPM: ");
    Serial.println(rpm1);
    //Test 0rpm
    lastUpdateTime = currentTime;
  }

  // Procesa los datos de Sensor 2
  if (newData2) {
    rpm2 = 60 / (timeBetweenInterrupts2 * 1E-6);  // Calcular RPM
    newData2 = false;
    Serial.print("Sensor 2 RPM: ");
    Serial.println(rpm2);
  }

  // Procesa los datos de Sensor 3
  if (newData3) {
    rpm3 = 60 / (timeBetweenInterrupts3 * 1E-6);  // Calcular RPM
    newData3 = false;
    Serial.print("Sensor 3 RPM: ");
    Serial.println(rpm3);
  }
  // Procesa los datos de Sensor 4
  if (newData4) {
    rpm4 = 60 / (timeBetweenInterrupts4 * 1E-6);  // Calcular RPM
    newData4 = false;
    Serial.print("Sensor 4 RPM: ");
    Serial.println(rpm4);
  }
  // Procesa los datos de Sensor 5
  if (newData5) {
    rpm5 = 60 / (timeBetweenInterrupts5 * 1E-6);  // Calcular RPM
    newData5 = false;
    Serial.print("Sensor 5 RPM: ");
    Serial.println(rpm5);
  }
  // Procesa los datos de Sensor 6
  if (newData6) {
    rpm6 = 60 / (timeBetweenInterrupts6 * 1E-6);  // Calcular RPM
    newData6 = false;
    Serial.print("Sensor 6 RPM: ");
    Serial.println(rpm6);
  }
  // Procesa los datos de Sensor 7
  if (newData7) {
    rpm7 = 60 / (timeBetweenInterrupts7 * 1E-6);  // Calcular RPM
    newData7 = false;
    Serial.print("Sensor 7 RPM: ");
    Serial.println(rpm7);
  }
  // Procesa los datos de Sensor 8
  if (newData8) {
    rpm8 = 60 / (timeBetweenInterrupts8 * 1E-6);  // Calcular RPM
    newData8 = false;
    Serial.print("Sensor 8 RPM: ");
    Serial.println(rpm8);
  }
  // Procesa los datos de Sensor 9
  if (newData9) {
    rpm9 = 60 / (timeBetweenInterrupts9 * 1E-6);  // Calcular RPM
    newData9 = false;
    Serial.print("Sensor 9 RPM: ");
    Serial.println(rpm9);
  }
  // Procesa los datos de Sensor 10
  if (newData10) {
    rpm10 = 60 / (timeBetweenInterrupts10 * 1E-6);  // Calcular RPM
    newData10 = false;
    Serial.print("Sensor 10 RPM: ");
    Serial.println(rpm10);
  }
  // Procesa los datos de Sensor 11
  if (newData11) {
    rpm11 = 60 / (timeBetweenInterrupts11 * 1E-6);  // Calcular RPM
    newData11 = false;
    Serial.print("Sensor 11 RPM: ");
    Serial.println(rpm11);
  }
  // Procesa los datos de Sensor 12
  if (newData12) {
    rpm12 = 60 / (timeBetweenInterrupts12 * 1E-6);  // Calcular RPM
    newData12 = false;
    Serial.print("Sensor 12 RPM: ");
    Serial.println(rpm12);
  }

  //Ejecución lectura de contadores y reseteo de RPMs
  updateTimeLapse();

  // listen for incoming clients
  EthernetClient client = server.accept();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 1");         // refresh the page automatically every X sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<head>");
          client.println("    <meta charset=\"UTF-8\" />");
          client.println("    <title>Agitadores RPM</title>");
          client.println("    <script src=\"script_test_agitadores.js\" defer></script>");
          client.println("    <style>");
          client.println("        body {");
          client.println("            font-family: system-ui, sans-serif;");
          client.println("            background: #f6f7fb;");
          client.println("            margin: 0;");
          client.println("            padding: 2rem;");
          client.println("            display: flex;");
          client.println("            justify-content: center;");
          client.println("        }");
          client.println("");
          client.println("        .card {");
          client.println("            background: #fffacd;");
          client.println("            border-radius: 16px;");
          client.println("            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1);");
          client.println("            padding: 2rem;");
          client.println("            width: 320px;");
          client.println("            text-align: center;");
          client.println("        }");
          client.println("");
          client.println("        table {");
          client.println("            width: 100%;");
          client.println("            background-color: #ffffffb0;");
          client.println("            border: 1px black;");
          client.println("            border-collapse: collapse;");
          client.println("            border-radius: 16px;");
          client.println("            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.1);");
          client.println("        }");
          client.println("");
          client.println("        th,");
          client.println("        td {");
          client.println("            width: 50%;");
          client.println("        }");
          client.println("    </style>");
          client.println("</head>");
          client.println("");
          client.println("<body>");
          client.println("    <div class=\"card\">");
          client.println("        <h1>Agitadores RPM</h1>");
          //==============================================================================
          //Mostrar contadores de tiempo
          client.print("          <div id=\"timer1\" style=\"color:#b22222;margin:10px\">Current time: ");
          client.print((millis() / 1000));
          client.println(" s</div>");
          client.print("          <div id=\"timer2\" style=\"color:#b22222;margin:10px\">Last time sensor 1 updated: ");
          client.print((lastUpdateTime / 1000000));
          client.println(" s</div>");
          //==============================================================================
          client.println("        <table id=\"table\">");
          client.println("            <thead>");
          client.println("                <tr>");
          client.println("                    <th>Sensor</th>");
          client.println("                    <th>RPM</th>");
          client.println("                </tr>");
          client.println("            </thead>");
          client.println("            <tbody id=\"table_data\">");
          client.println("              <tr>");
          client.print("                <td>");
          client.print("Agitador 1");
          client.println("</td>");
          client.print("                <td>");
          client.print(rpm1);
          client.println("</td>");
          client.println("              </tr>");
          client.println("              <tr>");
          client.print("                <td>");
          client.print("Agitador 2");
          client.println("</td>");
          client.print("                <td>");
          client.print(rpm2);
          client.println("</td>");
          client.println("              </tr>");
          client.println("              <tr>");
          client.print("                <td>");
          client.print("Agitador 3");
          client.println("</td>");
          client.print("                <td>");
          client.print(rpm3);
          client.println("</td>");
          client.println("              </tr>");
          client.println("              <tr>");
          client.print("                <td>");
          client.print("Agitador 4");
          client.println("</td>");
          client.print("                <td>");
          client.print(rpm4);
          client.println("</td>");
          client.println("              </tr>");
          client.println("              <tr>");
          client.print("                <td>");
          client.print("Agitador 5");
          client.println("</td>");
          client.print("                <td>");
          client.print(rpm5);
          client.println("</td>");
          client.println("              </tr>");
          client.println("              <tr>");
          client.print("                <td>");
          client.print("Agitador 6");
          client.println("</td>");
          client.print("                <td>");
          client.print(rpm6);
          client.println("</td>");
          client.println("              </tr>");
          client.println("              <tr>");
          client.print("                <td>");
          client.print("Agitador 7");
          client.println("</td>");
          client.print("                <td>");
          client.print(rpm7);
          client.println("</td>");
          client.println("              </tr>");
          client.println("              <tr>");
          client.print("                <td>");
          client.print("Agitador 8");
          client.println("</td>");
          client.print("                <td>");
          client.print(rpm8);
          client.println("</td>");
          client.println("              </tr>");
          client.println("              <tr>");
          client.print("                <td>");
          client.print("Agitador 9");
          client.println("</td>");
          client.print("                <td>");
          client.print(rpm9);
          client.println("</td>");
          client.println("              </tr>");
          client.println("              <tr>");
          client.print("                <td>");
          client.print("Agitador 10");
          client.println("</td>");
          client.print("                <td>");
          client.print(rpm10);
          client.println("</td>");
          client.println("              </tr>");
          client.println("              <tr>");
          client.print("                <td>");
          client.print("Agitador 11");
          client.println("</td>");
          client.print("                <td>");
          client.print(rpm11);
          client.println("</td>");
          client.println("              </tr>");
          client.println("              <tr>");
          client.print("                <td>");
          client.print("Agitador 12");
          client.println("</td>");
          client.print("                <td>");
          client.print(rpm12);
          client.println("</td>");
          client.println("              </tr>");
          client.println("            </tbody>");
          client.println("        </table>");
          client.println("    </div>");
          client.println("");
          client.println("</body>");
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}
