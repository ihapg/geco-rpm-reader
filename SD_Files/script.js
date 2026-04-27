// === Variables ===
// URL para recibir los datos (No es necesario cambiar por la ruta de servidor Arduino, con el endpoint de la API es suficiente)
// Añadir a URL para desarrollo en local: "http://169.254.1.2/api" // Para produccion: "/api"
const url = "/api";
// Intervalo de refresco en ms
const fetchInterval = 1000;
// Intervalo de comprobación de estado de logging en ms (no cambia frecuentemente)
const statusInterval = 5000;
// Timeout para cada petición fetch en ms
const fetchTimeout = 4000;

let isFetching = false;
let isCheckingStatus = false;

// Contadores de error para backoff exponencial
let sensorsErrorCount = 0;
let statusErrorCount = 0;

let isLogging = false;
let lastDataJSON = null;

const colors = ["#D3D3D3", "#9B9B9B"];


// === Funciones ===

function fillTable(data) {
    const tabla = document.getElementById('table_data');
    tabla.innerHTML = '';
    let index = 0;

    for (const sensor of data) {
        const fila = document.createElement('tr');
        fila.style.backgroundColor = colors[index % 2];

        const celdaClave = document.createElement('td');
        celdaClave.textContent = sensor.id;

        const celdaRPM = document.createElement('td');
        celdaRPM.textContent = Number(sensor.rpm).toFixed(2);

        const celdaHz = document.createElement('td');
        celdaHz.textContent = Number(sensor.hz).toFixed(2);

        fila.appendChild(celdaClave);
        fila.appendChild(celdaRPM);
        fila.appendChild(celdaHz);
        tabla.appendChild(fila);

        index++;
    };
}

async function fetchWithTimeout(resource, options = {}) {
    const { timeout = fetchTimeout } = options;
    const controller = new AbortController();
    const id = setTimeout(() => controller.abort(), timeout);
    return fetch(resource, { ...options, signal: controller.signal, cache: 'no-cache' }).finally(() => clearTimeout(id));
}

async function refreshTableData() {
    // Evita solapamientos si la petición anterior sigue en curso
    if (isFetching) return;
    isFetching = true;
    try {
        let req = url.concat("/sensors");
        const response = await fetchWithTimeout(req);
        if (!response.ok) throw new Error(response.status);
        const data = await response.json();

        // Evitar actualizar el DOM si los datos no han cambiado
        const newJSON = JSON.stringify(data);
        if (newJSON !== lastDataJSON) {
            lastDataJSON = newJSON;
            const status = document.getElementById('status');
            if (status) status.textContent = '';
            fillTable(data);
        }
    } catch (error) {
        sensorsErrorCount++;
        const status = document.getElementById('status');
        if (status) status.textContent = 'Error de conexión: ' + (error.name === 'AbortError' ? 'timeout' : error);
        console.error('Error al obtener datos:', error);
    } finally {
        isFetching = false;
        // Backoff exponencial: duplica el intervalo por cada fallo consecutivo (cap 30 s)
        const delay = sensorsErrorCount === 0
            ? fetchInterval
            : Math.min(fetchInterval * Math.pow(2, sensorsErrorCount), 30000);
        setTimeout(refreshTableData, delay);
    }
}

function updateLoggingStatus(logging) {
    let btnOpenModal = document.getElementById('btnOpenLogList');
    let spinnerLogging = document.getElementById('loggingSpinner');

    if (logging) {
        btnOpenModal.disabled = true;
        btnOpenModal.innerText = "Grabación en curso..."

        spinnerLogging.style.display = 'inline-block';

        if (modal.style.display !== 'none' && !isFetching) {
            modal.style.display = 'none';
        }
    } else {
        btnOpenModal.disabled = false;
        btnOpenModal.innerText = "Registro de datos"

        spinnerLogging.style.display = 'none';
    }
}

