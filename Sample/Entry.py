# -*- encoding: utf-8 -*-

from typing import Optional
import Engine
from Source import Scenes


def setup():
    pass


def entry(windowHandle: Optional[int] = None):
    setup()
    Engine.System.setScene(Scenes.Title())
    while Engine.System.shouldLoop():
        pass


if __name__ == "__main__":
    entry()
