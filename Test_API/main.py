import sys
import json
import os
from datetime import datetime

import requests
from PyQt6 import QtWidgets, QtCore, uic


# ---------------------------------------------------------------------------
# IpStore — persistencia de IPs en JSON
# ---------------------------------------------------------------------------
_IPS_FILE = os.path.join(os.path.dirname(__file__), "ips.json")


def _load_ips() -> list:
    try:
        with open(_IPS_FILE, "r", encoding="utf-8") as f:
            data = json.load(f)
            return data if isinstance(data, list) else []
    except (FileNotFoundError, json.JSONDecodeError):
        return []


def _save_ips(ips: list):
    with open(_IPS_FILE, "w", encoding="utf-8") as f:
        json.dump(ips, f, indent=2)


# ---------------------------------------------------------------------------
# ApiClient — encapsula comunicación HTTP con la API del dispositivo
# ---------------------------------------------------------------------------
class ApiClient:
    def __init__(self, ip: str, port: str):
        self.base_url = f"http://{ip}:{port}/api"
        self.timeout = 5

    def get_sensors(self) -> list:
        r = requests.get(f"{self.base_url}/sensors", timeout=self.timeout)
        r.raise_for_status()
        return r.json()

    def get_status(self) -> dict:
        r = requests.get(f"{self.base_url}/status", timeout=self.timeout)
        r.raise_for_status()
        return r.json()

    def get_logs(self) -> list:
        r = requests.get(f"{self.base_url}/logs", timeout=self.timeout)
        r.raise_for_status()
        return r.json()

    def download_log(self, filename: str, dest_path: str, progress_callback=None) -> bool:
        r = requests.get(
            f"{self.base_url}/download",
            params={"file": filename},
            timeout=30,
            stream=True,
        )
        if r.status_code == 409:
            raise RuntimeError("recording")
        r.raise_for_status()

        total = int(r.headers.get("Content-Length", 0))
        downloaded = 0
        with open(dest_path, "wb") as f:
            for chunk in r.iter_content(chunk_size=8192):
                f.write(chunk)
                downloaded += len(chunk)
                if progress_callback and total > 0:
                    progress_callback(int(downloaded * 100 / total))
        return True

    def clear_logs(self) -> dict:
        r = requests.get(f"{self.base_url}/clear-logs", timeout=self.timeout)
        r.raise_for_status()
        return r.json()


# ---------------------------------------------------------------------------
# DownloadWorker — hilo para descargar sin bloquear la UI
# ---------------------------------------------------------------------------
class DownloadWorker(QtCore.QThread):
    progress = QtCore.pyqtSignal(int)
    finished = QtCore.pyqtSignal(bool, str)  # (ok, message)

    def __init__(self, api: ApiClient, filename: str, dest_path: str):
        super().__init__()
        self.api = api
        self.filename = filename
        self.dest_path = dest_path

    def run(self):
        try:
            self.api.download_log(self.filename, self.dest_path, self.progress.emit)
            self.finished.emit(True, self.dest_path)
        except RuntimeError as e:
            if str(e) == "recording":
                self.finished.emit(False, "No se puede descargar durante grabación activa.")
            else:
                self.finished.emit(False, str(e))
        except Exception as e:
            self.finished.emit(False, str(e))

# ---------------------------------------------------------------------------
# PollingWorker — hilo de polling para sensores y status (no bloquea la UI)
# ---------------------------------------------------------------------------
class PollingWorker(QtCore.QThread):
    sensors_ready = QtCore.pyqtSignal(list)
    status_ready = QtCore.pyqtSignal(dict)
    poll_error = QtCore.pyqtSignal(str, str)  # (endpoint, message)

    def __init__(self, api: ApiClient):
        super().__init__()
        self.api = api
        self._running = False

    def stop(self):
        self._running = False

    def run(self):
        self._running = True
        cycle = 5  # dispara status en el primer ciclo
        while self._running:
            try:
                sensors = self.api.get_sensors()
                if self._running:
                    self.sensors_ready.emit(sensors)
            except Exception as e:
                if self._running:
                    self.poll_error.emit("/api/sensors", str(e))

            cycle += 1
            if cycle >= 5:
                cycle = 0
                try:
                    status = self.api.get_status()
                    if self._running:
                        self.status_ready.emit(status)
                except Exception as e:
                    if self._running:
                        self.poll_error.emit("/api/status", str(e))

            for _ in range(10):
                if not self._running:
                    return
                QtCore.QThread.msleep(100)


