# -*- encoding: utf-8 -*-

import os
from typing import Optional


def entry(windowHandle: Optional[int] = None):
    if windowHandle:
        os.environ["WINDOWHANDLE"] = str(windowHandle)
    import Engine
    from Source import Scenes

    print(Engine.Locale.getLocaleContent("zh_CN", "00001"))

    Engine.System.setScene(Scenes.Title())
    while Engine.System.shouldLoop():
        Engine.System.getScene().main()


if __name__ == "__main__":
    entry()
