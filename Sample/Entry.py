# -*- encoding: utf-8 -*-

import os
import sys
import builtins
import logging
import configparser
import locale
import threading
from typing import Sequence


_APP_NAME = "Ludork"
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

    import Engine
    import Global
    import Source

    def _stdinHelp(obj=None) -> None:
        if obj is None:
            print("Use help(object) to print documentation in the console.")
            return

        import pydoc

        print(pydoc.render_doc(obj, renderer=pydoc.plaintext))

    def _stdinWorker() -> None:
        builtinEnv = dict(vars(builtins))
        builtinEnv["help"] = _stdinHelp
        env = {
            "__builtins__": builtinEnv,
            "Engine": Engine,
            "Scenes": Source.Scenes,
            "Global": Global,
            "help": _stdinHelp,
        }
        while True:
            line = sys.stdin.readline()
            if not line:
                break
            cmd = line.strip()
            if not cmd:
                continue
            if cmd == "help":
                _stdinHelp()
                continue
            try:
                r = eval(cmd, env)
                if r is not None:
                    print(str(r))
            except Exception:
                try:
                    exec(cmd, env)
                except Exception as e:
                    logging.error(f"[STDIN] {e}")

    if not os.environ.get("WINDOWHANDLE", None) is None:
        t = threading.Thread(target=_stdinWorker, daemon=True)
        t.start()

    try:
        import debugpy  # type: ignore

        debugpy.listen(("localhost", 2333))
    except ImportError:
        print("Debugpy not found or this is a release build, not using it.")

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
    print("Game exited successfully.")


if __name__ == "__main__":
    entry()
