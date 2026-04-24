# Test_API — GeCo API Client Template

Desktop application (PyQt6) used as a **reference and template** to interact with the GeCo Agitators REST API.  
The main goal is not the interface itself, but to show how to use `api_client.py` in isolation and then integrate it into any project.

---

## Project structure

```
Test_API/
├── api_client.py    <- pure HTTP client, no UI dependencies  <- EXPORTABLE
├── main.py          <- PyQt6 application that uses api_client
├── interfaz.ui      <- main window layout (Qt Designer)
├── log_dialog.ui    <- log dialog layout (Qt Designer)
├── ips.json         <- saved IPs (generated automatically)
└── requirements.txt
```

---

## Requirements

- Python >= 3.10
- Dependencies: `pip install -r requirements.txt`

---

## Run the application

```bash
python main.py
```

---

## How the application works

### Connection
Enter the device IP and port (default `80`) and click **Connect**.  
The app first calls `/api/status` to verify that the device is reachable before starting polling.  
IPs that connect successfully are automatically saved in `ips.json`.

### Real-time polling
Once connected, two endpoints are queried from a background thread (`PollingWorker`):

| Endpoint | Frequency | Purpose |
|---|---|---|
| `/api/sensors` | ~1 s | RPM and Hz readings for agitators |
| `/api/status` | ~5 s | Device state and recording state |

The worker tolerates transient network errors (configurable with `_SENSORS_ERR_THRESHOLD` and `_STATUS_ERR_THRESHOLD` in `main.py`) and only reports sustained failures.  
If `/api/status` keeps failing, the app disconnects automatically.

### Log manager
The **Log Manager** button opens a dialog that allows you to:
- List available log files on the device (`/api/logs`)
- Download a log with streaming progress (`/api/download?file=<name>`)
- Delete old logs while keeping the newest one (`/api/clear-logs`)

Downloads run in a separate thread (`DownloadWorker`) so the UI stays responsive.  
If the device is actively recording during download, it returns HTTP 409 and raises `RecordingActiveError` — the app shows this as a warning.

---

## Device API — Quick reference

| Endpoint | Method | Response |
|---|---|---|
| `/api/sensors` | GET | `[{"rpm": float, "hz": float}, ...]` |
| `/api/status` | GET | `{"status": str, "logging": bool}` |
| `/api/logs` | GET | `["log_2026-04-20.log", ...]` |
| `/api/download?file=<name>` | GET | binary stream; 409 when recording is active |
| `/api/clear-logs` | GET | `{"deleted": int}` |

---

## Export `api_client.py` to another project

`api_client.py` has no PyQt dependency.  
Just copy the file into your target project and install `requests`:

```bash
pip install requests
```

### Basic usage

```python
from api_client import ApiClient, RecordingActiveError

client = ApiClient("192.168.1.254", "80")

# Read sensors
sensors = client.get_sensors()          # [{"rpm": 120.5, "hz": 2.0}, ...]

# Read device status
status = client.get_status()            # {"status": "ok", "logging": False}

# List available logs
logs = client.get_logs()                # ["log_2026-04-20.log", ...]

# Download a log (optional progress callback)
def on_progress(pct: int):
    print(f"\r{pct}%", end="")

client.download_log("log_2026-04-20.log", "/local/path/log.log", on_progress)

# Delete old logs
result = client.clear_logs()            # {"deleted": 2}

# Release network resources
client.close()
```

### Polling usage (without PyQt)

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

### Error handling

```python
import requests
from api_client import ApiClient, RecordingActiveError

client = ApiClient("192.168.1.254", "80")

try:
    client.download_log("log.log", "local.log")
except RecordingActiveError:
    print("Device is recording, try again later.")
except requests.Timeout:
    print("Device is not responding.")
except requests.HTTPError as e:
    print(f"HTTP error: {e.response.status_code}")
```

### PyQt integration (recommended pattern)

The pattern used in `main.py` is the recommended way to integrate `ApiClient` into a PyQt GUI:

```python
class MyWorker(QtCore.QThread):
    data_ready = QtCore.pyqtSignal(list)

    def __init__(self, api: ApiClient):
        super().__init__()
        self.api = api

    def run(self):
        data = self.api.get_sensors()    # blocking call - safe in background thread
        self.data_ready.emit(data)       # always emit from worker thread, never touch UI directly
```

Key rule: **always call `ApiClient` from a `QThread`, never from the main UI thread**, to avoid blocking the interface.

---

## Behavior settings (`main.py`)

Constants at the top of `main.py` let you tune behavior without changing business logic:

| Constant | Default value | Description |
|---|---|---|
| `_SENSOR_COUNT` | `12` | Number of rows in the sensor table |
| `_STATUS_POLL_EVERY` | `5` | Sensor cycles between status requests |
| `_POLL_SLEEP_MS` | `100` | Sleep ms per internal polling iteration |
| `_POLL_SLEEP_ITERS` | `10` | Iterations per cycle (cycle ~= 1 s total) |
| `_SENSORS_ERR_THRESHOLD` | `5` | Consecutive `/api/sensors` failures before reporting |
| `_STATUS_ERR_THRESHOLD` | `3` | Consecutive `/api/status` failures before disconnect |
