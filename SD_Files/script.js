// =========== VARIABLES ===========
// URL para recibir los datos (No es necesario cambiar por la ruta de servidor Arduino, con el endpoint de la API es suficiente)
const url = "/data.json";
// Intervalo de refresco en ms
const fetchInterval = 1000;
// Timeout para cada petición fetch en ms
const fetchTimeout = 4000;

let isFetching = false;
let lastDataJSON = null;

const colors = ["#D3D3D3", "#9B9B9B"];


// =========== FUNCTIONS ===========

function fillTable(data) {
    const tabla = document.getElementById('table_data');
    tabla.innerHTML = '';
    let index = 0;

    for (const sensor of data) {
        const fila = document.createElement('tr');
        fila.style.backgroundColor = colors[index % 2];

        const celdaClave = document.createElement('td');
        celdaClave.textContent = sensor.sensor;

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
        const response = await fetchWithTimeout(url);
        if (!response.ok) {
            throw new Error(response.status);
        }
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

// =========== SCRIPT ===========

refreshTableData();

