# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
import locale
import json
from typing import Any, Dict, List, Optional, TYPE_CHECKING
import configparser
from . import (
    Manager,
    Color,
    RenderWindow,
    RenderTexture,
    Texture,
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
from .NodeGraph import LatentManager
from .SceneBase import SceneBase
from .Utils import Math, File, Render

if TYPE_CHECKING:
    from Engine import Shader


class System:
    class Config:
        FontSize: int
        WindowColor: Color

    __data: configparser.ConfigParser
    __dataFilePath: str = None
    _window: RenderWindow = None
    _canvas: RenderTexture = None
    _canvasSprite: Sprite = None
    _transition: Texture = None
    _transitionTempTexture: RenderTexture = None
    _transitionSprite: Sprite = None
    _graphicsShader: Optional[Shader] = None
    _title: str = None
    _fonts: List[Font] = []
    _gameSize: Vector2u = None
    _icon: Image = None
    _windowskinName: str = None
    _mainScript: str = None
    _language: str = None
    _scale: float = 1.0
    _frameRate: int = 60
    _verticalSync: bool = False
    _musicOn: bool = True
    _soundOn: bool = True
    _voiceOn: bool = True
    _musicVolume: float = 100
    _soundVolume: float = 100
    _voiceVolume: float = 100
    _transitionShaderPath: str = None
    _tilemapLightMaskShaderPath: str = None
    _lightMaskShaderPath: str = None
    _materialShaderPath: str = None
    _transitionShader: Optional[Shader] = None
    _tilemapLightMaskShader: Optional[Shader] = None
    _lightMaskShader: Optional[Shader] = None
    _transitionResource: Optional[Texture] = None
    _inTransition: bool = False
    _transitionTimeCount: float = 0.0
    _transitionTime: float = 0.0
    _transitionFrozen: bool = False
    _transitionFreezePending: bool = False
    _scenes: List[SceneBase] = None
    _latentManager: LatentManager = None
    _variables: Dict[str, Any] = {}
    _debugMode: bool = False
    _showFPSGraph: bool = False
    _fpsHistory: List[float] = []
    _fpsAccumulator: float = 0.0
    _fpsCount: int = 0

    @classmethod
    def init(cls, inData: configparser.ConfigParser, dataFilePath: str) -> None:
        cls.__data = inData
        cls.__dataFilePath = dataFilePath
        data = inData["Main"]
        systemData = File.getJSONData("./Data/Configs/System.json")
        cls._title = systemData["title"]["value"]
        size = systemData["gameSize"]["value"]
        cls._gameSize = Vector2u(size[0], size[1])
        cls._fonts = [Manager.loadFont(font) for font in systemData["fonts"]["value"]]
        cls.Config.FontSize = systemData["fontSize"]["value"]
        cls._icon = Image(os.path.join("./Assets", systemData["icon"]["base"], systemData["icon"]["value"]))
        cls._windowskinName = systemData["windowskinName"]["value"]
        r, g, b, a = systemData["windowColor"]["value"]
        cls.Config.WindowColor = Color(r, g, b, a)
        SetCellSize(systemData["cellSize"]["value"])
        cls._mainScript = data["script"]
        cls._language = data["language"]
        if cls._language is None or cls._language == "" or cls._language == "None":
            lang, encoding = locale.getdefaultlocale()
            cls._language = lang
        cls._scale = data.getfloat("scale")
        cls._frameRate = data.getint("frameRate")
        cls._verticalSync = data.getboolean("verticalSync")
        cls._musicOn = data.getboolean("musicOn")
        cls._soundOn = data.getboolean("soundOn")
        cls._voiceOn = data.getboolean("voiceOn")
        cls._musicVolume = data.getfloat("musicVolume")
        cls._soundVolume = data.getfloat("soundVolume")
        cls._voiceVolume = data.getfloat("voiceVolume")
        cls._transitionShaderPath = systemData["transitionShaderPath"]["value"]
        cls._tilemapLightMaskShaderPath = systemData["tilemapLightMaskShaderPath"]["value"]
        cls._lightMaskShaderPath = systemData["lightMaskShaderPath"]["value"]
        cls._materialShaderPath = os.path.join(
            "./Assets", systemData["materialShaderPath"]["base"], systemData["materialShaderPath"]["value"]
        )
        cls._scenes = []
        realSize = Vector2u(
            int(cls._gameSize.x * cls._scale),
            int(cls._gameSize.y * cls._scale),
        )
        handle: Optional[str] = os.environ.get("WINDOWHANDLE")
        individual: Optional[str] = os.environ.get("INDIVIDUAL")
        cls._debugMode = handle is not None
        cls._showFPSGraph = os.environ.get("SHOWFPSGRAPH") == "True"
        if handle and individual != "True":
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
        cls._canvas.clear(Color.Transparent)
        cls._canvasSprite = Sprite(cls._canvas.getTexture())
        cls._transition = Texture(cls._window.getSize())
        cls._transitionTempTexture = RenderTexture(cls._window.getSize())
        cls._transitionTempTexture.clear(Color.Transparent)
        cls._transitionSprite = Sprite(cls._transitionTempTexture.getTexture())
        cls._window.setIcon(cls._icon)
        cls._window.setFramerateLimit(cls._frameRate)
        cls._window.setVerticalSyncEnabled(cls._verticalSync)
        cls._window.clear(Color.Transparent)
        if cls._transitionShaderPath:
            cls._transitionShader = Manager.loadShader(cls._transitionShaderPath)
        if cls._tilemapLightMaskShaderPath:
            cls._tilemapLightMaskShader = Manager.loadShader(cls._tilemapLightMaskShaderPath)
        if cls._lightMaskShaderPath:
            cls._lightMaskShader = Manager.loadShader(cls._lightMaskShaderPath)
        cls._latentManager = LatentManager()

    @classmethod
    def isDebugMode(cls) -> bool:
        return cls._debugMode

    @classmethod
    def recordFPS(cls, fps: float) -> None:
        if cls._debugMode and cls._showFPSGraph:
            cls._fpsAccumulator += fps
            cls._fpsCount += 1
            if cls._fpsCount >= 30:
                cls._fpsHistory.append(cls._fpsAccumulator / cls._fpsCount)
                cls._fpsAccumulator = 0.0
                cls._fpsCount = 0

    @classmethod
    def saveFPSHistory(cls) -> None:
        if not cls._debugMode or not cls._fpsHistory:
            return
        try:
            temp_dir = "./Temp"
            if not os.path.exists(temp_dir):
                os.makedirs(temp_dir)
            with open(os.path.join(temp_dir, "FPSHistory.json"), "w") as f:
                json.dump(cls._fpsHistory, f)
        except Exception as e:
            print(f"Failed to save FPS history: {e}")

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
    def display(cls, deltaTime: float) -> None:
        from .Utils import Render

        if cls._inTransition:
            cls._transitionTimeCount = min(cls._transitionTimeCount + deltaTime, cls._transitionTime)
        cls._canvas.display()
        states = Render.CanvasRenderStates()
        if cls._graphicsShader:
            cls._graphicsShader.setUniform("screenTex", cls._canvas.getTexture())
            cls._graphicsShader.setUniform("texSize", Math.ToVector2f(cls._canvas.getTexture().getSize()))
            states.shader = cls._graphicsShader

        if cls._inTransition:
            cls._transitionTempTexture.clear(Color.Transparent)
            cls._transitionTempTexture.draw(cls._canvasSprite, states)
            cls._transitionTempTexture.display()
            cls._transitionShader.setUniform("screenTex", cls._transitionTempTexture.getTexture())
            cls._transitionShader.setUniform("backTex", cls._transition)
            if cls._transitionResource:
                cls._transitionShader.setUniform("transitionResource", cls._transitionResource)
                cls._transitionShader.setUniform("useMask", 1)
            else:
                cls._transitionShader.setUniform("useMask", 0)
            cls._transitionShader.setUniform("progress", cls._transitionTimeCount)
            cls._transitionShader.setUniform("totalTime", cls._transitionTime)
            transitionStates = Render.CanvasRenderStates()
            transitionStates.shader = cls._transitionShader
            cls._window.draw(cls._transitionSprite, transitionStates)
        else:
            cls._window.draw(cls._canvasSprite, states)
        cls._window.display()
        if cls._transitionFreezePending:
            cls._transition.update(cls._window)
            cls._transitionFreezePending = False
            cls._transitionFrozen = True
        if cls._inTransition:
            if cls._transitionTimeCount >= cls._transitionTime:
                cls._inTransition = False
        cls._latentManager.update()

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
    def getLanguage(cls) -> str:
        return cls._language

    @classmethod
    def setLanguage(cls, language: str) -> None:
        cls._language = language
        cls._setIniData("language", cls._language)

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
        cls._setIniData("frameRate", cls._frameRate)

    @classmethod
    def getVerticalSync(cls) -> bool:
        return cls._verticalSync

    @classmethod
    def setVerticalSync(cls, verticalSync: bool) -> None:
        cls._verticalSync = verticalSync
        cls._window.setVerticalSyncEnabled(cls._verticalSync)
        cls._setIniData("verticalSync", cls._verticalSync)

    @classmethod
    def getMusicOn(cls) -> bool:
        return cls._musicOn

    @classmethod
    def setMusicOn(cls, musicOn: bool) -> None:
        cls._musicOn = musicOn
        if not cls._musicOn:
            Manager.stopMusic("BGM")
            Manager.stopMusic("BGS")
        cls._setIniData("musicOn", cls._musicOn)

    @classmethod
    def getSoundOn(cls) -> bool:
        return cls._soundOn

    @classmethod
    def setSoundOn(cls, soundOn: bool) -> None:
        cls._soundOn = soundOn
        if not cls._soundOn:
            Manager.stopSound()
        cls._setIniData("soundOn", cls._soundOn)

    @classmethod
    def getVoiceOn(cls) -> bool:
        return cls._voiceOn

    @classmethod
    def setVoiceOn(cls, voiceOn: bool) -> None:
        cls._voiceOn = voiceOn
        if not cls._voiceOn:
            pass
        cls._setIniData("voiceOn", cls._voiceOn)

    @classmethod
    def getMusicVolume(cls) -> float:
        return cls._musicVolume

    @classmethod
    def setMusicVolume(cls, musicVolume: float) -> None:
        cls._musicVolume = musicVolume
        cls._setIniData("musicVolume", cls._musicVolume)

    @classmethod
    def getSoundVolume(cls) -> float:
        return cls._soundVolume

    @classmethod
    def setSoundVolume(cls, soundVolume: float) -> None:
        cls._soundVolume = soundVolume
        cls._setIniData("soundVolume", cls._soundVolume)

    @classmethod
    def getVoiceVolume(cls) -> float:
        return cls._voiceVolume

    @classmethod
    def setVoiceVolume(cls, voiceVolume: float) -> None:
        cls._voiceVolume = voiceVolume
        cls._setIniData("voiceVolume", cls._voiceVolume)

    @classmethod
    def getMaterialShaderPath(cls) -> str:
        return cls._materialShaderPath

    @classmethod
    def getTilemapLightMaskShaderPath(cls) -> str:
        return cls._tilemapLightMaskShaderPath

    @classmethod
    def getLightMaskShaderPath(cls) -> str:
        return cls._lightMaskShaderPath

    @classmethod
    def getTransitionShader(cls) -> Shader:
        return cls._transitionShader

    @classmethod
    def getTilemapLightMaskShader(cls) -> Optional[Shader]:
        return cls._tilemapLightMaskShader

    @classmethod
    def getLightMaskShader(cls) -> Optional[Shader]:
        return cls._lightMaskShader

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
    def setTransition(cls, transitionResource: Optional[Texture] = None, transitionTime: float = 1.0) -> None:
        if not (cls._transitionShaderPath and cls._transitionShader):
            return
        cls._transitionResource = transitionResource
        cls._inTransition = True
        cls._transitionTimeCount = 0.0
        cls._transitionTime = float(transitionTime)
        cls._transitionTempTexture.clear(Color.Transparent)
        if not cls._transitionFrozen:
            cls._transition.update(cls._window)
        else:
            cls._transitionFrozen = False

    @classmethod
    def freezeTransitionBackground(cls) -> None:
        cls._transitionFreezePending = True

    @classmethod
    def getScene(cls) -> Optional[SceneBase]:
        if len(cls._scenes) == 0:
            return None
        return cls._scenes[-1]

    @classmethod
    def getSceneList(cls) -> List[SceneBase]:
        return cls._scenes

    @classmethod
    def setScene(cls, scene: SceneBase) -> None:
        cls.freezeTransitionBackground()
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
        scene = cls._scenes.pop()
        scene.onDestroy()

    @classmethod
    def exit(cls) -> None:
        while len(cls._scenes) > 0:
            cls.popScene()

    @classmethod
    def getVariable(cls, name: str) -> Any:
        if not name in cls._variables:
            return None
        return cls._variables[name]

    @classmethod
    def setVariable(cls, name: str, value: Any) -> None:
        cls._variables[name] = value

    @classmethod
    def getLatentManager(cls) -> LatentManager:
        return cls._latentManager

    @classmethod
    def _setIniData(cls, key: str, value: Any) -> None:
        cls.__data.set("Main", key, str(value))
        with open(cls.__dataFilePath, "w", encoding="utf-8") as f:
            cls.__data.write(f)
