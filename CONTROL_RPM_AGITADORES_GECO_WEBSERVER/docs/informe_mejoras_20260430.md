# Informe de Mejoras — Proyecto GeCo RPM Reader

**Fecha original:** 30 de abril de 2026
**Última actualización:** 15 de mayo de 2026
**Proyectos analizados:** 3 (Firmware PlatformIO, Frontend Web SD, Cliente Python PyQt6)
**Estado general actual:** Funcional y estable en operación diaria. Persisten mejoras relevantes en robustez de API, seguridad de parámetros y despliegue de configuración.

---

## Resumen ejecutivo actualizado

El proyecto mantiene una buena base técnica y separación por dominios (M4/M7, UI web y cliente Python). Desde el informe original se observa avance en configuración del frontend, pero los riesgos más importantes siguen concentrados en firmware/API (estado compartido entre endpoints y validación incompleta de entradas) y en endurecimiento de prácticas de calidad/entorno.

---

## Estado de hallazgos del informe original

### 1. Acoplamiento de estado en endpoints del firmware

**Estado:** Pendiente (sin cambios sustanciales)

**Situación actual:**
`/api/clear-logs` sigue dependiendo de la variable global `nameLogs`, que se rellena en `/api/logs`. Si no se llama antes a `/api/logs`, el borrado puede responder `no_logs` aunque existan archivos.

**Riesgo:** Inconsistencia funcional con uno o varios clientes.

**Acción recomendada:** Recalcular lista de logs dentro de `handleClearLogs` y eliminar la dependencia global.

---

### 2. Validación insuficiente en descarga de logs

**Estado:** Pendiente

**Situación actual:**
`/api/download` sigue obteniendo `file=` por `substring` y construye la ruta sin validación equivalente a `rename`.

**Riesgo:** Posible path traversal por nombres maliciosos (`..`, `/`, `\\`).

**Acción recomendada:** Añadir validación defensiva y decodificación de query string antes de servir archivo.

---

### 3. Llamadas HTTP bloqueantes desde hilo UI en PyQt

**Estado:** Parcialmente pendiente

**Situación actual:**
Existe patrón correcto con worker para descarga (`DownloadWorker`) y polling (`PollingWorker`), pero en `LogDialog` y conexión inicial aún hay llamadas bloqueantes con `processEvents()` como workaround.

**Riesgo:** Congelación de UI con latencia o fallos de red.

**Acción recomendada:** Mover `cargar_logs`, `limpiar_logs` y verificación/conexión manual a workers dedicados.

---

### 4. Configuración de entorno hardcodeada

**Estado:** Parcialmente resuelto

**Situación actual:**
- **Frontend web:** Resuelto. `SD_Files/script.js` usa URL relativa (`/api`).
- **Firmware:** Pendiente. `m7_web.cpp` mantiene configuración de red/NTP hardcodeada para desarrollo.

**Riesgo:** Fricción para despliegue prod/dev y riesgo de errores al cambiar entorno.

**Acción recomendada:** Externalizar parámetros de red/NTP con flags de compilación o fichero de configuración de entorno.

---

### 5. Falta de automatización de calidad

**Estado:** Pendiente

**Situación actual:**
Sin CI/CD mínima para build firmware ni pruebas automatizadas consistentes en cliente Python.

**Riesgo:** Regresiones detectadas tarde, dependencia excesiva de validación manual con hardware.

**Acción recomendada:** Pipeline mínima con build M7/M4 + checks Python + smoke tests de API cliente.

---

## Nuevos hallazgos relevantes (mayo 2026)

### 6. Falta de autosuficiencia de `/api/clear-logs` en escenarios multi-cliente

Aunque relacionado con el punto 1, se confirma como caso operativo específico: el endpoint mezcla estado cacheado (`nameLogs`) con escaneo de directorio, lo que puede producir decisiones de borrado basadas en datos desfasados.

**Recomendación:** Fuente única de verdad por request (escaneo local al endpoint).

---

### 7. Duplicación de respuestas HTTP de error en handlers

Persisten varias respuestas HTTP construidas manualmente (`client.println`) con encabezados repetidos.

**Riesgo:** Inconsistencias de formato y mayor coste de mantenimiento.

**Recomendación:** Helper común `sendErrorJson(...)` o equivalente.

---

### 8. Endpoints mutables aún con método GET

Operaciones con efectos (`clear-logs`, `rename`) siguen modelo simplificado de GET.

**Riesgo:** Semántica HTTP débil, cachés/proxies potencialmente problemáticos en escenarios extendidos.

**Recomendación:** Evolucionar a `POST/DELETE` cuando router y clientes estén listos.

---

## Priorización revisada

### Prioridad P0 (inmediata)

1. Desacoplar `/api/clear-logs` de `nameLogs`.
2. Endurecer validación de `file=` en `/api/download`.

### Prioridad P1 (corto plazo)

1. Configuración de entorno firmware (dev/prod) sin edición manual de código.
2. Eliminar bloqueos de red en UI PyQt para operaciones de logs.

### Prioridad P2 (mejora continua)

1. Helpers de respuestas HTTP para reducir duplicación.
2. CI mínima de build + checks.
3. Ajuste de semántica HTTP en endpoints mutables.

---

## Plan mínimo recomendado (sin cambios disruptivos)

1. Firmware API: corregir `clear-logs` + validar/normalizar `download`.
2. Firmware core: abstraer red/NTP por configuración de compilación.
3. Cliente Python: workers para carga/limpieza de logs.
4. Calidad: pipeline CI con build M7/M4 y validación básica de cliente Python.

Con este paquete, se reduce el mayor riesgo funcional y de soporte sin reestructurar la arquitectura existente.
