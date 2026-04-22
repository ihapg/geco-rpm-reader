# Plan de implementación: Servidor HTTP multi-slot non-blocking

> **Archivo a modificar:** `CONTROL_RPM_AGITADORES_GECO_WEBSERVER/src/m7/m7_web.cpp`
> **Ningún otro archivo requiere modificación.**

---

## Contexto de la conversación

### Proyecto
**GeCo Agitadores** — Portenta H7 (STM32H747, dual core CM7+CM4).
- **CM4** (`m4_acq.cpp`): adquiere RPM de 12 agitadores vía interrupciones, expone los datos por RPC.
- **CM7** (`m7_web.cpp`): servidor HTTP sobre Ethernet, hace polling RPC al CM4 cada 1 s, graba logs en SD, sirve API REST y frontend HTML/JS desde SD.

### API REST disponible
| Endpoint | Descripción |
|---|---|
| `GET /api/sensors` | JSON con RPM y Hz de los 12 sensores |
| `GET /api/status` | Estado del sistema (ok, uptime, logging activo) |
| `GET /api/logs` | Lista de archivos `.log` en SD |
| `GET /api/download?file=X` | Descarga archivo de log (409 si grabación activa) |
| `GET /api/clear-logs` | Borra logs antiguos (conserva el más reciente) |

### Clientes previstos
- 1 app PyQt6 (`Test_API/main.py`): polling a `/api/sensors` cada ~1 s, `/api/status` cada ~5 s.
- 1–4 navegadores web accediendo al frontend HTML servido desde SD.
- Total: hasta 5 clientes simultáneos.

### Problema observado
La app PyQt mostraba errores recurrentes:
```
ConnectionResetError(10054, 'Se ha forzado la interrupción de una conexión existente por el host remoto')
```
Y en sesiones largas (varios minutos) se producía desconexión espontánea de la app.

### Causa raíz (diagnosticada)
El bloque actual de gestión de clientes en `m7_loop()` tiene tres fallos:

1. **Reset de socket sin respuesta HTTP**: si la petición no llega en el buffer en 200 ms, el servidor ejecuta `client.stop()` sin haber enviado ningún byte HTTP → el cliente recibe un reset TCP, no un error HTTP limpio.
2. **Un solo cliente por iteración de loop**: el segundo cliente espera en la cola TCP del stack; si supera el timeout del cliente Python (5 s), la petición se pierde.
3. **`delay(50)` bloquea el loop**: congela NTP, RPC y logging 50 ms cada vez que hay cliente activo.

---

## Solución: gestión multi-slot non-blocking

### Principio de diseño
- Array estático de `MAX_CLIENTS = 5` slots (`EthernetClient`).
- Cada slot guarda un timestamp de cuándo fue aceptado.
- Cada iteración del loop realiza dos pasadas rápidas sin ningún `delay()`:
  1. Aceptar cliente entrante → asignar slot libre, o responder `503` si no hay.
  2. Recorrer todos los slots activos:
     - Datos disponibles → `processRequest()` → liberar slot.
     - Dentro del timeout, sin datos → esperar (próxima iteración).
     - Timeout superado o socket caído → responder `503` si sigue vivo → liberar slot.
- **Invariante clave**: nunca se llama a `client.stop()` sin haber enviado antes una respuesta HTTP válida.

---

## Implementación paso a paso

### Paso 1 — Añadir constantes y arrays estáticos

**Localización en `m7_web.cpp`:** justo después de:

```cpp
const uint32_t RPC_UPDATE_INTERVAL = 1000;
static uint32_t lastRPC = 0;
```

**Insertar estas líneas a continuación:**

```cpp
// Gestión multi-slot de clientes HTTP
static const uint8_t  MAX_CLIENTS       = 5;
static const uint32_t CLIENT_TIMEOUT_MS = 500;
static EthernetClient clients[MAX_CLIENTS];
static uint32_t       clientTimestamp[MAX_CLIENTS] = {0};
```

> **Notas:**
> - `EthernetClient` sin inicializar tiene `operator bool() == false` → slot libre de forma natural.
> - Arrays `static` → viven en BSS (memoria global), no en el stack de `m7_loop`.
> - `CLIENT_TIMEOUT_MS = 500`: margen suficiente para latencia de red local sin bloquear el loop más de 500 ms por slot (en la práctica, peticiones HTTP llegan en < 10 ms en LAN).

---

### Paso 2 — Reemplazar el bloque de gestión de clientes en `m7_loop()`

**Localizar y eliminar este bloque completo** (al final de `m7_loop()`):

```cpp
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
```

**Sustituirlo íntegramente por:**