# ---------------------------------------------------------------------------
# LogDialog — diálogo no-modal para gestionar logs
# ---------------------------------------------------------------------------
class LogDialog(QtWidgets.QDialog):
    def __init__(self, api: ApiClient, parent=None):
        super().__init__(parent)
        ui_path = os.path.join(os.path.dirname(__file__), "log_dialog.ui")
        uic.loadUi(ui_path, self)

        self.api = api
        self._worker = None

        self.btn_cargar_logs.clicked.connect(self.cargar_logs)
        self.btn_descargar.clicked.connect(self.descargar_log)
        self.btn_limpiar_logs.clicked.connect(self.limpiar_logs)
        self.combo_logs.currentIndexChanged.connect(self._reset_download_state)

        # Auto-cargar lista al abrir
        self.cargar_logs()

    # -- slots --------------------------------------------------------------

    def cargar_logs(self):
        self.lbl_log_info.setText("Cargando…")
        self.btn_cargar_logs.setEnabled(False)
        QtWidgets.QApplication.processEvents()
        try:
            logs = self.api.get_logs()
            self.combo_logs.clear()
            self.combo_logs.addItems(logs)
            self.lbl_log_info.setText(f"{len(logs)} archivo(s) encontrado(s)")
            # Preseleccionar el más reciente (primero de la lista)
            if logs:
                self.combo_logs.setCurrentIndex(0)
        except Exception as e:
            self.lbl_log_info.setText(f"Error: {e}")
        finally:
            self.btn_cargar_logs.setEnabled(True)

    def descargar_log(self):
        filename = self.combo_logs.currentText()
        if not filename:
            self.lbl_log_info.setText("Selecciona un archivo primero.")
            return

        dest, _ = QtWidgets.QFileDialog.getSaveFileName(
            self, "Guardar log", filename, "Log files (*.log);;All files (*)"
        )
        if not dest:
            return

        self.progress_descarga.setVisible(True)
        self.progress_descarga.setValue(0)
        self.btn_descargar.setEnabled(False)

        self._worker = DownloadWorker(self.api, filename, dest)
        self._worker.progress.connect(self.progress_descarga.setValue)
        self._worker.finished.connect(self._on_download_finished)
        self._worker.start()

    def _on_download_finished(self, ok: bool, msg: str):
        self.btn_descargar.setEnabled(True)
        if ok:
            self.progress_descarga.setValue(100)
            self.lbl_log_info.setText(f"Descargado: {msg}")
        else:
            self.progress_descarga.setVisible(False)
            if "grabación" in msg.lower():
                QtWidgets.QMessageBox.warning(self, "Grabación activa", msg)
            else:
                self.lbl_log_info.setText(f"Error: {msg}")

    def _reset_download_state(self):
        self.progress_descarga.setVisible(False)
        self.progress_descarga.setValue(0)
        self.lbl_log_info.setText("")

    def limpiar_logs(self):
        resp = QtWidgets.QMessageBox.question(
            self,
            "Confirmar limpieza",
            "Se eliminarán todos los logs antiguos (se conserva el más reciente). ¿Continuar?",
            QtWidgets.QMessageBox.StandardButton.Yes | QtWidgets.QMessageBox.StandardButton.No,
        )
        if resp != QtWidgets.QMessageBox.StandardButton.Yes:
            return
        try:
            data = self.api.clear_logs()
            deleted = data.get("deleted", 0)
            self.lbl_log_info.setText(f"Eliminados: {deleted} archivo(s)")
            self.cargar_logs()
        except Exception as e:
            self.lbl_log_info.setText(f"Error: {e}")


