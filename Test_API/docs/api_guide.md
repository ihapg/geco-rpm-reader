# Guía de implementación — Test_API (GeCo)

Este documento resume cómo está construida la aplicación de prueba y cómo reutilizar su arquitectura en otros programas.

Objetivo principal:

- Consumir la API del dispositivo sin bloquear la interfaz.
- Mantener el cliente HTTP desacoplado de la UI.
- Resolver el flujo de logs completo: listar, descargar, limpiar y renombrar.

---

## 1. Arquitectura recomendada

La implementación se apoya en dos capas:

1. Capa de transporte: `api_client.py`
2. Capa de presentación: `main.py` (PyQt6)

### 1.1 Capa de transporte (`api_client.py`)

Responsabilidades:

- Construir URLs y parámetros.
- Ejecutar peticiones HTTP con `requests.Session`.
- Estandarizar timeouts y manejo de estados HTTP.
- Exponer métodos claros (`get_status`, `get_logs`, `rename_log`, etc.).

Ventaja:

- Puedes copiar este módulo a otro proyecto sin arrastrar dependencias de PyQt.

### 1.2 Capa de presentación (`main.py`)

Responsabilidades:

- Dibujar estado en la UI.
- Coordinar workers/hilos.
- Interpretar reglas de negocio visuales (p. ej. cuándo abrir el modal de renombrado).

Regla práctica:

- La UI no implementa lógica HTTP de bajo nivel.

---

## 2. Flujo operativo de la aplicación

### 2.1 Conexión inicial

1. Usuario introduce IP/puerto.
2. Se prueba `/api/status`.
3. Si responde, se marca conexión activa y se arranca el polling.

### 2.2 Polling en segundo plano

`PollingWorker` consulta periódicamente:

- `/api/sensors` cada ~1 s
- `/api/status` cada ~5 s

Si hay fallos intermitentes, se toleran.
Si falla `status` de forma sostenida, se fuerza desconexión controlada.

### 2.3 Gestión de logs

`LogDialog` permite:

- Listar logs (`/api/logs`)
- Descargar (`/api/download`)
- Limpiar (`/api/clear-logs`)

Las descargas se ejecutan en `DownloadWorker` para no congelar la UI.

### 2.4 Renombrado al finalizar grabación

La app observa el cambio de estado `logging`:

- Si detecta transición `True -> False`, toma `last_log` de `/api/status`.
- Abre `RenameLogDialog`.
- Si se confirma y cambia el nombre, llama a `/api/rename`.
- Si se marca descarga automática, descarga el archivo con nombre final.

Semántica de cancelar:

- Cancelar en el modal no altera una grabación ya finalizada.
- Solo omite renombrado/descarga y conserva el nombre original.

---

## 3. Endpoint de renombrado: decisiones y validaciones

Endpoint:

- `GET /api/rename?from=<old>&to=<new>`

Validaciones del firmware:

1. Si grabación activa: `409`.
2. `from` y `to` obligatorios.
3. Rechazo de patrones peligrosos (`..`, `/`, `\\`).
4. Origen debe existir.
5. Destino no debe existir.
6. `rename()` real en SD.

Respuestas:

- Éxito: `{"status":"success","from":"...","to":"..."}`
- Error: JSON con causa (`recording`, `invalid_name`, `not_found`, `already_exists`, `rename_failed`)

---

## 4. Guía de reutilización en otros proyectos

## Caso A: Script de consola

1. Copia `api_client.py`.
2. Instala `requests`.
3. Usa los métodos del cliente de forma secuencial.

```python
from api_client import ApiClient

api = ApiClient("192.168.1.254", "80")
print(api.get_status())
print(api.get_logs())
api.close()
```

## Caso B: Otra GUI (Tkinter, Kivy, etc.)

1. Conserva `api_client.py` sin cambios.
2. Crea un mecanismo de tareas en segundo plano.
3. Mueve al hilo de fondo operaciones periódicas y descargas.
4. Deja en el hilo principal solo actualización visual.

## Caso C: Servicio backend

1. Envuelve `ApiClient` en un servicio interno.
2. Añade reintentos y circuito de fallos según tus necesidades.
3. Expón datos limpios al resto de tu sistema.

---

## 5. Buenas prácticas que ya aplica esta app

1. Separación clara de capas (HTTP vs UI).
2. Uso de sesión HTTP persistente para reducir latencia.
3. Polling robusto con umbrales de error.
4. Operaciones de red largas fuera del hilo de UI.
5. Validación defensiva en cliente y firmware.
6. Mensajes de error interpretables para usuario y depuración.

---

## 6. Checklist para implementar este patrón desde cero

1. Implementar cliente HTTP aislado.
2. Definir endpoints y códigos de error esperados.
3. Añadir worker de polling.
4. Añadir worker de descarga.
5. Conectar eventos UI con resultados de worker.
6. Añadir validación de nombres de archivo en cliente.
7. Añadir validación de seguridad en firmware.
8. Probar transición de grabación y flujo de renombrado.

---

## 7. Siguientes mejoras sugeridas

1. Unificar helper para respuestas JSON de error en firmware.
2. Mostrar en UI errores de rename con mensajes más específicos por código.
3. Añadir tests automáticos del cliente (`pytest` + mocks HTTP).
4. Cambiar texto de botón cancelar por "Omitir" para mayor claridad de UX.
