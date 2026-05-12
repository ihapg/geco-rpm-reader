# Manual operativo del firmware — GeCo RPM Reader

Este manual está orientado a operación y soporte en campo.

Objetivo:

- Identificar rápido qué está fallando.
- Aplicar una acción concreta para recuperar servicio.
- Dejar trazabilidad básica de la incidencia.

---

## 1. Comprobación rápida (menos de 2 minutos)

1. Verificar alimentación y cable Ethernet.
2. Confirmar que el firmware arrancó (mensajes por serial).
3. Probar estado de API:
   - GET /api/status
4. Verificar sensores:
   - GET /api/sensors
5. Verificar SD:
   - GET /api/logs

Si falla /api/status, revisar primero red/IP antes de revisar sensores o SD.

---

## 2. Síntomas frecuentes y diagnóstico

## Caso A: El cliente no conecta

Síntoma:

- Timeout o error de conexión al abrir la app.

Revisar:

1. IP y puerto del dispositivo.
2. Configuración de red (subred y gateway).
3. Que M7 haya inicializado Ethernet correctamente.

Acción recomendada:

1. Reiniciar solo el flujo de red.
2. Si persiste, reiniciar placa y repetir /api/status.

---

## Caso B: /api/sensors responde, pero hay cortes intermitentes

Síntoma:

- Lecturas que se actualizan y se pierden a ratos.

Revisar:

1. Carga de clientes concurrentes.
2. Estado del loop principal en M7.
3. Errores de red en el cliente.

Acción recomendada:

1. Reducir clientes temporales de prueba.
2. Verificar que sigue activo el esquema multicliente no bloqueante.

---

## Caso C: No aparecen logs nuevos

Síntoma:

- /api/logs no muestra archivos recientes.

Revisar:

1. Estado de grabación en /api/status (campo logging).
2. Disponibilidad de SD al arranque.
3. Permisos/estado de montaje de SD.

Acción recomendada:

1. Comprobar mensajes de SD por serial.
2. Verificar que logging_session está actualizando estado.

Comportamiento esperado con red caída durante grabación:

- La sesión activa debe seguir escribiendo líneas con progresión temporal.
- La resincronización NTP debe quedar diferida hasta finalizar la grabación.

---

## Caso D: El renombrado de logs falla

Síntoma:

- /api/rename devuelve error.

Interpretación por código:

- 400: parámetros inválidos o nombre peligroso.
- 404: archivo origen no existe.
- 409: grabación activa o nombre destino ya ocupado.
- 500: fallo en operación rename de SD.

Acción recomendada:

1. Reintentar con nombre válido simple (ejemplo: lote_01.log).
2. Verificar que no se esté grabando.
3. Verificar que el archivo origen sigue en /api/logs.

---

## 3. Procedimiento de recuperación escalonado

1. Reintento de endpoint afectado.
2. Revisión de red y estado API.
3. Reinicio de cliente (PyQt/web).
4. Reinicio de firmware (placa).
5. Si persiste, capturar evidencias y escalar.

Nota operativa:

- Ante caída de Ethernet durante sesión activa, priorizar que el logging continúe y validar la calidad temporal del fichero al cierre antes de forzar reinicios.

No aplicar cambios de código en caliente sin reproducir primero el fallo.

---

## 4. Evidencias mínimas para soporte

Guardar siempre:

1. Hora del incidente.
2. Endpoint que falló y código HTTP.
3. Última respuesta de /api/status.
4. Últimos mensajes relevantes de serial.
5. Acción aplicada y resultado.

Formato sugerido:

- Fecha/hora:
- Equipo/IP:
- Síntoma:
- Endpoint/código:
- Acción:
- Resultado:

---

## 5. Mantenimiento preventivo

1. Revisar periódicamente espacio y salud de SD.
2. Limpiar logs antiguos con /api/clear-logs.
3. Evitar múltiples clientes de test simultáneos sin necesidad.
4. Mantener NTP operativo para timestamps consistentes.

---

## 6. Checklist antes de cerrar incidencia

1. /api/status responde correctamente.
2. /api/sensors devuelve datos estables.
3. /api/logs lista archivos esperados.
4. Si aplica, /api/rename ejecuta correctamente.
5. Se registró evidencia del incidente.
6. Si hubo caída de red en grabación, el log cerrado mantiene secuencia temporal coherente sin huecos por resincronización NTP.

---

## 7. Referencias de código para diagnóstico

- src/m7/m7_web.cpp
- src/m7/core/web_router.cpp
- src/m7/api/api_handlers.cpp
- src/m7/logging/logging_session.cpp
- src/m4/m4_acq.cpp

Con estos archivos normalmente se localiza la mayoría de incidencias operativas.
