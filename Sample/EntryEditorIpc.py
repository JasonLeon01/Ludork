# -*- encoding: utf-8 -*-

r"""\brief Provides editor-only stream forwarding and command IPC."""

import os
import json
import logging
import socket
import sys
import threading
import time
from typing import Any


_DEFAULT_COMMAND_PORT = 2222
_DEFAULT_MESSAGE_PORT = 3333


def getEnvPort(name: str, default: int) -> int:
    r"""\brief Read a validated port from the environment.

    - \param name - Environment variable name.
    - \param default - Port used when the value is invalid or missing.
    - \return A valid TCP port number.
    """
    raw = os.environ.get(name, "")
    try:
        port = int(raw)
    except (TypeError, ValueError):
        return default
    return port if 0 < port <= 65535 else default


class _EditorMessageClient:
    def __init__(self, host: str = "127.0.0.1", port: int = _DEFAULT_MESSAGE_PORT) -> None:
        self._host = host
        self._port = port
        self._socket: socket.socket | None = None
        self._lock = threading.Lock()
        self._nextRetryTime = 0.0

    def sendLine(self, stream: str, text: str) -> bool:
        payload = json.dumps({"type": "line", "stream": stream, "text": text}, ensure_ascii=False)
        data = (payload + "\n").encode("utf-8")
        with self._lock:
            sock = self._ensureSocket()
            if sock is None:
                return False
            try:
                sock.sendall(data)
                return True
            except OSError:
                self._closeSocket()
                return False

    def _ensureSocket(self) -> socket.socket | None:
        if self._socket is not None:
            return self._socket
        now = time.monotonic()
        if now < self._nextRetryTime:
            return None
        self._nextRetryTime = now + 0.2
        try:
            self._socket = socket.create_connection((self._host, self._port), timeout=0.05)
            self._socket.settimeout(None)
        except OSError:
            self._socket = None
        return self._socket

    def _closeSocket(self) -> None:
        sock = self._socket
        self._socket = None
        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass


class _SocketTextStream:
    def __init__(self, client: _EditorMessageClient, stream: str, fallback: Any) -> None:
        self._client = client
        self._stream = stream
        self._fallback = fallback
        self._buffer = ""
        self.encoding = "utf-8"

    def write(self, text: str) -> int:
        self._buffer += text
        while "\n" in self._buffer:
            line, self._buffer = self._buffer.split("\n", 1)
            self._writeLine(line)
        return len(text)

    def flush(self) -> None:
        if self._buffer:
            line = self._buffer
            self._buffer = ""
            self._writeLine(line)

    def isatty(self) -> bool:
        return False

    def _writeLine(self, line: str) -> None:
        if self._client.sendLine(self._stream, line):
            return
        try:
            self._fallback.write(line + "\n")
            self._fallback.flush()
        except Exception:
            pass


def redirectEditorStreams() -> None:
    r"""\brief Redirect standard streams to the editor message server."""
    client = _EditorMessageClient(port=getEnvPort("LUDORK_MESSAGE_PORT", _DEFAULT_MESSAGE_PORT))
    sys.stdout = _SocketTextStream(client, "stdout", sys.__stdout__)
    sys.stderr = _SocketTextStream(client, "stderr", sys.__stderr__)


def _executeEditorCommand(cmd: str, environment: dict[str, Any]) -> None:
    cmd = cmd.strip()
    if not cmd:
        return
    if cmd == "help":
        environment["help"]()
        return
    try:
        result = eval(cmd, environment)
        if result is not None:
            print(str(result))
    except Exception:
        try:
            exec(cmd, environment)
        except Exception as exception:
            logging.error("[IPC] %s", exception)


def _commandSocketWorker(environment: dict[str, Any], host: str, port: int) -> None:
    server: socket.socket | None = None
    actualPort = port
    for candidate in range(actualPort, 65536):
        try:
            server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind((host, candidate))
            server.listen(1)
            actualPort = candidate
            break
        except OSError:
            if server is not None:
                try:
                    server.close()
                except OSError:
                    pass
            server = None
    if server is None:
        logging.error("[IPC] command server failed on %s:%s+", host, port)
        return
    with server:
        print(f"[IPC] command server listening on {host}:{actualPort}")
        while True:
            connection, _address = server.accept()
            with connection:
                stream = connection.makefile("r", encoding="utf-8", errors="replace", newline="\n")
                with stream:
                    for line in stream:
                        _executeEditorCommand(line, environment)


def startCommandSocket(environment: dict[str, Any], port: int) -> None:
    r"""\brief Start the editor command server on a background thread.

    - \param environment - Globals available to editor-issued commands.
    - \param port - Preferred local TCP port.
    """
    thread = threading.Thread(
        target=_commandSocketWorker,
        args=(environment, "127.0.0.1", port),
        daemon=True,
    )
    thread.start()
