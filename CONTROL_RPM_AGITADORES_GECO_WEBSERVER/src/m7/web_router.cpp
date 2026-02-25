#include "shared.h"
#include "api_handlers.h"

// Estructura para los endpoints de la API
struct Route
{
    const char *path;
    void (*handler)(EthernetClient &);
};

// Array con las rutas de los endpoints
Route apiRoutes[] = {
    {"/api/sensors", handleSensors},
    {"/api/status", handleStatus}};

// Función para enviar el archivo solicitado
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
        if (path == route.path)
        {
            route.handler(client);
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
