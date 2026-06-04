# -*- encoding: utf-8 -*-

import os
import sys
import builtins
import logging
import configparser
import threading


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

    iniFilePath = "./Main.ini"
    iniFile = configparser.ConfigParser()
    iniFile.read(iniFilePath, encoding="utf-8")
    Engine.Locale.init("./Data/Locale")
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
