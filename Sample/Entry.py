# -*- encoding: utf-8 -*-

import os
import sys
import logging
from typing import Optional
import configparser
import threading


def entry(windowHandle: Optional[int] = None):
    if windowHandle:
        os.environ["WINDOWHANDLE"] = str(windowHandle)
    import Engine
    from Source import Scenes

    iniFilePath = "./Main.ini"
    iniFile = configparser.ConfigParser()
    iniFile.read(iniFilePath, encoding="utf-8")
    Engine.Locale.init("./Assets/Locale")
    Engine.System.init(iniFile, iniFilePath)
    Engine.System.setScene(Scenes.Title())

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

    t = threading.Thread(target=_stdinWorker, daemon=True)
    t.start()
    while Engine.System.shouldLoop():
        Engine.System.getScene().main()


if __name__ == "__main__":
    if len(sys.argv) > 1:
        entry(int(sys.argv[1]))
    else:
        entry()
