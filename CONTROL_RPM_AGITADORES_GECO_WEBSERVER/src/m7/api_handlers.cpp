#include "shared.h"
#include "api_handlers.h"

extern SensorData sensorsM7; // viene del main_m7.cpp actualizado por RPC

// Función para generar archivo JSON respuesta
void sendJsonResponse(EthernetClient& client, const JsonDocument& doc)
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Access-Control-Allow-Origin: *");
    client.println("Connection: close");
    client.println();

    serializeJson(doc, client);
}

// Función que recopila datos de los sensores y los envía en archivo JSON
void handleSensors(EthernetClient& client)
{
    JsonDocument doc;

    for (auto &sensor : sensorsM7.sensors)
    {
        JsonObject obj = doc.add<JsonObject>();
        obj["id"] = sensor.id;
        obj["rpm"] = sensor.rpm;
        obj["hz"] = sensor.hz;
    }

    sendJsonResponse(client, doc);    
}

// Función que recopila el estado del programa y lo envía en archivo JSON
void handleStatus(EthernetClient& client)
{
    JsonDocument doc;
    doc["status"] = "ok";
    doc["uptime"] = millis() / 1000;

    sendJsonResponse(client, doc);
}
