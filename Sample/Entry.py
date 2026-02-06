# -*- encoding: utf-8 -*-

import os
import sys
import logging
import configparser
import threading


def entry():
    import Engine
    from Source import Scenes

    def _stdinWorker():
        env = {"Engine": Engine, "Scenes": Scenes, "System": Engine.System}
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

    iniFilePath = "./Main.ini"
    iniFile = configparser.ConfigParser()
    iniFile.read(iniFilePath, encoding="utf-8")
    Engine.Locale.init("./Data/Locale")
    Engine.System.init(iniFile, iniFilePath)
    Engine.System.setScene(Scenes.Init())

    while Engine.System.shouldLoop():
        Engine.System.getScene().main()
    Engine.System.saveFPSHistory()


if __name__ == "__main__":
    entry()
