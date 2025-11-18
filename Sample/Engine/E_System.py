# -*- encoding: utf-8 -*-

from __future__ import annotations
import configparser
import os
from typing import Any, Dict, List, Optional, TYPE_CHECKING
from . import (
    Manager,
    Color,
    RenderWindow,
    RenderTexture,
    View,
    Vector2u,
    Font,
    Image,
    Sprite,
    Shader,
    Drawable,
    VideoMode,
    SetCellSize,
    GetGameRunning,
    Style,
    ContextSettings,
)
from .Gameplay import SceneBase
from .Utils import Math, File, Render

if TYPE_CHECKING:
    from Engine import Shader


class System:
    class Config:
        FontSize: int
        WindowColor: Color

    _window: RenderWindow = None
    _canvas: RenderTexture = None
    _canvasSprite: Sprite = None
    _graphicsShader: Optional[Shader] = None
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
    _lightShaderPath: str = None
    _vagueShaderPath: str = None
    _toneShaderPath: str = None
    _grayScaleShaderPath: str = None
    _scenes: List[SceneBase] = None
    _variables: Dict[str, Any] = {}
    _debugMode: bool = False

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
        cls._lightShaderPath = systemData["lightShaderPath"]
        cls._vagueShaderPath = systemData["vagueShaderPath"]
        cls._toneShaderPath = systemData["toneShaderPath"]
        cls._grayScaleShaderPath = systemData["grayScaleShaderPath"]
        cls._scenes = []
        realSize = Vector2u(
            int(cls._gameSize.x * cls._scale),
            int(cls._gameSize.y * cls._scale),
        )
        handle: Optional[int] = os.environ.get("WINDOWHANDLE")
        cls._debugMode = handle is not None
        cls._debugMode = True
        if handle:
            cls._window = RenderWindow(int(handle), settings=ContextSettings(antiAliasingLevel=8))
            windowSize = cls._window.getSize()
            cls._scale = min(windowSize.x / cls._gameSize.x, windowSize.y / cls._gameSize.y)
        else:
            cls._window = RenderWindow(
                VideoMode(realSize),
                cls._title,
                Style.Titlebar | Style.Close,
                settings=ContextSettings(antiAliasingLevel=8),
            )
        cls._canvas = RenderTexture(cls._window.getSize())
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
        return cls._window.isOpen() and GetGameRunning() and len(cls._scenes) > 0

    @classmethod
    def getWindow(cls) -> RenderWindow:
        return cls._window

    @classmethod
    def getFonts(cls) -> List[Font]:
        return cls._fonts

    @classmethod
    def clearCanvas(cls) -> None:
        cls._window.clear(Color.Transparent)
        cls._canvas.clear(Color.Transparent)

    @classmethod
    def setWindowMapView(cls) -> None:
        cls._canvas.setView(View(Math.ToVector2f(cls._gameSize / 2), Math.ToVector2f(cls._gameSize)))

    @classmethod
    def setWindowDefaultView(cls) -> None:
        cls._canvas.setView(cls._canvas.getDefaultView())

    @classmethod
    def draw(cls, drawable: Drawable, shader: Optional[Shader] = None) -> None:
        states = Render.CanvasRenderStates()
        if shader:
            states.shader = shader
        cls._canvas.draw(drawable, states)

    @classmethod
    def display(cls) -> None:
        from Engine.Utils import Render

        cls._canvas.display()
        states = Render.CanvasRenderStates()
        if cls._graphicsShader:
            cls._graphicsShader.setUniform("screenTex", cls._canvas.getTexture())
            states.shader = cls._graphicsShader
        cls._window.draw(cls._canvasSprite, states)
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

    @classmethod
    def getSoundVolume(cls) -> float:
        return cls._soundVolume

    @classmethod
    def setSoundVolume(cls, soundVolume: float) -> None:
        cls._soundVolume = soundVolume

    @classmethod
    def getVoiceVolume(cls) -> float:
        return cls._voiceVolume

    @classmethod
    def setVoiceVolume(cls, voiceVolume: float) -> None:
        cls._voiceVolume = voiceVolume
        pass

    @classmethod
    def getLightShaderPath(cls) -> str:
        return cls._lightShaderPath

    @classmethod
    def getVagueShaderPath(cls) -> str:
        return cls._vagueShaderPath

    @classmethod
    def getToneShaderPath(cls) -> str:
        return cls._toneShaderPath

    @classmethod
    def getGrayScaleShaderPath(cls) -> str:
        return cls._grayScaleShaderPath

    @classmethod
    def getGraphicsShader(cls) -> Optional[Shader]:
        return cls._graphicsShader

    @classmethod
    def setGraphicsShader(cls, shader: Optional[Shader], uniforms: Optional[Dict[str, Any]] = None) -> None:
        cls._graphicsShader = shader
        if shader and uniforms:
            for name, value in uniforms.items():
                shader.setUniform(name, value)

    @classmethod
    def getScene(cls) -> SceneBase:
        return cls._scenes[-1]

    @classmethod
    def getSceneList(cls) -> List[SceneBase]:
        return cls._scenes

    @classmethod
    def setScene(cls, scene: SceneBase) -> None:
        if len(cls._scenes) == 0:
            cls._scenes.append(scene)
        else:
            cls._scenes[-1] = scene

    @classmethod
    def pushScene(cls, scene: SceneBase) -> None:
        cls._scenes.append(scene)

    @classmethod
    def popScene(cls) -> None:
        assert len(cls._scenes) > 0
        cls._scenes.pop()

    @classmethod
    def getVariable(cls, name: str) -> Any:
        if not name in cls._variables:
            return None
        return cls._variables[name]

    @classmethod
    def setVariable(cls, name: str, value: Any) -> None:
        cls._variables[name] = value

    @classmethod
    def isDebugMode(cls) -> bool:
        return cls._debugMode


iniFile = configparser.ConfigParser()
iniFile.read("Main.ini")
System.init(iniFile["Main"])
