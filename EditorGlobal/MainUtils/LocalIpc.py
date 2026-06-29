# -*- encoding: utf-8 -*-

from __future__ import annotations

import json
import queue
import socket
import threading
import time
from typing import Iterable, Optional
from PyQt5 import QtCore


DEFAULT_COMMAND_PORT = 2222
DEFAULT_MESSAGE_PORT = 3333
IPC_HOST = "127.0.0.1"


def FindAvailableLocalPort(startPort: int, host: str = IPC_HOST, excludedPorts: Iterable[int] = ()) -> int:
    excluded = set(excludedPorts)
    for port in range(int(startPort), 65536):
        if port in excluded:
            continue
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                sock.bind((host, port))
            return port
        except OSError:
            continue
    raise OSError(f"No available local port from {startPort}")


class LocalCommandClient:
    def __init__(self, host: str = IPC_HOST, port: int = DEFAULT_COMMAND_PORT) -> None:
        self._host = host
        self._port = port
        self._queue: queue.Queue[Optional[str]] = queue.Queue(maxsize=4096)
        self._stop = threading.Event()
        self._socket: Optional[socket.socket] = None
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def sendLine(self, line: str) -> None:
        if not line.endswith("\n"):
            line += "\n"
        try:
            self._queue.put_nowait(line)
        except queue.Full:
            pass

    def close(self) -> None:
        self._stop.set()
        try:
            self._queue.put_nowait(None)
        except queue.Full:
            pass
        self._closeSocket()
        if self._thread.is_alive():
            self._thread.join(timeout=0.5)

    def _run(self) -> None:
        while not self._stop.is_set():
            try:
                line = self._queue.get(timeout=0.1)
            except queue.Empty:
                continue
            if line is None:
                break
            self._sendLine(line)

    def _sendLine(self, line: str) -> None:
        payload = line.encode("utf-8")
        while not self._stop.is_set():
            sock = self._ensureSocket()
            if sock is None:
                return
            try:
                sock.sendall(payload)
                return
            except OSError:
                self._closeSocket()
                time.sleep(0.05)

    def _ensureSocket(self) -> Optional[socket.socket]:
        if self._socket is not None:
            return self._socket
        deadline = time.monotonic() + 3.0
        while not self._stop.is_set() and time.monotonic() < deadline:
            try:
                sock = socket.create_connection((self._host, self._port), timeout=0.2)
                sock.settimeout(None)
                self._socket = sock
                return sock
            except OSError:
                time.sleep(0.05)
        return None

    def _closeSocket(self) -> None:
        sock = self._socket
        self._socket = None
        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass


class LocalMessageServer(QtCore.QThread):
    LINE_RECEIVED = QtCore.pyqtSignal(str, str)
    PERFORMANCE_SAMPLE = QtCore.pyqtSignal(float, float)

    _PERFORMANCE_SAMPLE_PREFIX = "__LUDORK_PERF__:"

    def __init__(self, host: str = IPC_HOST, port: int = DEFAULT_MESSAGE_PORT) -> None:
        super().__init__()
        self._host = host
        self._port = port
        self._stop = threading.Event()
        self._server: Optional[socket.socket] = None
        self._connection: Optional[socket.socket] = None

    @property
    def port(self) -> int:
        return self._port

    def stop(self) -> None:
        self._stop.set()
        self._closeConnection()
        server = self._server
        self._server = None
        if server is not None:
            try:
                server.close()
            except OSError:
                pass

    def run(self) -> None:
        try:
            server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind((self._host, self._port))
            server.listen(1)
            server.settimeout(0.2)
            self._server = server
        except OSError as e:
            self.LINE_RECEIVED.emit(f"Local IPC server failed on {self._host}:{self._port}: {e}", "ERROR")
            return

        while not self._stop.is_set():
            try:
                conn, _addr = server.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            self._connection = conn
            try:
                self._readConnection(conn)
            finally:
                self._closeConnection()

    def _readConnection(self, conn: socket.socket) -> None:
        conn.settimeout(0.2)
        buffer = b""
        while not self._stop.is_set():
            try:
                chunk = conn.recv(4096)
            except socket.timeout:
                continue
            except OSError:
                break
            if not chunk:
                break
            buffer += chunk
            while b"\n" in buffer:
                raw, buffer = buffer.split(b"\n", 1)
                self._handleLine(raw.decode("utf-8", errors="replace").rstrip("\r"))
        if buffer:
            self._handleLine(buffer.decode("utf-8", errors="replace").rstrip("\r"))

    def _handleLine(self, line: str) -> None:
        try:
            payload = json.loads(line)
        except json.JSONDecodeError:
            self.LINE_RECEIVED.emit(line, "INFO")
            return
        if not isinstance(payload, dict):
            self.LINE_RECEIVED.emit(str(payload), "INFO")
            return
        text = str(payload.get("text", ""))
        if text.startswith(self._PERFORMANCE_SAMPLE_PREFIX):
            raw = text[len(self._PERFORMANCE_SAMPLE_PREFIX) :].strip()
            try:
                perf = json.loads(raw)
                self.PERFORMANCE_SAMPLE.emit(float(perf.get("fps", 0.0)), float(perf.get("memory", 0.0)))
            except Exception:
                self.LINE_RECEIVED.emit(text, "INFO")
            return
        level = str(payload.get("level", "")).upper()
        if not level:
            level = self._detectLevel(text)
        self.LINE_RECEIVED.emit(text, level)

    def _detectLevel(self, text: str) -> str:
        t = text.lower()
        if "traceback" in t or "error" in t or "exception" in t or "critical" in t or t.startswith("fatal"):
            return "ERROR"
        if "warning" in t or t.startswith("warn") or "deprecated" in t:
            return "WARNING"
        if "debug" in t:
            return "DEBUG"
        return "INFO"

    def _closeConnection(self) -> None:
        conn = self._connection
        self._connection = None
        if conn is not None:
            try:
                conn.close()
            except OSError:
                pass
