# -*- encoding: utf-8 -*-
r"""\brief Game system bootstrap: initialises engine, loads data, and starts the game loop."""

import os
from typing import List, Optional
import Engine
from Engine import (
    Vector2u,
    Font,
    Image,
    Cursor,
    RenderWindow,
    State,
    VideoMode,
    Style,
    ContextSettings,
)
from Engine import UI
from Engine.Utils import File
from Engine.Utils.Inner import IS_IOS_PLATFORM, warnIosShaderSkippedOnce
from Global import Manager, GameMap
from Global import System as GlobalSystem


class System:
    r"""\brief Game system bootstrap that initialises engine subsystems."""

    _title: str
    _fonts: List[Font] = []
    _fontSize: int = 32
    _icon: Image
    _cursor: Optional[Cursor] = None
    _windowskinName: str
    _coverOpaqueAlpha: int = 0
    _startMap: str = ""
    _startPos: Vector2u
    _cursorSE: str = ""
    _decisionSE: str = ""
    _cancelSE: str = ""
    _buzzerSE: str = ""
    _shopSE: str = ""
    _saveSE: str = ""
    _loadSE: str = ""
    _gateSE: str = ""
    _stairSE: str = ""
    _getSE: str = ""
    _equipSE: str = ""
    _titleBGM: str = ""

    @classmethod
    def init(cls) -> None:
        r"""\brief Initialise the game system from configuration files.

        Loads system.json, sets up the window, fonts, cursor, and global settings.
        """
        systemData = File.getJSONData("./Data/Configs/System.json")
        cls._title = systemData["title"]["value"]
        size = systemData["gameSize"]["value"]
        gameSize = Vector2u(size[0], size[1])
        cls._fonts = [Manager.loadFont(font) for font in systemData["fonts"]["value"]]
        cls._fontSize = systemData["fontSize"]["value"]
        cls._icon = Image(os.path.join("./Assets", systemData["icon"]["base"], systemData["icon"]["value"]))
        cls._cursor = None
        cursorPath = os.path.join("./Assets", systemData["cursor"]["base"], systemData["cursor"]["value"])
        if IS_IOS_PLATFORM:
            warnIosShaderSkippedOnce(
                "Source.System.cursor",
                "iOS: custom mouse cursor is not supported; skipped",
            )
        elif cursorPath and os.path.exists(cursorPath):
            try:
                cursorImage = Image(cursorPath)
                cls._cursor = Cursor(cursorImage.getPixelsArray(), cursorImage.getSize(), Vector2u(0, 0))
            except RuntimeError:
                warnIosShaderSkippedOnce(
                    "Source.System.cursorLoadFailed",
                    f"Failed to create cursor from pixels; skipped. Path: {cursorPath}",
                )
        cls._windowskinName = systemData["windowskinName"]["value"]
        Engine.CellSize = systemData["cellSize"]["value"]
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
            contextSettings = ContextSettings(antiAliasingLevel=8)
            window = RenderWindow(int(handle), settings=contextSettings)
            windowSize = window.getSize()
            scale = min(windowSize.x / gameSize.x, windowSize.y / gameSize.y)
            GlobalSystem.setScale(scale)
            if handle:
                from Engine import Input as EngineInput

                EngineInput.setUseInjectedMouseOnly(True)
        elif IS_IOS_PLATFORM:
            iosContext = ContextSettings(antiAliasingLevel=0)
            window = RenderWindow(
                VideoMode(realSize),
                cls._title,
                Style.Default,
                State.Windowed,
                settings=iosContext,
            )
            windowSize = window.getSize()
            scale = min(windowSize.x / gameSize.x, windowSize.y / gameSize.y)
            GlobalSystem.setScale(scale)
        else:
            contextSettings = ContextSettings(antiAliasingLevel=8)
            window = RenderWindow(
                VideoMode(realSize),
                cls._title,
                Style.Titlebar | Style.Close,
                State.Windowed,
                settings=contextSettings,
            )
        if IS_IOS_PLATFORM:
            try:
                window.setIcon(cls._icon)
            except Exception:
                pass
        else:
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
        audioData = File.getJSONData("./Data/Configs/Audio.json")
        cls._cursorSE = audioData["cursorSE"]["value"]
        cls._decisionSE = audioData["decisionSE"]["value"]
        cls._cancelSE = audioData["cancelSE"]["value"]
        cls._buzzerSE = audioData["buzzerSE"]["value"]
        cls._shopSE = audioData["shopSE"]["value"]
        cls._saveSE = audioData["saveSE"]["value"]
        cls._loadSE = audioData["loadSE"]["value"]
        cls._gateSE = audioData["gateSE"]["value"]
        cls._stairSE = audioData["stairSE"]["value"]
        cls._getSE = audioData["getSE"]["value"]
        cls._equipSE = audioData["equipSE"]["value"]
        cls._titleBGM = audioData["titleBGM"]["value"]

    @classmethod
    def getTitle(cls) -> str:
        r"""\brief Get the game window title.

        - \return The window title string.
        """
        return cls._title

    @classmethod
    def getFonts(cls) -> List[Font]:
        r"""\brief Get the list of loaded fonts.

        - \return A list of Font objects.
        """
        return cls._fonts

    @classmethod
    def getFontSize(cls) -> int:
        r"""\brief Get the default font size.

        - \return The font size in pixels.
        """
        return cls._fontSize

    @classmethod
    def getWindowskinName(cls) -> str:
        r"""\brief Get the window skin texture name.

        - \return The windowskin name.
        """
        return cls._windowskinName

    @classmethod
    def setWindowskinName(cls, name: str) -> None:
        r"""\brief Set the window skin texture name.

        - \param name The new windowskin name.
        """
        cls._windowskinName = name

    @classmethod
    def getStartMap(cls) -> str:
        r"""\brief Get the starting map path.

        - \return The start map path.
        """
        return cls._startMap

    @classmethod
    def getStartPos(cls) -> Vector2u:
        r"""\brief Get the starting position on the map.

        - \return The start position.
        """
        return cls._startPos

    @classmethod
    def getCursorSE(cls) -> str:
        r"""\brief Get the cursor sound effect filename.

        - \return The cursor SE filename.
        """
        return cls._cursorSE

    @classmethod
    def getDecisionSE(cls) -> str:
        r"""\brief Get the decision sound effect filename.

        - \return The decision SE filename.
        """
        return cls._decisionSE

    @classmethod
    def getCancelSE(cls) -> str:
        r"""\brief Get the cancel sound effect filename.

        - \return The cancel SE filename.
        """
        return cls._cancelSE

    @classmethod
    def getBuzzerSE(cls) -> str:
        r"""\brief Get the buzzer sound effect filename.

        - \return The buzzer SE filename.
        """
        return cls._buzzerSE

    @classmethod
    def getShopSE(cls) -> str:
        r"""\brief Get the shop sound effect filename.

        - \return The shop SE filename.
        """
        return cls._shopSE

    @classmethod
    def getSaveSE(cls) -> str:
        r"""\brief Get the save sound effect filename.

        - \return The save SE filename.
        """
        return cls._saveSE

    @classmethod
    def getLoadSE(cls) -> str:
        r"""\brief Get the load sound effect filename.

        - \return The load SE filename.
        """
        return cls._loadSE

    @classmethod
    def getGateSE(cls) -> str:
        r"""\brief Get the gate sound effect filename.

        - \return The gate SE filename.
        """
        return cls._gateSE

    @classmethod
    def getStairSE(cls) -> str:
        r"""\brief Get the stair sound effect filename.

        - \return The stair SE filename.
        """
        return cls._stairSE

    @classmethod
    def getGetSE(cls) -> str:
        r"""\brief Get the item get sound effect filename.

        - \return The get SE filename.
        """
        return cls._getSE

    @classmethod
    def getEquipSE(cls) -> str:
        r"""\brief Get the equip sound effect filename.

        - \return The equip SE filename.
        """
        return cls._equipSE

    @classmethod
    def getTitleBGM(cls) -> str:
        r"""\brief Get the title screen BGM filename.

        - \return The title BGM filename.
        """
        return cls._titleBGM
