# Test_API — Reference Client for the GeCo API

A PyQt6 desktop application designed as a practical example for consuming the GeCo REST API.

The interface serves to validate the complete workflow, but the most reusable component is `api_client.py`: an HTTP client decoupled from PyQt that you can integrate into other programs.

---

## Project Structure

```
Test_API/
├── api_client.py          <- pure HTTP client (exportable)
├── main.py                <- reference PyQt6 application
├── interfaz.ui            <- main window (Qt Designer)
├── log_dialog.ui          <- log management dialog
├── ips.json               <- automatically saved IPs
├── requirements.txt
└── docs/
    └── api_guide.md       <- implementation and reuse guide
```

---

## Requirements

- Python >= 3.10
- Dependencies:

```bash
pip install -r requirements.txt
```

---

## Running the App

```bash
python main.py
```

---

## App Functional Flow

### 1) Connection

1. Enter the IP and port (default `80`).
2. When you click **Connect**, `/api/status` is verified first.
3. If there is a connection, the IP is saved in `ips.json` and background polling starts.

Additional IP management in the UI:

- **Save IP**: manually stores the current IP in `ips.json`.
- **Remove IP**: removes the current IP from `ips.json`.

Manual API check:

- **Check API** triggers an on-demand call to `/api/status` and shows the result in the status bar.

### 2) Real-time Polling

The `PollingWorker` queries the API without blocking the UI:

| Endpoint | Frequency approx. | Usage |
|---|---|---|
| `/api/sensors` | ~1 s | RPM/Hz readings |
| `/api/status` | ~5 s | General status and recording state |

The worker tolerates transient failures and only reports sustained errors.

### 3) Log Management

From the **Log Manager** you can:

- List logs (`/api/logs`)
- Download (`/api/download?file=<name>`)
- Clear old logs while preserving the most recent one (`/api/clear-logs`)

Downloads use `DownloadWorker` (separate thread) to maintain UI responsiveness.

### 4) Automatic Renaming When Recording Closes

When the app detects the transition `logging: true -> false` in `/api/status`:

1. Reads `last_log`.
2. Opens a rename modal.
3. Allows keeping the name, renaming, and optionally downloading.

Rename field behavior:

- The editable field shows the base name (without extension).
- The `.log` extension is appended automatically when confirming.
- Name validation uses `_LOG_NAME_RE` before submitting the rename.

Modal cancel button behavior:

- **Does not cancel the recording** (it has already ended).
- Only **cancels the rename/download action** and keeps the original file name.

---

## Device API (Summary)

| Endpoint | Method | Typical Response |
|---|---|---|
| `/api/sensors` | GET | `[{"rpm": float, "hz": float}, ...]` |
| `/api/status` | GET | `{"status": str, "logging": bool, "last_log": str}` |
| `/api/logs` | GET | `["log_2026-04-20.log", ...]` |
| `/api/download?file=<name>` | GET | binary stream |
| `/api/clear-logs` | GET | `{"deleted": int}` |
| `/api/rename?from=<old>&to=<new>` | GET | `{"status":"success","from":"...","to":"..."}` |

Common errors:

- `409`: operation not allowed during active recording, or destination name already exists (rename).
- `400`: invalid parameters.
- `404`: source file not found (in rename).
- `500`: SD rename operation failed.

For rename failures, the device may return a JSON body with a specific cause such as:
`recording`, `invalid_name`, `not_found`, `already_exists`, `rename_failed`.

---

## Reusing `api_client.py` in Other Programs

`api_client.py` does not depend on PyQt. You can copy it directly and use it in scripts, services, or any GUI.

Minimal installation:

```bash
pip install requests
```

Quick example:

```python
from api_client import ApiClient, RecordingActiveError

client = ApiClient("192.168.1.254", "80")

status = client.get_status()
logs = client.get_logs()

if logs:
    client.rename_log(logs[0], "my_renamed_log.log")

client.close()
```

---

## Configurable Settings in `main.py`

| Constant | Default Value | Description |
|---|---|---|
| `_SENSOR_COUNT` | `12` | Number of sensor rows in table |
| `_STATUS_POLL_EVERY` | `5` | Sensor cycles between status reads |
| `_POLL_SLEEP_MS` | `100` | Wait per internal iteration |
| `_POLL_SLEEP_ITERS` | `10` | Iterations per cycle (~1 s) |
| `_SENSORS_ERR_THRESHOLD` | `5` | `/api/sensors` failures before notifying |
| `_STATUS_ERR_THRESHOLD` | `3` | `/api/status` failures before disconnecting |
| `_LOG_NAME_RE` | `^[\w\-]{1,60}\.log$` | Name validation rule for renaming |

---

## Integration Recommendations

1. Separate HTTP transport and UI (as in `api_client.py` + `main.py`).
2. Run periodic requests in a background thread.
3. Maintain validation on both client and server for file operations.
4. For long-running operations (e.g., downloads), use dedicated workers.

For a more detailed guide aimed at implementation in other projects, check `docs/api_guide.md`.
