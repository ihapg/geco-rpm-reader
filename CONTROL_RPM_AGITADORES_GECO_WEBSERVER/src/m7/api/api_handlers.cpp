#include "shared.h"
#include "api_handlers.h"
#include "../core/web_router.h"
#include "../m7_web.h"

extern SensorData sensorsM7; // viene del m7_web.cpp actualizado por RPC

std::vector<String> nameLogs;

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
void handleSensors(EthernetClient& client, String path)
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
void handleStatus(EthernetClient& client, String path)
{
    JsonDocument doc;
    doc["status"] = "ok";
    doc["uptime"] = millis() / 1000;
    doc["logging"] = getLoggingSession().isActive();

    sendJsonResponse(client, doc);
}

// Función para listar los archivos log guardados en la SD
void handleListLogs(EthernetClient& client, String path)
{
    // Recopilación de logs
    // std::vector<String> nameLogs;
    nameLogs.clear();

    DIR *dir = opendir("/sd");
    struct dirent *ent;
    String activeLogFile = getLoggingSession().getActiveFileName();

    if (dir != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            String name = ent->d_name;
            if (name.endsWith(".log") && !(getLoggingSession().isActive() && name == activeLogFile))
            {
                nameLogs.push_back(name);
            }
        }
        closedir(dir);
    }

    // Ordenar lista de logs por nombre descendente (más recientes primero)
    std::sort(nameLogs.begin(), nameLogs.end(), [](const String& a, const String&b) {
        return a > b;
    });

    // Generación de respuesta JSON y escritura en CSV
    JsonDocument doc;
    JsonArray files = doc.to<JsonArray>();
    for (auto &name : nameLogs)
    {
        files.add(name);
    }    

    sendJsonResponse(client, doc);
}

// Función para descargar archivo log en el cliente
void handleDownloadLog(EthernetClient& client, String path)
{
    // Control de descarga mientras se está grabando un log
    if (getLoggingSession().isActive())
    {
        client.println("HTTP/1.1 409 Conflict\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\n\r\n{\"error\":\"recording\"}");
        return;
    }

    int index = path.indexOf("file=");
    if (index == -1)
    {

        client.println("HTTP/1.1 400 Bad Request\r\n\r\n");
        return;
    }
    String fileName = path.substring(index + 5);
    String fullPath = "/sd/" + fileName;

    serveFile(client, fullPath.c_str(), "text/csv", fileName.c_str());
}

// Función para borrar los archivos log de la SD
void handleClearLogs(EthernetClient& client, String path)
{
    int deletedCount = 0;
    DIR *dir = opendir("/sd");
    struct dirent *ent;

    // === TEST ===
    // Recorrer directorio y eliminar en base a nameLogs (menos el primero/más reciente)
    if (dir != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            String name = ent->d_name;
            if (name.endsWith(".log") && !name.equals(nameLogs.at(0).c_str()))
            {
                String fullPath = "/sd/" + name;
                if (remove(fullPath.c_str()) == 0) deletedCount++;
            }
            
        }
        closedir(dir);
    }

    JsonDocument doc;
    doc["status"] = "success";
    doc["deleted"] = deletedCount;
    sendJsonResponse(client, doc);
}
