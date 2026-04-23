# Test_API — Plantilla de cliente para la API GeCo

Aplicación de escritorio (PyQt6) que sirve como **referencia y plantilla** para comunicarse con la API REST del dispositivo GeCo Agitadores.  
El objetivo principal no es la interfaz en sí, sino demostrar cómo usar `api_client.py` de forma aislada y luego integrarlo en cualquier proyecto.

---

## Estructura del proyecto

```
Test_API/
├── api_client.py    ← cliente HTTP puro, sin dependencias de UI  ← EXPORTABLE
├── main.py          ← aplicación PyQt6 que consume api_client
├── interfaz.ui      ← layout de la ventana principal (Qt Designer)
├── log_dialog.ui    ← layout del diálogo de logs (Qt Designer)
├── ips.json         ← IPs guardadas (generado automáticamente)
└── requirements.txt
```

---

## Requisitos

- Python >= 3.10
- Dependencias: `pip install -r requirements.txt`

---

## Ejecutar la aplicación

```bash
python main.py
```

---

## Cómo funciona la aplicación

### Conexión
Se introduce la IP del dispositivo y el puerto (por defecto `80`) y se pulsa **Conectar**.  
La aplicación hace una primera llamada a `/api/status` para verificar que el dispositivo responde antes de iniciar el polling.  
Las IPs que conectan con éxito se guardan automáticamente en `ips.json`.

### Polling en tiempo real
Una vez conectado, dos endpoints se interrogan en un hilo de fondo (`PollingWorker`):

| Endpoint | Cadencia | Propósito |
|---|---|---|
| `/api/sensors` | ~1 s | Lecturas de RPM y Hz de los agitadores |
| `/api/status` | ~5 s | Estado del dispositivo y estado de grabación |

El hilo tolera fallos de red transitorios (configurable con `_SENSORS_ERR_THRESHOLD` y `_STATUS_ERR_THRESHOLD` en `main.py`) y solo reporta errores sostenidos.  
Si `/api/status` falla de forma sostenida, la aplicación se desconecta automáticamente.

### Gestor de logs
El botón **Gestor de logs** abre un diálogo que permite:
- Listar los ficheros de log disponibles en el dispositivo (`/api/logs`)
- Descargar un log con barra de progreso en streaming (`/api/download?file=<nombre>`)
- Eliminar logs antiguos conservando el más reciente (`/api/clear-logs`)

La descarga se hace en un hilo separado (`DownloadWorker`) para no bloquear la UI.  
Si el dispositivo está grabando en el momento de la descarga, devuelve HTTP 409 y se lanza `RecordingActiveError` — la aplicación lo muestra como aviso.

---

## API del dispositivo — Referencia rápida

| Endpoint | Método | Respuesta |
|---|---|---|
| `/api/sensors` | GET | `[{"rpm": float, "hz": float}, ...]` |
| `/api/status` | GET | `{"status": str, "logging": bool}` |
| `/api/logs` | GET | `["log_2026-04-20.log", ...]` |
| `/api/download?file=<nombre>` | GET | binario en streaming; 409 si grabación activa |
| `/api/clear-logs` | GET | `{"deleted": int}` |

---

## Exportar `api_client.py` a otro proyecto

`api_client.py` no tiene ninguna dependencia de PyQt.  
Basta con copiar el fichero al proyecto destino e instalar `requests`:

```bash
pip install requests
```

### Uso básico

```python
from api_client import ApiClient, RecordingActiveError

client = ApiClient("192.168.1.254", "80")

# Leer sensores
sensors = client.get_sensors()          # [{"rpm": 120.5, "hz": 2.0}, ...]

# Leer estado del dispositivo
status = client.get_status()            # {"status": "ok", "logging": False}

# Listar logs disponibles
logs = client.get_logs()                # ["log_2026-04-20.log", ...]

# Descargar un log (con callback de progreso opcional)
def on_progress(pct: int):
    print(f"\r{pct}%", end="")

client.download_log("log_2026-04-20.log", "/ruta/local/log.log", on_progress)

# Eliminar logs antiguos
result = client.clear_logs()            # {"deleted": 2}

# Liberar recursos de red al terminar
client.close()
```

### Uso en polling (sin PyQt)

```python
import time
from api_client import ApiClient

client = ApiClient("192.168.1.254", "80")

while True:
    sensors = client.get_sensors()
    for i, s in enumerate(sensors):
        print(f"AG{i+1}: {s['rpm']:.1f} RPM")
    time.sleep(1)
```

### Gestión de errores

```python
import requests
from api_client import ApiClient, RecordingActiveError

client = ApiClient("192.168.1.254", "80")

try:
    client.download_log("log.log", "local.log")
except RecordingActiveError:
    print("El dispositivo está grabando, inténtalo más tarde.")
except requests.Timeout:
    print("El dispositivo no responde.")
except requests.HTTPError as e:
    print(f"Error HTTP: {e.response.status_code}")
```

### Integración con PyQt (patrón recomendado)

El patrón usado en `main.py` es el recomendado para integrar `ApiClient` en una GUI PyQt:

```python
class MiWorker(QtCore.QThread):
    datos_listos = QtCore.pyqtSignal(list)

    def __init__(self, api: ApiClient):
        super().__init__()
        self.api = api

    def run(self):
        datos = self.api.get_sensors()   # bloqueante — seguro en hilo separado
        self.datos_listos.emit(datos)    # emitir siempre desde el hilo, nunca tocar la UI directamente
```

La regla clave: **`ApiClient` se llama siempre desde un `QThread`, nunca desde el hilo principal**, para no bloquear la UI.

---

## Ajustes de comportamiento (`main.py`)

Las constantes al inicio de `main.py` permiten adaptar el comportamiento sin tocar la lógica:

| Constante | Valor por defecto | Descripción |
|---|---|---|
| `_SENSOR_COUNT` | `12` | Número de filas en la tabla de sensores |
| `_STATUS_POLL_EVERY` | `5` | Ciclos de sensores entre cada consulta de status |
| `_POLL_SLEEP_MS` | `100` | ms de sleep por iteración interna del poller |
| `_POLL_SLEEP_ITERS` | `10` | Iteraciones por ciclo (ciclo ≈ 1 s total) |
| `_SENSORS_ERR_THRESHOLD` | `5` | Fallos consecutivos en `/api/sensors` antes de reportar |
| `_STATUS_ERR_THRESHOLD` | `3` | Fallos consecutivos en `/api/status` antes de desconectar |
