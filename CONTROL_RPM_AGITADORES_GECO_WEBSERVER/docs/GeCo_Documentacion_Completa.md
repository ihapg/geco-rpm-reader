# GeCo RPM Reader — Documentación Técnica Completa

**Firmware · API HTTP · Cliente de referencia (Test_API)**
Versión: 1.0 — Abril 2026

---

## Índice

1. [Visión general](#1-visión-general)
2. [Estructura del proyecto](#2-estructura-del-proyecto)
3. [Flujo de datos del firmware](#3-flujo-de-datos-del-firmware)
4. [API HTTP — Referencia completa](#4-api-http--referencia-completa)
5. [Router y handlers del firmware](#5-router-y-handlers-del-firmware)
6. [Subsistema de logging en SD](#6-subsistema-de-logging-en-sd)
7. [Reloj y sincronización NTP](#7-reloj-y-sincronización-ntp)
8. [Test_API — Cliente de referencia y plantilla](#8-test_api--cliente-de-referencia-y-plantilla)
9. [Guía de reutilización de api_client.py](#9-guía-de-reutilización-de-api_clientpy)
10. [Reutilizar esta arquitectura en otros proyectos](#10-reutilizar-esta-arquitectura-en-otros-proyectos)
11. [Mantenimiento y archivos clave](#11-mantenimiento-y-archivos-clave)

---

## 1. Visión general

GeCo RPM Reader es un sistema embebido que monitoriza agitadores industriales, exponiendo sus lecturas y estado a través de una API HTTP REST. El firmware corre en una Arduino Portenta H7 (doble núcleo ARM Cortex-M7 + M4) y divide responsabilidades entre los dos núcleos para garantizar muestreo estable en tiempo real y servicios de red sin interferencias.

Este documento recoge la documentación técnica completa del sistema: arquitectura del firmware, flujo de datos, referencia de la API HTTP, y una guía de uso del cliente de referencia (Test_API) pensada como plantilla para integraciones en otros programas.

| Componente | Responsabilidad |
|---|---|
| Núcleo M4 | Adquisición de señales de sensores en tiempo real |
| Núcleo M7 | Red, API HTTP, logging en SD y sincronización NTP |
| API HTTP | Interfaz REST para consulta de datos y gestión de logs |
| Test_API | Cliente de referencia y plantilla reutilizable (PyQt6) |

> **Nota de diseño:** La separación M4/M7 evita mezclar tareas críticas de muestreo con tareas de red y almacenamiento, garantizando integridad en la adquisición de datos.

---

## 2. Estructura del proyecto

### 2.1 Firmware

```
CONTROL_RPM_AGITADORES_GECO_WEBSERVER/
├── platformio.ini
├── include/
│   └── shared.h
├── src/
│   ├── main.cpp
│   ├── m4/
│   │   ├── m4_acq.cpp
│   │   └── m4_acq.h
│   └── m7/
│       ├── m7_web.cpp / m7_web.h
│       ├── api/
│       │   ├── api_handlers.cpp
│       │   └── api_handlers.h
│       ├── core/
│       │   ├── web_router.cpp
│       │   └── web_router.h
│       ├── logging/
│       │   ├── logging_session.cpp / .h
│       │   └── sensor_log_writer.cpp / .h
│       └── time/
│           ├── time_service.cpp
│           └── time_service.h
└── docs/
    ├── firmware_guide.md
    └── firmware_operational_manual.md
```

### 2.2 Cliente de referencia (Test_API)

```
Test_API/
├── api_client.py          ← cliente HTTP puro (exportable)
├── main.py                ← aplicación PyQt6 de referencia
├── interfaz.ui            ← ventana principal (Qt Designer)
├── log_dialog.ui          ← diálogo de gestión de logs
├── ips.json               ← IPs guardadas automáticamente
├── requirements.txt
└── docs/
    └── app_guide.md
```

---

## 3. Flujo de datos del firmware

### 3.1 Adquisición en M4

El núcleo M4 toma lecturas continuas de los agitadores y mantiene los datos preparados para ser consultados por RPC desde M7. Al estar aislado de la red, el muestreo no se ve afectado por latencias de HTTP ni operaciones de SD.

### 3.2 Sincronización M7 ← M4

M7 consulta periódicamente los datos del M4 por RPC y actualiza su estructura interna de sensores. El resultado de esta arquitectura es:

- La API HTTP devuelve siempre datos desde M7, con coherencia garantizada.
- M4 queda completamente aislado de conexiones de red y operaciones de almacenamiento.

### 3.3 Publicación y logging en M7

Con los datos actualizados de sensores, M7 atiende de forma concurrente:

1. Peticiones HTTP de clientes.
2. Mantenimiento del estado de grabación activa.
3. Escritura periódica de logs en tarjeta SD con timestamp.
4. Sincronización de hora via NTP.

Durante pérdida de red Ethernet, el sistema mantiene continuidad temporal de la grabación usando una base de época válida y el reloj interno de milisegundos. Al recuperarse la red, la resincronización NTP se realiza fuera de la sesión activa de logging para evitar saltos temporales en las líneas del fichero.

### 3.4 Gestión multicliente HTTP

El firmware implementa una estrategia multi-slot no bloqueante para atender varios clientes simultáneamente sin interrumpir el loop principal. Esto aporta:

- Evita resets de conexión por cierres abruptos entre clientes.
- Reduce timeouts en situaciones de acceso concurrente.
- Mantiene responsive el ciclo de RPC, NTP y logging mientras se atienden peticiones.

---

## 4. API HTTP — Referencia completa

Todos los endpoints usan método GET por simplicidad del router actual. Las respuestas se devuelven en JSON cuando aplica. Durante grabación activa, las operaciones sensibles devuelven HTTP 409.

| Endpoint | Método | Descripción / Respuesta típica |
|---|---|---|
| `/api/sensors` | GET | `[{"rpm": float, "hz": float}, ...]` |
| `/api/status` | GET | `{"status": str, "logging": bool, "last_log": str}` |
| `/api/logs` | GET | `["log_2026-04-20.log", ...]` |
| `/api/download?file=<name>` | GET | Stream binario del log. HTTP 409 si grabación activa. |
| `/api/clear-logs` | GET | `{"deleted": int}` — conserva el log más reciente |
| `/api/rename?from=<old>&to=<new>` | GET | `{"status":"success","from":"...","to":"..."}` |

### 4.1 Endpoint /api/sensors

Devuelve las lecturas actuales de todos los agitadores. El array tiene tantas posiciones como sensores configurados en el firmware. Cada elemento contiene:

- `rpm`: velocidad en revoluciones por minuto.
- `hz`: frecuencia de la señal en herzios.

### 4.2 Endpoint /api/status

Devuelve el estado general del dispositivo.

| Campo | Descripción |
|---|---|
| `status` | Estado operativo del dispositivo (p.ej. `"ok"`) |
| `logging` | `true` si hay una sesión de grabación activa |
| `last_log` | Nombre del último log cerrado (útil tras reinicio) |

> Al arrancar M7, se recupera el log más reciente en SD para que `last_log` sea informativo incluso antes de iniciar una nueva sesión de grabación.

### 4.3 Endpoint /api/rename

Permite renombrar un fichero de log en la SD. El firmware aplica las siguientes validaciones en orden antes de ejecutar el rename:

1. Sin grabación activa (devuelve 409 si `logging = true`).
2. Parámetros `from` y `to` presentes y no vacíos.
3. Rechazo de patrones peligrosos en el nombre: `..`, `/`, `\`.
4. El archivo de origen debe existir.
5. El archivo de destino no debe existir ya.
6. Ejecución del rename real en SD.

Códigos de error posibles:

| HTTP | Código JSON | Causa |
|---|---|---|
| 409 | `recording` | Grabación activa |
| 400 | `invalid_name` | Patrón de nombre peligroso o vacío |
| 404 | `not_found` | El archivo de origen no existe |
| 409 | `already_exists` | El archivo destino ya existe |
| 500 | `rename_failed` | Error interno al ejecutar el rename en SD |

---

## 5. Router y handlers del firmware

### 5.1 Router (`web_router.cpp`)

El router mapea rutas a funciones handler. Sus responsabilidades son:

- Encaminar la petición al handler correspondiente.
- Resolver recursos estáticos si aplica.
- Devolver HTTP 404 cuando no hay coincidencia de ruta.

### 5.2 Handlers de API (`api_handlers.cpp`)

Cada handler implementa el patrón de tres fases:

1. Validación de entrada.
2. Lógica de negocio.
3. Respuesta HTTP/JSON.

> **Patrón recomendado:** rechazo temprano de casos inválidos, mensajes de error concretos, y operaciones de filesystem solo después de validar todos los parámetros.

### 5.3 Cómo añadir un endpoint nuevo

1. Declarar el handler en `api_handlers.h`.
2. Implementarlo en `api_handlers.cpp`.
3. Registrar la ruta en `web_router.cpp`.
4. Validar todos los parámetros de entrada.
5. Responder siempre con HTTP y JSON coherente.
6. Probar el caso de éxito y todos los casos de error.

Checklist mínimo de pruebas para cada nuevo endpoint:

- Caso válido con parámetros correctos.
- Parámetros faltantes o en blanco.
- Parámetros maliciosos o con patrones peligrosos.
- Recursos no existentes.
- Conflictos de estado (grabación activa).

---

## 6. Subsistema de logging en SD

El subsistema de logging gestiona el ciclo completo de grabación de datos de sensores a ficheros en tarjeta SD.

| Fase | Descripción |
|---|---|
| Inicio | Detección de actividad en sensores e inicio automático de sesión |
| Escritura | Escritura periódica de lecturas con timestamp NTP |
| Cierre | Cierre controlado del fichero tras parada de actividad |
| Recuperación | Al arrancar M7, recupera el último log cerrado para `last_log` |

### 6.1 Continuidad temporal durante desconexión de red

Cuando el enlace Ethernet cae durante una sesión activa, el logging no se detiene ni congela timestamps. El firmware conserva una referencia temporal de la última hora válida y continúa incrementando el tiempo con el reloj interno.

Este comportamiento permite que:

- La secuencia temporal del fichero siga siendo monotónica durante la desconexión.
- No se introduzcan huecos por bloqueo de sincronización NTP en mitad de la grabación.
- La sesión se cierre de forma normal en SD aunque la red no esté disponible.

### 6.2 Archivos clave del subsistema

- `logging_session.cpp/h`: gestión del ciclo de sesión.
- `sensor_log_writer.cpp/h`: escritura de registros con formato y timestamp.

---

## 7. Reloj y sincronización NTP

La gestión del tiempo garantiza timestamps coherentes en todos los logs y trazabilidad operativa. Buenas prácticas aplicadas en el firmware:

- Sincronización periódica con servidor NTP (no solo en arranque).
- Separación del offset de horario (DST) respecto a la hora base UTC.
- Evitar depender de una única actualización puntual.

El firmware distingue dos contextos de sincronización:

1. Arranque: sincronización inicial para establecer una referencia horaria válida.
2. Runtime: resincronización no bloqueante en recuperación de red, con reintentos espaciados y controlados por estado.

Durante una grabación activa, los intentos de resincronización NTP se difieren hasta que la sesión termine. De esta forma, el ajuste de hora no interrumpe el ciclo de escritura ni introduce discontinuidades en la traza temporal del log.

El tiempo se usa para:

- Generación de timestamps en ficheros de log.
- Nombrado de ficheros de sesión con fecha.
- Estado operativo y trazabilidad ante incidencias.

---

## 8. Test_API — Cliente de referencia y plantilla

> Test_API es una aplicación de escritorio en PyQt6 diseñada como ejemplo práctico para consumir la API de GeCo. El componente más reutilizable es `api_client.py`: un cliente HTTP desacoplado que puede integrarse en cualquier programa sin arrastrar dependencias de interfaz gráfica.

### 8.1 Arquitectura en dos capas

La aplicación separa de forma clara transporte HTTP y presentación visual:

| Capa | Responsabilidades |
|---|---|
| `api_client.py` (transporte) | Construir URLs, ejecutar peticiones HTTP con `requests.Session`, estandarizar timeouts y errores, exponer métodos claros. |
| `main.py` (presentación) | Dibujar estado en UI, coordinar workers/hilos, interpretar reglas de negocio visuales. |

> **Regla de oro:** la UI no implementa lógica HTTP de bajo nivel. La capa de transporte no depende de PyQt ni de ningún framework de UI.

### 8.2 Flujo operativo de la aplicación

#### Conexión inicial

1. El usuario introduce IP y puerto (por defecto `80`).
2. Se realiza una llamada de prueba a `/api/status`.
3. Si responde, se marca la conexión como activa, se guarda la IP en `ips.json` y se inicia el polling en segundo plano.

#### Polling en segundo plano (PollingWorker)

Un hilo dedicado consulta la API periódicamente sin bloquear la interfaz:

| Endpoint | Frecuencia aprox. | Propósito |
|---|---|---|
| `/api/sensors` | ~1 segundo | Lecturas RPM/Hz de agitadores |
| `/api/status` | ~5 segundos | Estado general y estado de grabación |

El worker tolera fallos transitorios de red. Si los fallos en `/api/status` se vuelven sostenidos (superan el umbral configurable), se fuerza desconexión controlada.

#### Gestión de logs (LogDialog)

- Listar logs disponibles en el dispositivo.
- Descargar un log seleccionado con barra de progreso en streaming.
- Limpiar logs antiguos conservando el más reciente.

Las descargas se ejecutan en un `DownloadWorker` independiente para no congelar la interfaz. Si el dispositivo devuelve HTTP 409, se muestra un aviso de grabación activa.

#### Renombrado automático al cerrar grabación

La app detecta la transición `logging: true → false` en `/api/status` y desencadena el siguiente flujo:

1. Lee el campo `last_log` de `/api/status`.
2. Abre el modal `RenameLogDialog` con el nombre actual.
3. El usuario puede mantener el nombre, renombrar, y opcionalmente descargar.
4. Si se confirma el nuevo nombre, llama a `/api/rename`.
5. Si se activa descarga automática, descarga con el nombre final.

> **Importante:** Cancelar en el modal NO cancela la grabación (ya ha finalizado). Solo omite el renombrado y la descarga, conservando el nombre original del archivo en SD.

### 8.3 Constantes configurables en `main.py`

| Constante | Valor por defecto | Descripción |
|---|---|---|
| `_SENSOR_COUNT` | `12` | Filas de sensores en la tabla de la UI |
| `_STATUS_POLL_EVERY` | `5` | Ciclos de sensores entre lecturas de status |
| `_POLL_SLEEP_MS` | `100` | Milisegundos de sleep por iteración interna del poller |
| `_POLL_SLEEP_ITERS` | `10` | Iteraciones por ciclo (ciclo total ~1 segundo) |
| `_SENSORS_ERR_THRESHOLD` | `5` | Fallos en `/api/sensors` antes de notificar |
| `_STATUS_ERR_THRESHOLD` | `3` | Fallos en `/api/status` antes de desconectar |
| `_LOG_NAME_RE` | `^[\w\-]{1,60}\.log$` | Validación de nombres en renombrado (cliente) |

---

## 9. Guía de reutilización de `api_client.py`

`api_client.py` no tiene ninguna dependencia de PyQt. Puede copiarse directamente al proyecto destino instalando únicamente `requests`:

```bash
pip install requests
```

### 9.1 Caso A: Script de consola

El caso más sencillo: uso secuencial y directo del cliente.

```python
from api_client import ApiClient, RecordingActiveError

client = ApiClient("192.168.1.254", "80")

# Consultar estado
status = client.get_status()    # {"status": "ok", "logging": False, "last_log": "..."}

# Leer sensores
sensors = client.get_sensors()  # [{"rpm": 120.5, "hz": 2.0}, ...]

# Listar logs disponibles
logs = client.get_logs()        # ["log_2026-04-20.log", ...]

# Renombrar un log
if logs:
    client.rename_log(logs[0], "mi_sesion.log")

# Descargar con barra de progreso
def on_progress(pct: int):
    print(f"\r{pct}%", end="")

client.download_log("mi_sesion.log", "/local/mi_sesion.log", on_progress)

# Eliminar logs antiguos
result = client.clear_logs()    # {"deleted": 2}

# Liberar recursos de red
client.close()
```

### 9.2 Caso B: Polling sin UI

```python
import time
from api_client import ApiClient

client = ApiClient("192.168.1.254", "80")

while True:
    sensors = client.get_sensors()
    for i, s in enumerate(sensors):
        print(f"AG{i+1}: {s['rpm']:.1f} RPM  /  {s['hz']:.2f} Hz")
    time.sleep(1)
```

### 9.3 Caso C: Integración con PyQt (patrón recomendado)

La regla clave: `ApiClient` se llama siempre desde un `QThread`, nunca desde el hilo principal, para no bloquear la UI.

```python
class MiWorker(QtCore.QThread):
    datos_listos = QtCore.pyqtSignal(list)

    def __init__(self, api: ApiClient):
        super().__init__()
        self.api = api

    def run(self):
        # Llamada bloqueante: segura en hilo separado
        datos = self.api.get_sensors()
        # Nunca tocar la UI directamente desde el hilo
        self.datos_listos.emit(datos)
```

### 9.4 Caso D: Servicio backend

- Envuelve `ApiClient` en un servicio interno de tu aplicación.
- Añade reintentos y lógica de circuit-breaker según tus necesidades.
- Expón datos limpios y normalizados al resto de tu sistema.

### 9.5 Gestión de errores

```python
import requests
from api_client import ApiClient, RecordingActiveError

client = ApiClient("192.168.1.254", "80")

try:
    client.download_log("log.log", "local.log")
except RecordingActiveError:
    print("El dispositivo está grabando. Inténtalo al finalizar la sesión.")
except requests.Timeout:
    print("El dispositivo no responde en el tiempo esperado.")
except requests.HTTPError as e:
    print(f"Error HTTP: {e.response.status_code}")
```

---

## 10. Reutilizar esta arquitectura en otros proyectos

Este diseño es útil en cualquier proyecto que necesite:

- Muestreo estable en tiempo real en hardware embebido.
- API HTTP con soporte para varios clientes concurrentes.
- Persistencia local en almacenamiento (SD, EEPROM, flash).
- Cliente de escritorio o script que consuma la API sin acoplarse a la UI.

### 10.1 Patrón reutilizable del firmware

| # | Principio |
|---|---|
| 1 | Núcleo de adquisición aislado: solo muestreo, sin red ni almacenamiento. |
| 2 | Núcleo de servicios desacoplado: red, logging y NTP sin interferir en muestreo. |
| 3 | Comunicación interna simple entre núcleos: RPC o cola de datos. |
| 4 | Endpoints pequeños y validados: cada handler valida, opera y responde. |
| 5 | Logging robusto orientado a diagnóstico con timestamps coherentes. |

### 10.2 Patrón reutilizable del cliente

| # | Buena práctica |
|---|---|
| 1 | Separación clara de capas: cliente HTTP vs. UI (o lógica de negocio). |
| 2 | Sesión HTTP persistente para reducir latencia entre peticiones. |
| 3 | Polling robusto con umbrales de error configurables. |
| 4 | Operaciones de red largas (descarga, etc.) fuera del hilo principal. |
| 5 | Validación defensiva en cliente Y en firmware para operaciones de archivos. |
| 6 | Mensajes de error interpretables tanto para el usuario como para depuración. |

---

## 11. Mantenimiento y archivos clave

### 11.1 Recomendaciones de mantenimiento del firmware

1. Mantener consistencia en las respuestas JSON de error entre todos los endpoints.
2. Extraer helper común para parseo de query string cuando crezca el número de endpoints.
3. Añadir pruebas de integración para operaciones de archivos (list, clear, rename, download).
4. Documentar cualquier cambio de API en el directorio `docs/` del repositorio.

### 11.2 Mejoras sugeridas para el cliente

1. Unificar helper para respuestas JSON de error en el firmware (reduce duplicidades).
2. Mostrar en la UI mensajes de error de rename más específicos por código JSON.
3. Añadir tests automatizados del cliente (`pytest` + mocks HTTP).
4. Valorar cambiar el botón "Cancelar" del modal por "Omitir" para mayor claridad de UX.

### 11.3 Archivos clave para empezar a leer

Con estos cinco archivos se comprende casi todo el comportamiento de extremo a extremo del firmware:

- `src/m7/m7_web.cpp` — punto de entrada del núcleo M7.
- `src/m7/core/web_router.cpp` — enrutamiento HTTP.
- `src/m7/api/api_handlers.cpp` — lógica de cada endpoint.
- `src/m7/logging/logging_session.cpp` — ciclo de grabación.
- `src/m4/m4_acq.cpp` — adquisición de sensores en M4.

Para el cliente de referencia:

- `api_client.py` — cliente HTTP exportable.
- `main.py` — ejemplo de integración con PyQt6.

---

*GeCo RPM Reader — Documentación Técnica Completa — Abril 2026*
