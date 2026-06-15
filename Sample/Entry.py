# -*- encoding: utf-8 -*-

import os
import sys
import builtins
import json
import logging
import configparser
import locale
import socket
import threading
import time
from typing import Sequence


_APP_NAME = "Ludork"
_DEFAULT_COMMAND_PORT = 2222
_DEFAULT_MESSAGE_PORT = 3333
_DEFAULT_MAIN_ITEMS: tuple[tuple[str, str], ...] = (
    ("script", "Entry.py"),
    ("language", "en_GB"),
    ("scale", "2.0"),
    ("framerate", "120"),
    ("verticalsync", "True"),
    ("musicon", "True"),
    ("soundon", "True"),
    ("voiceon", "True"),
    ("musicvolume", "100.00"),
    ("soundvolume", "100.00"),
    ("voicevolume", "100.00"),
)


def _getEnvPort(name: str, default: int) -> int:
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
        self._socket = None
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

    def _ensureSocket(self):
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
    def __init__(self, client: _EditorMessageClient, stream: str, fallback) -> None:
        self._client = client
        self._stream = stream
        self._fallback = fallback
        self._buffer = ""
        self.encoding = "utf-8"

    def write(self, text) -> int:
        s = str(text)
        self._buffer += s
        while "\n" in self._buffer:
            line, self._buffer = self._buffer.split("\n", 1)
            self._writeLine(line)
        return len(s)

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


def _redirectEditorStreams() -> None:
    client = _EditorMessageClient(port=_getEnvPort("LUDORK_MESSAGE_PORT", _DEFAULT_MESSAGE_PORT))
    sys.stdout = _SocketTextStream(client, "stdout", sys.__stdout__)
    sys.stderr = _SocketTextStream(client, "stderr", sys.__stderr__)


def _executeEditorCommand(cmd: str, env) -> None:
    cmd = cmd.strip()
    if not cmd:
        return
    if cmd == "help":
        env["help"]()
        return
    try:
        r = eval(cmd, env)
        if r is not None:
            print(str(r))
    except Exception:
        try:
            exec(cmd, env)
        except Exception as e:
            logging.error(f"[IPC] {e}")


def _commandSocketWorker(env, host: str = "127.0.0.1", port: int = _DEFAULT_COMMAND_PORT) -> None:
    server = None
    actualPort = int(port)
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
        logging.error(f"[IPC] command server failed on {host}:{port}+")
        return
    with server:
        print(f"[IPC] command server listening on {host}:{actualPort}")
        while True:
            conn, _addr = server.accept()
            with conn:
                stream = conn.makefile("r", encoding="utf-8", errors="replace", newline="\n")
                with stream:
                    for line in stream:
                        _executeEditorCommand(line, env)


def _getPlatformLanguage() -> str:
    try:
        lang, _encoding = locale.getdefaultlocale()
    except Exception:
        lang = None
    return lang or "en_GB"


def _resolveLanguage(language: str, languageKeys: Sequence[str]) -> str:
    resolved = str(language).strip()
    if resolved and resolved not in ("None", "en_GB"):
        return resolved if resolved in languageKeys else "en_GB"
    platformLang = _getPlatformLanguage()
    return platformLang if platformLang in languageKeys else "en_GB"


def _ensureMainIni(languageKeys: Sequence[str]) -> str:
    from Engine.Utils.Inner import getUserDataPath

    iniFilePath = os.path.join(getUserDataPath(_APP_NAME), "Main.ini")
    if os.path.exists(iniFilePath):
        return iniFilePath

    iniFile = configparser.ConfigParser()
    templatePath = os.path.abspath("./Main.ini")
    if os.path.exists(templatePath):
        iniFile.read(templatePath, encoding="utf-8")
    if "Main" not in iniFile:
        iniFile["Main"] = {}

    sec = iniFile["Main"]
    for key, value in _DEFAULT_MAIN_ITEMS:
        if not str(sec.get(key, "")).strip():
            sec[key] = value
    sec["language"] = _resolveLanguage(sec.get("language", ""), languageKeys)

    os.makedirs(os.path.dirname(os.path.abspath(iniFilePath)), exist_ok=True)
    with open(iniFilePath, "w", encoding="utf-8") as f:
        iniFile.write(f)
    return iniFilePath


def entry() -> None:
    r"""
    \brief entry.
    """
    logging.basicConfig(level=logging.INFO, format="%(levelname)s:%(message)s", force=True)

    import Engine
    import Global
    import Source

    if not os.environ.get("WINDOWHANDLE", None) is None:
        _redirectEditorStreams()

    def _stdinHelp(obj=None) -> None:
        if obj is None:
            logging.info("Use help(object) to print documentation in the console.")
            return

        import pydoc

        logging.info("%s", pydoc.render_doc(obj, renderer=pydoc.plaintext))

    if not os.environ.get("WINDOWHANDLE", None) is None:
        builtinEnv = dict(vars(builtins))
        builtinEnv["help"] = _stdinHelp
        commandEnv = {
            "__builtins__": builtinEnv,
            "Engine": Engine,
            "Scenes": Source.Scenes,
            "Global": Global,
            "help": _stdinHelp,
        }
        commandPort = _getEnvPort("LUDORK_COMMAND_PORT", _DEFAULT_COMMAND_PORT)
        t = threading.Thread(target=_commandSocketWorker, args=(commandEnv, "127.0.0.1", commandPort), daemon=True)
        t.start()

    try:
        import debugpy  # type: ignore

        debugpy.listen(("localhost", 2333))
    except ImportError:
        logging.info("Debugpy not found or this is a release build, not using it.")

    Engine.Locale.init("./Data/Locale")
    iniFilePath = _ensureMainIni(Engine.Locale.GetLocaleKeys())
    iniFile = configparser.ConfigParser()
    iniFile.read(iniFilePath, encoding="utf-8")
    Engine.NodeGraph.initLatent()
    Global.System.init(iniFile, iniFilePath)
    Global.System.bindSceneOperationThread()
    Global.System.setScene(Source.Scenes.Init())
    Source.System.init()

    while Global.System.shouldLoop():
        Global.System.applyPendingSceneReplace()
        scene = Global.System.getScene()
        if scene:
            scene.main()
    logging.info("Game exited successfully.")


if __name__ == "__main__":
    entry()
