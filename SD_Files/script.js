// === Variables ===
// URL para recibir los datos (No es necesario cambiar por la ruta de servidor Arduino, con el endpoint de la API es suficiente)
// Añadir a URL para desarrollo en local: http://169.254.1.2
const url = "http://169.254.1.2/api";
// Intervalo de refresco en ms
const fetchInterval = 1000;
// Timeout para cada petición fetch en ms
const fetchTimeout = 4000;

let isFetching = false;
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
        const status = document.getElementById('status');
        if (status) status.textContent = 'Error de conexión: ' + (error.name === 'AbortError' ? 'timeout' : error);
        console.error('Error al obtener datos:', error);
    } finally {
        isFetching = false;
        // Programa siguiente intento tras intervalo
        setTimeout(refreshTableData, fetchInterval);
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
 *    `getFileNameFromResponse`, usa `data.csv` por defecto.
 * 6. Crea un `<a>` oculto, le asigna `href`+`download` y simula el clic para iniciar la descarga.
 * 7. Limpia el `objectURL` y vuelve a habilitar el botón.
 * Errores: se registran en consola; no se propagan.
 */
async function downloadCSV() {
    if (isFetching) return;
    const btn = document.getElementById('btnDownload');
    if (btn) btn.disabled = true;

    try {
        isFetching = true;

        const req = url.concat("/download_csv");
        // const req = `${url}/download_csv`;

        const response = await fetchWithTimeout(req);
        if (!response.ok) throw new Error(response.status);
        const blob = await response.blob();
        const filename = getFileNameFromResponse(response) || 'data.csv';
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
        if (btn) btn.disabled = false;
    }
}

// === Ejecución ===
const btnDownload = document.getElementById('btnDownload');
if (btnDownload) btnDownload.addEventListener('click', downloadCSV);

refreshTableData();

