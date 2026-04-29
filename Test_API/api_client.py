"""
api_client.py — Cliente HTTP para la API del dispositivo GeCo.

Este módulo es completamente independiente de PyQt y puede importarse
directamente en cualquier proyecto (scripts, tests, otras GUI, etc.):

    from api_client import ApiClient, RecordingActiveError

    client = ApiClient("192.168.1.254", "80")
    sensors = client.get_sensors()   # lista de dicts {rpm, hz}
    status  = client.get_status()    # dict {status, logging}
"""

import requests

# ---------------------------------------------------------------------------
# Constantes internas del cliente
# ---------------------------------------------------------------------------
_TIMEOUT_API = 5  # segundos — peticiones normales
_TIMEOUT_DOWNLOAD = 30  # segundos — descarga de ficheros de log (pueden ser grandes)
_CHUNK_SIZE = 8192  # bytes — tamaño de bloque en descarga en streaming
_HTTP_RECORDING = 409  # código HTTP que devuelve el dispositivo cuando está grabando


# ---------------------------------------------------------------------------
# Excepciones propias del cliente
# ---------------------------------------------------------------------------
class RecordingActiveError(RuntimeError):
    """Se lanza cuando el dispositivo rechaza una operación por tener grabación activa."""


# ---------------------------------------------------------------------------
# ApiClient
# ---------------------------------------------------------------------------
class ApiClient:
    """
    Encapsula toda la comunicación HTTP con la API REST del dispositivo.

    Reutiliza la misma sesión TCP (requests.Session) para todas las
    peticiones, lo que reduce la latencia en el polling continuo.

    Parámetros
    ----------
    ip : str
        Dirección IP del dispositivo.
    port : str
        Puerto HTTP del dispositivo (normalmente "80").
    """

    def __init__(self, ip: str, port: str):
        self.base_url = f"http://{ip}:{port}/api"
        self._session = requests.Session()

    # -- endpoints de lectura -----------------------------------------------

    def get_sensors(self) -> list:
        """
        Obtiene la lectura actual de todos los sensores.

        Returns
        -------
        list of dict
            Cada elemento corresponde a un agitador: ``{"rpm": float, "hz": float}``.
        """
        r = self._session.get(f"{self.base_url}/sensors", timeout=_TIMEOUT_API)
        r.raise_for_status()
        return r.json()

    def get_status(self) -> dict:
        """
        Obtiene el estado general del dispositivo.

        Returns
        -------
        dict
            Ejemplo: ``{"status": "ok", "logging": False}``.
        """
        r = self._session.get(f"{self.base_url}/status", timeout=_TIMEOUT_API)
        r.raise_for_status()
        return r.json()

    def get_logs(self) -> list:
        """
        Obtiene la lista de ficheros de log disponibles en el dispositivo.

        Returns
        -------
        list of str
            Nombres de fichero, p. ej. ``["log_2026-04-20.log", ...]``.
        """
        r = self._session.get(f"{self.base_url}/logs", timeout=_TIMEOUT_API)
        r.raise_for_status()
        return r.json()

    # -- endpoint de descarga -----------------------------------------------

    def download_log(
        self, filename: str, dest_path: str, progress_callback=None
    ) -> bool:
        """
        Descarga un fichero de log en streaming y lo guarda en disco.

        Parámetros
        ----------
        filename : str
            Nombre del fichero a descargar (tal como lo devuelve ``get_logs()``).
        dest_path : str
            Ruta local donde se guardará el fichero.
        progress_callback : callable, opcional
            Función que recibirá el porcentaje completado (0-100) como ``int``.
            Útil para actualizar barras de progreso en cualquier tipo de UI.

        Returns
        -------
        bool
            ``True`` al completar la descarga correctamente.

        Raises
        ------
        RecordingActiveError
            Si el dispositivo está grabando y no permite descargar.
        requests.HTTPError
            Para cualquier otro error HTTP.
        """
        r = self._session.get(
            f"{self.base_url}/download",
            params={"file": filename},
            timeout=_TIMEOUT_DOWNLOAD,
            stream=True,
        )
        if r.status_code == _HTTP_RECORDING:
            raise RecordingActiveError(
                "No se puede descargar durante grabación activa."
            )
        r.raise_for_status()

        total = int(r.headers.get("Content-Length", 0))
        downloaded = 0
        with open(dest_path, "wb") as f:
            for chunk in r.iter_content(chunk_size=_CHUNK_SIZE):
                f.write(chunk)
                downloaded += len(chunk)
                if progress_callback and total > 0:
                    progress_callback(int(downloaded * 100 / total))
        return True

    # -- endpoint de mantenimiento ------------------------------------------

    def clear_logs(self) -> dict:
        """
        Elimina los logs antiguos del dispositivo, conservando el más reciente.

        Returns
        -------
        dict
            Respuesta del dispositivo, p. ej. ``{"deleted": 3}``.
        """
        r = self._session.get(f"{self.base_url}/clear-logs", timeout=_TIMEOUT_API)
        r.raise_for_status()
        return r.json()

    # -- endpoint de renombrado ---------------------------------------------

    def rename_log(self, old_name: str, new_name: str) -> dict:
        """
        Renombra un fichero de log existente en el dispositivo.

        Parámetros
        ----------
        old_name : str
            Nombre actual del fichero (tal como lo devuelve ``get_logs()``).
        new_name : str
            Nombre nuevo deseado para el fichero.

        Returns
        -------
        dict
            Respuesta del dispositivo, p. ej. ``{"status": "success", "from": "...", "to": "..."}``.

        Raises
        ------
        RecordingActiveError
            Si el dispositivo está grabando y no permite renombrar.
        requests.HTTPError
            Para cualquier otro error HTTP (400 nombre inválido, 404 no existe, 409 destino ocupado, 500 fallo en SD).
        """
        r = self._session.get(
            f"{self.base_url}/rename",
            params={"from": old_name, "to": new_name},
            timeout=_TIMEOUT_API,
        )
        if r.status_code == _HTTP_RECORDING:
            raise RecordingActiveError(
                "No se puede renombrar con una grabación activa."
            )
        r.raise_for_status()
        return r.json()

    # -- ciclo de vida ------------------------------------------------------

    def close(self):
        """Cierra la sesión HTTP liberando recursos de red."""
        self._session.close()
