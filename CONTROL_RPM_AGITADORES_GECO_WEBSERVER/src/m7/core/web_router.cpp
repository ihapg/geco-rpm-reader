#include "shared.h"
#include "../api/api_handlers.h"

// Estructura para los endpoints de la API
struct Route
{
    const char *path;
    void (*handler)(EthernetClient &, String);
};

// Array con las rutas de los endpoints
Route apiRoutes[] = {
    {"/api/sensors", handleSensors},
    {"/api/status", handleStatus},
    {"/api/logs", handleListLogs},
    {"/api/download", handleDownloadLog},
    {"/api/clear-logs", handleClearLogs},
    {"/api/rename", handleRenameLog}};

// Función para enviar el archivo solicitado
void serveFile(EthernetClient &client, const char *path, const char *mime, const char *fileName = nullptr)
{
    FILE *file = fopen(path, "r");
    if (!file)
    {
        client.println("HTTP/1.1 404 Not Found\r\n\r\n");
        return;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    rewind(file);

    client.println("HTTP/1.1 200 OK");
    client.print("Content-Type: ");
    client.println(mime);
    client.print("Content-Length: ");
    client.println(fileSize);
    client.println("Access-Control-Allow-Origin: *");
    client.println("Access-Control-Expose-Headers: Content-Disposition");

    if (fileName)
    {
        client.print("Content-Disposition: attachment; filename=\"");
        client.print(fileName);
        client.println("\"");
    }

    client.println("Connection: close");
    client.println();

    uint8_t buf[4096];
    size_t bytesRead;

    while ((bytesRead = fread(buf, 1, sizeof(buf), file)) > 0)
    {
        size_t bytesSent = 0;
        while (bytesSent < bytesRead)
        {
            int sent = client.write(buf + bytesSent, bytesRead - bytesSent);
            if (sent > 0)
                bytesSent += sent;
            else
                delay(1);
        }
    }

    fclose(file);
    client.flush();
}

// Función que gestiona la petición recibida y utiliza el método adecuado para esta
void processRequest(EthernetClient &client)
{
    String request = client.readStringUntil('\r');
    client.flush();

    int firstSpace = request.indexOf(' ');
    int secondSpace = request.indexOf(' ', firstSpace + 1);
    String path = request.substring(firstSpace + 1, secondSpace);

    // Revisa las rutas guardadas
    for (auto &route : apiRoutes)
    {
        if (path.startsWith(route.path))
        {
            route.handler(client, path);
            return;
        }
    }

    // en caso de no ser una ruta API, pasa por este otro filtro
    if (path == "/" || path == "/index.html")
    {
        serveFile(client, "/sd/index.html", "text/html");
    }
    else if (path == "/script.js")
    {
        serveFile(client, "/sd/script.js", "application/javascript");
    }
    else
    {
        client.println("HTTP/1.1 404 Not Found\r\n\r\n");
    }
}