# ---------------------------------------------------------------------------
# ApiTester — ventana principal
# ---------------------------------------------------------------------------
class ApiTester(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        ui_path = os.path.join(os.path.dirname(__file__), "interfaz.ui")
        uic.loadUi(ui_path, self)

        self.api: ApiClient | None = None
        self._connected = False
        self._log_dialog: LogDialog | None = None
        self._poller: PollingWorker | None = None

        # Cargar IPs guardadas en el combo
        for ip in _load_ips():
            self.combo_ip.addItem(ip)

        # Inicializar tabla de sensores
        for row in range(12):
            self.table_sensors.setItem(row, 0, QtWidgets.QTableWidgetItem(f"AG{row + 1}"))
            self.table_sensors.setItem(row, 1, QtWidgets.QTableWidgetItem("—"))
            self.table_sensors.setItem(row, 2, QtWidgets.QTableWidgetItem("—"))
        self.table_sensors.horizontalHeader().setSectionResizeMode(
            QtWidgets.QHeaderView.ResizeMode.Stretch
        )

        # Tabla: sin hover, sin scroll vertical, ajustar para 12 filas
        self.table_sensors.setStyleSheet(
            "QTableWidget::item:hover { background-color: transparent; }"
        )
        self.table_sensors.setVerticalScrollBarPolicy(
            QtCore.Qt.ScrollBarPolicy.ScrollBarAlwaysOff
        )
        self.table_sensors.verticalHeader().setSectionResizeMode(
            QtWidgets.QHeaderView.ResizeMode.Stretch
        )

        # Conectar señales
        self.btn_conectar.clicked.connect(self.conectar_desconectar)
        self.btn_comprobar_api.clicked.connect(self.comprobar_api)
        self.btn_gestor_logs.clicked.connect(self.abrir_gestor_logs)
        self.btn_save_ip.clicked.connect(self._save_current_ip)
        self.btn_remove_ip.clicked.connect(self._remove_current_ip)

    # -- helpers de consola -------------------------------------------------

    def _timestamp(self) -> str:
        return datetime.now().strftime("%H:%M:%S")

    def log_error(self, msg: str):
        self.console_errores.appendPlainText(f"[{self._timestamp()}] {msg}")
        self.tabConsole.setCurrentWidget(self.tab_console)

    def log_json(self, endpoint: str, data):
        text = json.dumps(data, indent=2, ensure_ascii=False)
        self.console_json.appendPlainText(f"[{self._timestamp()}] {endpoint}\n{text}\n")

    # -- conexión -----------------------------------------------------------

    def conectar_desconectar(self):
        if self._connected:
            self._disconnect()
        else:
            self._connect()

    def _save_current_ip(self):
        ip = self.combo_ip.currentText().strip()
        if not ip:
            return
        ips = _load_ips()
        if ip not in ips:
            ips.append(ip)
            _save_ips(ips)
            self.combo_ip.addItem(ip)
            self.statusBar().showMessage(f"IP '{ip}' guardada", 3000)
        else:
            self.statusBar().showMessage(f"IP '{ip}' ya está en la lista", 3000)

    def _remove_current_ip(self):
        ip = self.combo_ip.currentText().strip()
        if not ip:
            return
        ips = _load_ips()
        if ip in ips:
            ips.remove(ip)
            _save_ips(ips)
            idx = self.combo_ip.findText(ip)
            if idx >= 0:
                self.combo_ip.removeItem(idx)
            self.statusBar().showMessage(f"IP '{ip}' eliminada", 3000)
        else:
            self.statusBar().showMessage(f"IP '{ip}' no está en la lista guardada", 3000)

    def _connect(self):
        ip = self.combo_ip.currentText().strip()
        port = self.txt_port.text().strip() or "80"
        if not ip:
            self.log_error("Introduce una dirección IP.")
            self.btn_conectar.setChecked(False)
            return

        self.api = ApiClient(ip, port)
        self.lbl_red.setText("● Red: Comprobando…")
        self.lbl_red.setStyleSheet("color: orange; font-weight: bold;")
        QtWidgets.QApplication.processEvents()

        try:
            status = self.api.get_status()
            self.log_json("/api/status", status)
        except Exception as e:
            self.log_error(f"No se pudo conectar a {ip}:{port} — {e}")
            self.lbl_red.setText("● Red: Error")
            self.lbl_red.setStyleSheet("color: red; font-weight: bold;")
            self.btn_conectar.setChecked(False)
            self.api = None
            return

        # Auto-guardar IP al conectar con éxito
        ips = _load_ips()
        if ip not in ips:
            ips.append(ip)
            _save_ips(ips)
            self.combo_ip.addItem(ip)

        self._connected = True
        self.btn_conectar.setText("Desconectar")
        self.btn_comprobar_api.setEnabled(True)
        self.lbl_red.setText("● Red: Conectado")
        self.lbl_red.setStyleSheet("color: green; font-weight: bold;")
        self.statusBar().showMessage(f"Conectado a {ip}:{port}", 3000)

        self._update_status_labels(status)

        # Arrancar polling en hilo separado
        self._poller = PollingWorker(self.api)
        self._poller.sensors_ready.connect(self._on_sensors)
        self._poller.status_ready.connect(self._on_status)
        self._poller.poll_error.connect(self._on_poll_error)
        self._poller.start()

    def _disconnect(self):
        if self._poller:
            self._poller.stop()
            self._poller.wait(2000)
            self._poller = None
        self._connected = False
        self.api = None
        self.btn_conectar.setText("Conectar")
        self.btn_conectar.setChecked(False)
        self.btn_comprobar_api.setEnabled(False)
        self.lbl_red.setText("● Red: Desconectado")
        self.lbl_red.setStyleSheet("color: red; font-weight: bold;")
        self.lbl_status.setText("Status: —")
        self.lbl_logging.setText("Grabación: —")
        self.lbl_logging.setStyleSheet("")
        self.statusBar().showMessage("Desconectado", 3000)

    # -- slots del poller ---------------------------------------------------

    def _on_sensors(self, sensors: list):
        for i, s in enumerate(sensors):
            rpm = s.get("rpm", 0)
            hz = s.get("hz", 0)
            self.table_sensors.item(i, 1).setText(f"{rpm:.2f}")
            self.table_sensors.item(i, 2).setText(f"{hz:.2f}")
            color = QtCore.Qt.GlobalColor.darkGreen if rpm > 0 else QtCore.Qt.GlobalColor.black
            self.table_sensors.item(i, 1).setForeground(color)
            self.table_sensors.item(i, 2).setForeground(color)

    def _on_status(self, status: dict):
        self._update_status_labels(status)
        self.log_json("/api/status", status)

    def _on_poll_error(self, endpoint: str, msg: str):
        self.log_error(f"{endpoint} — {msg}")
        if endpoint == "/api/status":
            self._disconnect()

    def _update_status_labels(self, status: dict):
        self.lbl_status.setText(f"Status: {status.get('status', '?')}")
        logging_active = status.get("logging", False)
        if logging_active:
            self.lbl_logging.setText("Grabación: ACTIVA")
            self.lbl_logging.setStyleSheet("color: red; font-weight: bold;")
        else:
            self.lbl_logging.setText("Grabación: Inactiva")
            self.lbl_logging.setStyleSheet("color: green;")

    # -- acciones manuales --------------------------------------------------

    def comprobar_api(self):
        self.statusBar().showMessage("Comprobando API…")
        QtWidgets.QApplication.processEvents()
        try:
            status = self.api.get_status()
            self._update_status_labels(status)
            self.log_json("/api/status", status)
            s = status.get('status', '?')
            logging = "ACTIVA" if status.get('logging', False) else "Inactiva"
            self.statusBar().showMessage(f"API OK — Status: {s} | Grabación: {logging}", 4000)
        except Exception as e:
            self.log_error(f"Comprobar API — {e}")
            self.statusBar().showMessage(f"Error al comprobar API", 4000)

    def abrir_gestor_logs(self):
        if not self.api:
            self.log_error("Conéctate primero para gestionar logs.")
            return
        if self._log_dialog is None or not self._log_dialog.isVisible():
            self._log_dialog = LogDialog(self.api, self)
        self._log_dialog.show()
        self._log_dialog.raise_()
        self._log_dialog.activateWindow()
        self.statusBar().showMessage("Gestor de logs abierto", 3000)


# ---------------------------------------------------------------------------
# Punto de entrada
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    ventana = ApiTester()
    ventana.show()
    sys.exit(app.exec())