async function checkLoggingStatus() {
    if (isCheckingStatus) return;
    isCheckingStatus = true;
    try {
        const response = await fetchWithTimeout(`${url}/status`);
        if (!response.ok) throw new Error(response.status);
        const result = await response.json();

        statusErrorCount = 0; // Éxito: resetear backoff
        if (result.logging != isLogging) {
            isLogging = result.logging;
            updateLoggingStatus(isLogging);
        }
    } catch (error) {
        statusErrorCount++;
        console.error("Error al comprobar estado: " + (error.name === 'AbortError' ? 'timeout' : error.message));
    } finally {
        isCheckingStatus = false;
        // Backoff exponencial: duplica el intervalo por cada fallo consecutivo (cap 30 s)
        const delay = statusErrorCount === 0
            ? statusInterval
            : Math.min(statusInterval * Math.pow(2, statusErrorCount), 30000);
        setTimeout(checkLoggingStatus, delay);
    }
}

/**
 * getFileNameFromResponse
 * Extrae el nombre de archivo desde la cabecera `Content-Disposition` de la respuesta.
 * - Soporta formatos simples como: `filename="datos.csv"` y RFC5987 `filename*=UTF-8''datos%20con%20espacios.csv`.
 * - Devuelve `null` si la cabecera no está presente o no contiene un filename.
 * - NOTA: en peticiones cross-origin el servidor debe exponer la cabecera mediante
 *   `Access-Control-Expose-Headers: Content-Disposition` para que el navegador la pueda leer.
 */
