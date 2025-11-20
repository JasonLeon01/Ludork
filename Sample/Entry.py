# -*- encoding: utf-8 -*-

import os
import sys
from typing import Optional
import configparser


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
    while Engine.System.shouldLoop():
        Engine.System.getScene().main()


if __name__ == "__main__":
    if len(sys.argv) > 1:
        entry(int(sys.argv[1]))
    else:
        entry()
