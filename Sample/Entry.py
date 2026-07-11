# -*- encoding: utf-8 -*-

import os
import logging
import configparser
import locale
from typing import Sequence


_APP_NAME = "Ludork"


def _registerAppName() -> None:
    from Engine.Utils.Inner import setAppName

    setAppName(_APP_NAME)


_registerAppName()
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
    import sys

    from Engine.Utils.Inner import getMainIniPath

    iniFilePath = getMainIniPath()
    if os.path.exists(iniFilePath):
        return iniFilePath

    iniFile = configparser.ConfigParser()
    if sys.platform == "win32":
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

    if os.environ.get("WINDOWHANDLE") is not None:
        import builtins
        import EntryEditorIpc

        EntryEditorIpc.redirectEditorStreams()

        def _stdinHelp(obj: object | None = None) -> None:
            if obj is None:
                logging.info("Use help(object) to print documentation in the console.")
                return

            import pydoc

            logging.info("%s", pydoc.render_doc(obj, renderer=pydoc.plaintext))

        builtinEnv = dict(vars(builtins))
        builtinEnv["help"] = _stdinHelp
        commandEnv = {
            "__builtins__": builtinEnv,
            "Engine": Engine,
            "Scenes": Source.Scenes,
            "Global": Global,
            "help": _stdinHelp,
        }
        commandPort = EntryEditorIpc.getEnvPort("LUDORK_COMMAND_PORT", 2222)
        EntryEditorIpc.startCommandSocket(commandEnv, commandPort)

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
