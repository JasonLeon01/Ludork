# -*- encoding: utf-8 -*-

import configparser
import json
import Engine.Manager as Manager
from typing import Any, Dict, List
from Engine import (
    Color,
    RenderWindow,
    RenderTexture,
    View,
    Vector2u,
    Vector2f,
    Font,
    Image,
    Sprite,
    Drawable,
    VideoMode,
    SetCellSize,
    GetGameRunning,
    Style,
    ContextSettings,
)
from Engine.Gameplay import SceneBase
from Engine.Utils import Math, File, Render


class System:
    class Config:
        FontSize: int
        WindowColor: Color

    _window: RenderWindow = None
    _canvas: RenderTexture = None
    _title: str = None
    _fonts: List[Font] = []
    _gameSize: Vector2u = None
    _icon: Image = None
    _windowskinName: str = None
    _mainScript: str = None
    _scale: float = 1.0
    _frameRate: int = 60
    _verticalSync: bool = False
    _musicOn: bool = True
    _soundOn: bool = True
    _voiceOn: bool = True
    _musicVolume: float = 100
    _soundVolume: float = 100
    _voiceVolume: float = 100
    _scene: SceneBase = None
    _variables: Dict[str, Any] = {}

    @classmethod
    def init(cls, data: configparser.ConfigParser) -> None:
        systemData = File.getJSONData("./Data/Configs/System.json")
        cls._title = systemData["title"]
        cls._gameSize = Vector2u(systemData["gameSize"][0], systemData["gameSize"][1])
        cls._fonts = [Manager.loadFont(font) for font in systemData["fonts"]]
        cls.Config.FontSize = systemData["fontSize"]
        cls._icon = Image(f"./Assets/System/{systemData['icon']}")
        cls._windowskinName = systemData["windowskinName"]
        r, g, b, a = systemData["windowColor"]
        cls.Config.WindowColor = Color(r, g, b, a)
        SetCellSize(systemData["cellSize"])
        cls._mainScript = data["script"]
        cls._scale = data.getfloat("scale")
        cls._frameRate = data.getint("frameRate")
        cls._verticalSync = data.getboolean("verticalSync")
        cls._musicOn = data.getboolean("musicOn")
        cls._soundOn = data.getboolean("soundOn")
        cls._voiceOn = data.getboolean("voiceOn")
        cls._musicVolume = data.getfloat("musicVolume")
        cls._soundVolume = data.getfloat("soundVolume")
        cls._voiceVolume = data.getfloat("voiceVolume")
        realSize = Vector2u(
            int(cls._gameSize.x * cls._scale),
            int(cls._gameSize.y * cls._scale),
        )
        cls._window = RenderWindow(
            VideoMode(realSize),
            cls._title,
            Style.Titlebar | Style.Close,
            settings=ContextSettings(antiAliasingLevel=8),
        )
        # cls._window.setView(View(Vector2f(320, 240), Math.ToVector2f(cls._gameSize)))
        cls._canvas = RenderTexture(cls._gameSize)
        cls._canvasSprite = Sprite(cls._canvas.getTexture())
        cls._window.setIcon(cls._icon)
        cls._window.setFramerateLimit(cls._frameRate)
        cls._window.setVerticalSyncEnabled(cls._verticalSync)
        cls.setMusicVolume(cls._musicVolume)
        cls.setSoundVolume(cls._soundVolume)
        cls.setVoiceVolume(cls._voiceVolume)

    @classmethod
    def isActive(cls) -> bool:
        return cls._window.isOpen() and GetGameRunning()

    @classmethod
    def shouldLoop(cls) -> bool:
        return cls._window.isOpen() and GetGameRunning() and cls._scene is not None

    @classmethod
    def getWindow(cls) -> RenderWindow:
        return cls._window

    @classmethod
    def getCanvas(cls) -> RenderTexture:
        return cls._canvas

    @classmethod
    def clearCanvas(cls) -> None:
        cls._window.clear(Color.Transparent)
        cls._canvas.clear(Color.Transparent)

    @classmethod
    def drawOnCanvas(cls, drawable: Drawable) -> None:
        cls._window.draw(drawable)

    @classmethod
    def display(cls) -> None:
        # cls._canvas.display()
        # cls._window.draw(cls._canvasSprite)
        cls._window.display()

    @classmethod
    def getTitle(cls) -> str:
        return cls._title

    @classmethod
    def setTitle(cls, title: str) -> None:
        cls._title = title
        cls._window.setTitle(cls._title)

    @classmethod
    def getGameSize(cls) -> Vector2u:
        return cls._gameSize

    @classmethod
    def getWindowskinName(cls) -> str:
        return cls._windowskinName

    @classmethod
    def setWindowskinName(cls, name: str) -> None:
        cls._windowskinName = name

    @classmethod
    def getMainScript(cls) -> str:
        return cls._mainScript

    @classmethod
    def getScale(cls) -> float:
        return cls._scale

    @classmethod
    def getFrameRate(cls) -> int:
        return cls._frameRate

    @classmethod
    def setFrameRate(cls, frameRate: int) -> None:
        cls._frameRate = frameRate
        cls._window.setFramerateLimit(cls._frameRate)

    @classmethod
    def getVerticalSync(cls) -> bool:
        return cls._verticalSync

    @classmethod
    def setVerticalSync(cls, verticalSync: bool) -> None:
        cls._verticalSync = verticalSync
        cls._window.setVerticalSyncEnabled(cls._verticalSync)

    @classmethod
    def getMusicOn(cls) -> bool:
        return cls._musicOn

    @classmethod
    def setMusicOn(cls, musicOn: bool) -> None:
        cls._musicOn = musicOn
        if not cls._musicOn:
            Manager.stopMusic("BGM")
            Manager.stopMusic("BGS")

    @classmethod
    def getSoundOn(cls) -> bool:
        return cls._soundOn

    @classmethod
    def setSoundOn(cls, soundOn: bool) -> None:
        cls._soundOn = soundOn
        if not cls._soundOn:
            Manager.stopSound()

    @classmethod
    def getVoiceOn(cls) -> bool:
        return cls._voiceOn

    @classmethod
    def setVoiceOn(cls, voiceOn: bool) -> None:
        cls._voiceOn = voiceOn
        if not cls._voiceOn:
            pass

    @classmethod
    def getMusicVolume(cls) -> float:
        return cls._musicVolume

    @classmethod
    def setMusicVolume(cls, musicVolume: float) -> None:
        cls._musicVolume = musicVolume
        Manager.setMusicVolume(musicVolume)

    @classmethod
    def getSoundVolume(cls) -> float:
        return cls._soundVolume

    @classmethod
    def setSoundVolume(cls, soundVolume: float) -> None:
        cls._soundVolume = soundVolume
        Manager.setSoundVolume(soundVolume)

    @classmethod
    def getVoiceVolume(cls) -> float:
        return cls._voiceVolume

    @classmethod
    def setVoiceVolume(cls, voiceVolume: float) -> None:
        cls._voiceVolume = voiceVolume
        pass

    @classmethod
    def getScene(cls) -> SceneBase:
        return cls._scene

    @classmethod
    def setScene(cls, scene: SceneBase) -> None:
        cls._scene = scene

    @classmethod
    def getVariable(cls, name: str) -> Any:
        if not name in cls._variables:
            return None
        return cls._variables[name]

    @classmethod
    def setVariable(cls, name: str, value: Any) -> None:
        cls._variables[name] = value


iniFile = configparser.ConfigParser()
iniFile.read("Main.ini")
# System.init(iniFile["Main"])