```cpp
  // Gestión multi-slot de clientes HTTP (non-blocking)

  // 1. Aceptar cliente entrante si hay slot libre
  EthernetClient incoming = server.accept();
  if (incoming)
  {
    bool placed = false;
    for (uint8_t i = 0; i < MAX_CLIENTS; i++)
    {
      if (!clients[i])
      {
        clients[i]         = incoming;
        clientTimestamp[i] = millis();
        placed             = true;
        break;
      }
    }
    if (!placed)
    {
      // Sin slot libre: rechazar con respuesta HTTP correcta (nunca reset en seco)
      incoming.println("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n");
      incoming.flush();
      incoming.stop();
    }
  }

  // 2. Atender cada slot activo
  for (uint8_t i = 0; i < MAX_CLIENTS; i++)
  {
    if (!clients[i]) continue;

    if (clients[i].available())
    {
      // Petición lista: procesar y cerrar limpiamente
      processRequest(clients[i]);
      clients[i].flush();
      clients[i].stop();
    }
    else if (!clients[i].connected() || (millis() - clientTimestamp[i]) > CLIENT_TIMEOUT_MS)
    {
      // Timeout o desconexión inesperada: responder si el socket sigue vivo
      if (clients[i].connected())
      {
        clients[i].println("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n");
        clients[i].flush();
      }
      clients[i].stop();
    }
    // else: slot activo dentro del timeout → esperar siguiente iteración
  }
```

---

## Estado final de `m7_loop()` completo (referencia)

```cpp
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
      loggingSession.writeData(sensorsM7, lastLoggedTime);
    }
  }

  // Sincronización de tiempo
  if (timeClient.update())
  {
    syncDSTOffset = getDSTOffset(timeClient.getEpochTime());
    Serial.println("[NTP] Fecha y hora sincronizadas con éxito.");
  }

  // Actualizar sesión de logging
  loggingSession.update(sensorsM7, sd_check, now);

  // Gestión multi-slot de clientes HTTP (non-blocking)

  // 1. Aceptar cliente entrante si hay slot libre
  EthernetClient incoming = server.accept();
  if (incoming)
  {
    bool placed = false;
    for (uint8_t i = 0; i < MAX_CLIENTS; i++)
    {
      if (!clients[i])
      {
        clients[i]         = incoming;
        clientTimestamp[i] = millis();
        placed             = true;
        break;
      }
    }
    if (!placed)
    {
      incoming.println("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n");
      incoming.flush();
      incoming.stop();
    }
  }

  // 2. Atender cada slot activo
  for (uint8_t i = 0; i < MAX_CLIENTS; i++)
  {
    if (!clients[i]) continue;

    if (clients[i].available())
    {
      processRequest(clients[i]);
      clients[i].flush();
      clients[i].stop();
    }
    else if (!clients[i].connected() || (millis() - clientTimestamp[i]) > CLIENT_TIMEOUT_MS)
    {
      if (clients[i].connected())
      {
        clients[i].println("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n");
        clients[i].flush();
      }
      clients[i].stop();
    }
  }
}
```

---

## Archivos afectados

| Archivo | Cambio |
|---|---|
| `src/m7/m7_web.cpp` | ✅ Añadir 4 líneas de variables estáticas + reemplazar bloque de clientes en `m7_loop()` |
| `src/m7/web_router.cpp` | ❌ Sin cambios — `processRequest(EthernetClient&)` ya es compatible |
| `src/m7/api_handlers.cpp` | ❌ Sin cambios — todos los `handleXxx(EthernetClient&, ...)` son compatibles |
| `src/m7/m7_web.h` | ❌ Sin cambios — no hay nuevas funciones exportadas |
| `src/m7/web_router.h` | ❌ Sin cambios |
| `src/m7/api_handlers.h` | ❌ Sin cambios |
| `src/main.cpp` | ❌ Sin cambios |
| `src/m4/*` | ❌ Sin cambios |

---

## Verificación tras flashear

1. **Monitor serie PlatformIO**: debe aparecer `[HTTP] Servidor arrancado.` sin errores en setup.
2. **Un cliente** (app PyQt o navegador): JSON correcto, sin `ConnectionResetError` en consola.
3. **Polling simultáneo** (app PyQt a 1 Hz + navegador con frontend abierto): sin resets, sin 503 innecesarios en uso normal.
4. **Saturación intencional** (abrir 6+ pestañas recargando rápido): las peticiones extra deben recibir `HTTP 503`, nunca un reset de socket.
5. **Estabilidad larga** (10+ minutos con app PyQt conectada): sin desconexión espontánea.

---

## Notas técnicas adicionales

- **Sockets disponibles**: el stack lwIP/Mbed OS de Portenta H7 soporta 8–16 sockets TCP simultáneos. Con `MAX_CLIENTS = 5` más el socket del servidor = 6 totales. Valor seguro.
- **Descargas largas**: durante `handleDownloadLog()`, `serveFile()` ocupa el slot hasta completar la transferencia. El `delay(1)` dentro de `serveFile()` (rama `sent <= 0`) solo se activa si el buffer TCP está lleno; es esperable y no afecta a los demás slots.
- **Reducción de buffer si fuera necesario**: si en pruebas se observa contención excesiva durante descargas, reducir el buffer de `serveFile` de 4096 a 1024 bytes es suficiente, sin cambios estructurales adicionales.
- **`processRequest()` es thread-safe** en este contexto: se llama de forma secuencial desde el loop del CM7, nunca concurrentemente, por lo que no hay riesgo de acceso simultáneo a `sensorsM7` o `loggingSession`.
