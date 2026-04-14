# -*- encoding: utf-8 -*-

import os
import sys
import logging
import configparser
import threading


def entry():
    try:
        import debugpy
    except ImportError:
        pass
    import Engine
    import Global
    import Source

    def _stdinWorker():
        env = {"Engine": Engine, "Scenes": Source.Scenes, "Global": Global}
        while True:
            line = sys.stdin.readline()
            if not line:
                break
            cmd = line.strip()
            if not cmd:
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

    debugpy.listen(("localhost", 5678))
    iniFilePath = "./Main.ini"
    iniFile = configparser.ConfigParser()
    iniFile.read(iniFilePath, encoding="utf-8")
    Engine.Locale.init("./Data/Locale")
    Engine.NodeGraph.initLatent()
    Global.System.init(iniFile, iniFilePath)
    Global.System.setScene(Source.Scenes.Init())
    Source.System.init()

    while Global.System.shouldLoop():
        Global.System.getScene().main()
    Global.System.saveFPSHistory()
    print("Game exited successfully.")


if __name__ == "__main__":
    entry()
