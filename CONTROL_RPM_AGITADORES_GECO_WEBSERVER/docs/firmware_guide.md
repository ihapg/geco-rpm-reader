# Guía del Firmware — GeCo RPM Reader

Esta guía explica de forma sencilla cómo está organizado el firmware, cómo fluye la información entre núcleos y cómo ampliar la API sin romper el comportamiento actual.

Está pensada como documentación técnica de referencia y también como base para reutilizar el diseño en otros proyectos similares.

---

## 1. Visión general

El firmware corre en una Portenta H7 (doble núcleo) y divide responsabilidades:

- M4: adquisición de señales de sensores (tiempo real)
- M7: red, API HTTP, logging en SD y sincronización de hora

Esta separación evita mezclar tareas críticas de muestreo con tareas de red/almacenamiento.

---

## 2. Estructura del proyecto (firmware)

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
│       ├── m7_web.cpp
│       ├── m7_web.h
│       ├── api/
│       │   ├── api_handlers.cpp
│       │   └── api_handlers.h
│       ├── core/
│       │   ├── web_router.cpp
│       │   └── web_router.h
│       ├── logging/
│       │   ├── logging_session.cpp
│       │   ├── logging_session.h
│       │   ├── sensor_log_writer.cpp
│       │   └── sensor_log_writer.h
│       └── time/
│           ├── time_service.cpp
│           └── time_service.h
└── docs/
    ├── PLAN_HTTP_MULTICLIENTE.md
    ├── firmware_guide.md
    └── firmware_operational_manual.md
```

---

## 3. Flujo principal de datos

### 3.1 Adquisición en M4

M4 toma lecturas de agitadores y mantiene los datos preparados para ser consultados por RPC.

### 3.2 Sincronización M7 <- M4

M7 consulta periódicamente por RPC y actualiza su estructura de sensores.

Resultado:

- La API HTTP devuelve datos siempre desde M7
- M4 queda aislado de conexiones de red

### 3.3 Publicación y logging en M7

Con los datos actualizados:

1. Se atienden peticiones HTTP
2. Se mantiene estado de grabación
3. Se escriben logs en SD
4. Se sincroniza hora/NTP

Durante una desconexión de red, el firmware mantiene continuidad temporal del logging usando una referencia de tiempo válida más el reloj interno. Al recuperarse la red, la resincronización NTP en runtime se ejecuta de forma no bloqueante y fuera de sesión activa de grabación para evitar discontinuidades en el fichero.

---

## 4. API HTTP actual

Endpoints expuestos por el firmware:

| Endpoint | Método | Uso |
|---|---|---|
| /api/sensors | GET | Lecturas RPM/Hz |
| /api/status | GET | Estado general + logging + last_log |
| /api/logs | GET | Lista de archivos .log |
| /api/download?file=<name> | GET | Descarga de log |
| /api/clear-logs | GET | Limpia logs antiguos |
| /api/rename?from=<old>&to=<new> | GET | Renombra archivo en SD |

Notas de diseño:

- Se usa GET en todos los endpoints por simplicidad del router actual.
- Las respuestas se devuelven en JSON cuando aplica.
- Durante grabación activa, operaciones sensibles devuelven 409.

---

## 5. Router y handlers

### 5.1 Router

El router mapea rutas a funciones handler y delega el procesamiento de cada endpoint.

Responsabilidad del router:

- Encaminar la petición
- Resolver recursos estáticos si aplica
- Devolver 404 cuando no hay coincidencia

### 5.2 Handlers de API

Cada handler implementa:

1. Validación de entrada
2. Lógica de negocio
3. Respuesta HTTP/JSON

Patrón recomendado para nuevos handlers:

- Rechazo temprano de casos inválidos
- Mensajes de error concretos
- Operaciones de filesystem solo después de validar parámetros

---

## 6. Logging en SD

El subsistema de logging mantiene el ciclo completo de grabación:

- Inicio cuando detecta actividad en sensores
- Escritura periódica con timestamp
- Cierre controlado tras parada

Cuando Ethernet no está disponible durante una sesión, la grabación continúa con tiempo interno y mantiene una secuencia temporal monotónica.

### 6.1 last_log al arranque

Al iniciar M7, se intenta recuperar el log más reciente en SD para no arrancar con estado vacío.

Esto permite que /api/status informe last_log incluso antes de una nueva sesión.

### 6.2 Renombrado seguro

El endpoint de rename aplica validaciones mínimas antes de ejecutar rename:

1. No grabación activa
2. from/to presentes y no vacíos
3. Rechazo de patrones peligrosos (.., /, \\)
4. Origen existente
5. Destino libre

Errores típicos:

- 400 invalid_name
- 404 not_found
- 409 recording / already_exists
- 500 rename_failed

---

## 7. Gestión multicliente HTTP

El firmware usa una estrategia multi-slot no bloqueante para atender varios clientes sin bloquear el loop principal.

Ventajas:

- Evita resets de conexión por cierres abruptos
- Reduce timeouts de clientes concurrentes
- Mantiene responsive RPC, NTP y logging

---

## 8. Reloj y NTP

El tiempo se usa para:

- Timestamp consistente en logs
- Estado operativo y trazabilidad

Buenas prácticas:

- Sincronizar periódicamente
- Separar offset de horario (DST) de la hora base
- Evitar depender de una sola actualización puntual

Comportamiento operativo del firmware:

1. Arranque: sincronización inicial para establecer referencia horaria.
2. Runtime: resincronización no bloqueante al recuperar red, con reintentos espaciados.

Durante grabación activa, los intentos de resincronización NTP se difieren hasta el cierre de sesión para evitar saltos temporales en el log.

---

## 9. Cómo añadir un endpoint nuevo (guía rápida)

1. Declarar el handler en api_handlers.h.
2. Implementarlo en api_handlers.cpp.
3. Registrar la ruta en web_router.cpp.
4. Validar parámetros de entrada.
5. Responder siempre con HTTP y JSON coherente.
6. Probar éxito y casos de error.

Checklist mínimo:

- Caso válido
- Parámetros faltantes
- Parámetros maliciosos o inválidos
- Recursos no existentes
- Conflictos de estado (por ejemplo grabación activa)

---

## 10. Reutilizar esta arquitectura en otros proyectos

Este diseño es útil cuando necesitas:

- Muestreo estable en tiempo real
- API de red con varios clientes
- Persistencia local en almacenamiento

Patrón reusable:

1. Núcleo de adquisición aislado
2. Núcleo de servicios/red desacoplado
3. Comunicación interna simple (RPC o cola)
4. Endpoints pequeños y validados
5. Logging robusto orientado a diagnóstico

---

## 11. Recomendaciones de mantenimiento

1. Mantener consistencia de respuestas JSON de error entre endpoints.
2. Evitar duplicar lógica de parseo de query string; extraer helper cuando crezcan endpoints.
3. Añadir pruebas de integración para operaciones de archivos (list, clear, rename).
4. Documentar cambios de API en este directorio docs/.

Para operación diaria y resolución rápida de incidencias, consultar también:

- docs/firmware_operational_manual.md

---

## 12. Archivos clave para empezar a leer

- src/m7/m7_web.cpp
- src/m7/core/web_router.cpp
- src/m7/api/api_handlers.cpp
- src/m7/logging/logging_session.cpp
- src/m4/m4_acq.cpp

Con estos cinco archivos se entiende casi todo el comportamiento de extremo a extremo.