function getFileNameFromResponse(response) {
    const cd = response.headers.get('content-disposition');
    if (!cd) return null;
    const match = cd.match(/filename\*?=(?:UTF-8''|\")?([^;\"]+)/i);
    return match ? decodeURIComponent(match[1]) : null;
}

/**
 * downloadCSV
 * Solicita el CSV al servidor y fuerza la descarga en el navegador.
 * Flujo principal:
 * 1. Evita solapamientos con `isFetching`.
 * 2. Deshabilita el botón de descarga para prevenir dobles clics.
 * 3. Realiza `fetchWithTimeout` al endpoint `/download_csv`.
 * 4. Convierte la respuesta a `Blob` y crea un `objectURL` temporal.
 * 5. Extrae el nombre de archivo (si el servidor lo proporciona) con
 *    `getFileNameFromResponse`.
 * 6. Crea un `<a>` oculto, le asigna `href`+`download` y simula el clic para iniciar la descarga.
 * 7. Limpia el `objectURL` y vuelve a habilitar el botón.
 * Errores: se registran en consola; no se propagan.
 */
async function downloadCSV() {
    if (isFetching) return;
    // const btn = document.getElementById('btnDownload');
    // if (btn) btn.disabled = true;

    if (btnDownloadLog && btnClearLogs) {
        btnDownloadLog.disabled = true;
        btnClearLogs.disabled = true;
    }

    try {
        isFetching = true;

        const req = url.concat("/download");
        // const req = `${url}/download?file=...`;

        const response = await fetchWithTimeout(req);
        if (!response.ok) throw new Error(response.status);

        const blob = await response.blob();
        const filename = getFileNameFromResponse(response);
        const a = document.createElement('a');
        const objectUrl = URL.createObjectURL(blob);
        a.href = objectUrl;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        a.remove();
        URL.revokeObjectURL(objectUrl);
    } catch (error) {
        console.error('Error al descargar el archivo: ', error);
    } finally {
        isFetching = false;
        if (btnDownloadLog && btnClearLogs) {
            btnDownloadLog.disabled = false;
            btnClearLogs.disabled = false;
        }
    }
}

async function loadLogList() {
    if (btnRefreshLogList) {
        btnRefreshLogList.disabled = true;
        btnRefreshLogList.classList.add('loading');
    }
    logSelect.innerHTML = '<option>Cargando...</option>';

    try {
        const response = await fetchWithTimeout(`${url}/logs`);
        const files = await response.json();

        logSelect.innerHTML = files.length
            ? files.map(f => `<option value="${f}">${f}</option>`).join('')
            : '<option value="">No hay archivos .log</option>';
    } catch (error) {
        logSelect.innerHTML = '<option>Error al cargar archivos</option>';
    } finally {
        if (btnRefreshLogList) {
            btnRefreshLogList.disabled = false;
            btnRefreshLogList.classList.remove('loading');
        }
    }
}

// === Ejecución ===

const modal = document.getElementById('logsModal');
const logSelect = document.getElementById('logSelect');
const btnOpenLogList = document.getElementById('btnOpenLogList');
const btnDownloadLog = document.getElementById('btnDownloadLog');
const btnClearLogs = document.getElementById('btnClearLogs');
const btnCloseLogList = document.getElementById('btnCloseLogList');
const btnRefreshLogList = document.getElementById('btnRefreshLogList');

btnOpenLogList.onclick = async () => {
    modal.style.display = 'block';
    loadLogList();
};

btnCloseLogList.onclick = () => modal.style.display = 'none';

btnRefreshLogList.onclick = () => { if (!isFetching) loadLogList(); };

btnDownloadLog.onclick = async () => {
    const fileName = logSelect.value;
    if (!fileName || isFetching) return;

    // Feedback sobre el estado de la descarga
    const statusDiv = document.getElementById('downloadStatus');
    const statusText = document.getElementById('downloadStatusText');
    const progressBar = document.getElementById('downloadProgressBar');
    const showStatus = (msg) => {
        statusText.textContent = msg;
        statusDiv.style.display = 'block';
    };
    const hideStatus = () => {
        statusDiv.style.display = 'none';
        progressBar.classList.remove('has-progress');
        progressBar.style.width = '0%';
    };

    try {
        isFetching = true;
        btnDownloadLog.disabled = true;
        btnClearLogs.disabled = true;
        btnCloseLogList.disabled = true;
        showStatus(`Descargando...`);

        const req = `${url}/download?file=${fileName}`;

        const response = await fetchWithTimeout(req);
        if (response.status === 409) throw new Error("No se puede descargar: hay una grabación en curso");
        if (!response.ok) throw new Error("Error al descargar el archivo");

        // Si el servidor envía Content-Length, muestra progreso real
        const contentLength = response.headers.get('content-length');

        if (contentLength) {
            const total = parseInt(contentLength);
            let received = 0;
            const reader = response.body.getReader();
            const chunks = [];

            while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                chunks.push(value);
                received += value.length;

                // Actualizar progreso
                const pct = Math.round((received / total) * 100);
                progressBar.classList.add('has-progress');
                progressBar.style.width = pct + '%';
                statusText.textContent = `Descargando... ${pct}%`;
            }

            // Reconstruir blob desde chunks
            const blob = new Blob(chunks);
            const a = document.createElement('a');
            a.href = URL.createObjectURL(blob);
            a.download = fileName;
            a.click();
            URL.revokeObjectURL(a.href);

        } else {
            const blob = await response.blob();
            const a = document.createElement('a');
            a.href = URL.createObjectURL(blob);
            a.download = fileName;
            a.click();
            URL.revokeObjectURL(a.href);
        }

        modal.style.display = 'none';

    } catch (error) {
        alert("Error: " + error.message);
    } finally {
        isFetching = false;
        btnDownloadLog.disabled = false;
        btnClearLogs.disabled = false;
        btnCloseLogList.disabled = false;
        hideStatus();
    }
};

btnClearLogs.onclick = async () => {
    if (!confirm("¿Está seguro de que desea eliminar TODOS los archivos log? Esta acción no se puede deshacer."))
        return;

    try {
        const response = await fetchWithTimeout(`${url}/clear-logs`);
        const result = await response.json();

        if (result.status == "success") {
            alert(`Se han eliminado ${result.deleted} archivos.`);
            await loadLogList();
        }
    }
    catch (error) {
        alert("Error: " + error.message);
    }
};

refreshTableData();
// Desfase de 500 ms para evitar rafales simultáneas al servidor
setTimeout(checkLoggingStatus, 500);
