# -*- encoding: utf-8 -*-

import os
from typing import List, Optional, Dict, Any
from Engine import (
    Vector2u,
    Font,
    Image,
    Cursor,
    SetCellSize,
    RenderWindow,
    State,
    VideoMode,
    Style,
    ContextSettings,
)
from Engine import UI
from Engine.Utils import File
from Global import Manager, GameMap
from Global import System as GlobalSystem


class System:
    _title: str = None
    _fonts: List[Font] = []
    _fontSize: int = 32
    _icon: Image = None
    _cursor: Optional[Cursor] = None
    _windowskinName: str = None
    _coverOpaqueAlpha: int = 0
    _startMap: str = ""
    _startPos: Vector2u = None
    _variables: Dict[str, Any] = {}

    @classmethod
    def init(cls) -> None:
        systemData = File.getJSONData("./Data/Configs/System.json")
        cls._title = systemData["title"]["value"]
        size = systemData["gameSize"]["value"]
        gameSize = Vector2u(size[0], size[1])
        cls._fonts = [Manager.loadFont(font) for font in systemData["fonts"]["value"]]
        cls._fontSize = systemData["fontSize"]["value"]
        cls._icon = Image(os.path.join("./Assets", systemData["icon"]["base"], systemData["icon"]["value"]))
        cursorPath = os.path.join("./Assets", systemData["cursor"]["base"], systemData["cursor"]["value"])
        if cursorPath and os.path.exists(cursorPath):
            cursorImage = Image(cursorPath)
            cls._cursor = Cursor(cursorImage.getPixelsArray(), cursorImage.getSize(), Vector2u(0, 0))
        cls._windowskinName = systemData["windowskinName"]["value"]
        SetCellSize(systemData["cellSize"]["value"])
        coverOpaqueAlpha = systemData["coverOpaqueAlpha"]["value"]
        cls._startMap = systemData["startMap"]["value"]
        cls._startPos = Vector2u(*systemData["startPos"]["value"])
        realSize = Vector2u(
            int(gameSize.x * GlobalSystem.getScale()),
            int(gameSize.y * GlobalSystem.getScale()),
        )
        handle: Optional[str] = os.environ.get("WINDOWHANDLE")
        individual: Optional[str] = os.environ.get("INDIVIDUAL")
        if handle and individual != "True":
            window = RenderWindow(int(handle), settings=ContextSettings(antiAliasingLevel=8))
            windowSize = window.getSize()
            scale = min(windowSize.x / gameSize.x, windowSize.y / gameSize.y)
            GlobalSystem.setScale(scale)
        else:
            window = RenderWindow(
                VideoMode(realSize),
                cls._title,
                Style.Titlebar | Style.Close,
                State.Windowed,
                settings=ContextSettings(antiAliasingLevel=8),
            )
        window.setIcon(cls._icon)
        if cls._cursor:
            window.setMouseCursor(cls._cursor)
        GlobalSystem.setGameSize(gameSize)
        GlobalSystem.setDebugMode(handle is not None)
        GlobalSystem.setShowFPSGraph(os.environ.get("SHOWFPSGRAPH") == "True")
        GlobalSystem.initWindow(window)
        GlobalSystem.initCanvas(window.getSize())
        UI.DefaultFont = cls._fonts[0]
        UI.DefaultFontSize = cls._fontSize
        UI.DefaultWindowskinName = cls._windowskinName
        GameMap.DefaultCoverAlpha = coverOpaqueAlpha

    @classmethod
    def getTitle(cls) -> str:
        return cls._title

    @classmethod
    def getFonts(cls) -> List[Font]:
        return cls._fonts

    @classmethod
    def getFontSize(cls) -> int:
        return cls._fontSize

    @classmethod
    def getWindowskinName(cls) -> str:
        return cls._windowskinName

    @classmethod
    def setWindowskinName(cls, name: str) -> None:
        cls._windowskinName = name

    @classmethod
    def getStartMap(cls) -> str:
        return cls._startMap

    @classmethod
    def getStartPos(cls) -> Vector2u:
        return cls._startPos

    @classmethod
    def getVariable(cls, name: str) -> Any:
        if not name in cls._variables:
            return None
        return cls._variables[name]

    @classmethod
    def setVariable(cls, name: str, value: Any) -> None:
        cls._variables[name] = value
