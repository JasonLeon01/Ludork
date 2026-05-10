# -*- encoding: utf-8 -*-
r"""\brief Global system: window management, scene transitions, rendering pipeline, and game loop."""

from __future__ import annotations
import os
import locale
import json
from typing import Any, Dict, List, Optional, TYPE_CHECKING
import configparser
import Engine
from Engine import (
    Color,
    RenderWindow,
    RenderTexture,
    Texture,
    View,
    Vector2u,
    Sprite,
    Shader,
    Drawable,
    Locale,
)
from Engine.Utils import Math, Render

if TYPE_CHECKING:
    from .SceneBase import SceneBase


class System:
    r"""\brief Global system managing the game window, scenes, transitions, and rendering.

    All methods are classmethods operating on shared class-level state.
    """

    __data: configparser.ConfigParser
    __dataFilePath: str
    __graphicsCanvases: List[RenderTexture] = []
    _window: RenderWindow
    _canvas: RenderTexture
    _canvasSprite: Sprite
    _transition: Texture
    _transitionTempTexture: RenderTexture
    _transitionSprite: Sprite
    _graphicsShaders: List[Shader] = []
    _mainScript: str = ""
    _frameRate: int = 60
    _verticalSync: bool = False
    _musicOn: bool = True
    _soundOn: bool = True
    _voiceOn: bool = True
    _musicVolume: float = 100
    _soundVolume: float = 100
    _voiceVolume: float = 100
    _transitionShader: Optional[Shader] = None
    _transitionResource: Optional[Texture] = None
    _inTransition: bool = False
    _transitionTimeCount: float = 0.0
    _transitionTime: float = 0.0
    _transitionFrozen: bool = False
    _transitionFreezePending: bool = False
    _scenes: List[SceneBase] = []
    _debugMode: bool = False
    _showFPSGraph: bool = False
    _fpsHistory: List[float] = []
    _fpsAccumulator: float = 0.0
    _fpsCount: int = 0

    @classmethod
    def init(cls, inData: configparser.ConfigParser, dataFilePath: str) -> None:
        r"""\brief Initialise the global system from configuration data.

        - \param inData ConfigParser instance with game settings.
        - \param dataFilePath Path to the configuration file for saving changes.
        """
        cls.__data = inData
        cls.__dataFilePath = dataFilePath
        cls.__graphicsCanvases = []
        data = inData["Main"]
        cls._mainScript = data["script"]
        _language = data["language"]
        if _language is None or _language == "" or _language == "None":
            lang, encoding = locale.getdefaultlocale()
            _language = lang
        Locale.LANGUAGE = _language
        Engine.Scale = data.getfloat("scale")
        cls._frameRate = data.getint("frameRate")
        cls._verticalSync = data.getboolean("verticalSync")
        cls._musicOn = data.getboolean("musicOn")
        cls._soundOn = data.getboolean("soundOn")
        cls._voiceOn = data.getboolean("voiceOn")
        cls._musicVolume = data.getfloat("musicVolume")
        cls._soundVolume = data.getfloat("soundVolume")
        cls._voiceVolume = data.getfloat("voiceVolume")
        cls._scenes = []
        cls._transitionShader = Shader("./Assets/Shaders/Transition.frag", Shader.Type.Fragment)

    @classmethod
    def isDebugMode(cls) -> bool:
        r"""\brief Check if debug mode is enabled.

        - \return True if debug mode is active.
        """
        return cls._debugMode

    @classmethod
    def setDebugMode(cls, debugMode: bool) -> None:
        r"""\brief Enable or disable debug mode.

        - \param debugMode True to enable debug mode.
        """
        cls._debugMode = debugMode

    @classmethod
    def setShowFPSGraph(cls, showFPSGraph: bool) -> None:
        r"""\brief Show or hide the FPS graph in debug mode.

        - \param showFPSGraph True to show the FPS graph.
        """
        cls._showFPSGraph = showFPSGraph

    @classmethod
    def recordFPS(cls, fps: float) -> None:
        r"""\brief Record a frame's FPS value for averaging.

        - \param fps The frame's FPS value.
        """
        if cls._debugMode and cls._showFPSGraph:
            cls._fpsAccumulator += fps
            cls._fpsCount += 1
            if cls._fpsCount >= 30:
                cls._fpsHistory.append(cls._fpsAccumulator / cls._fpsCount)
                cls._fpsAccumulator = 0.0
                cls._fpsCount = 0

    @classmethod
    def saveFPSHistory(cls) -> None:
        r"""\brief Save the FPS history to a JSON file."""
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
    def getGameSize(cls) -> Vector2u:
        r"""\brief Get the game's logical resolution.

        - \return The game size in pixels.
        """
        return Engine.GameSize

    @classmethod
    def setGameSize(cls, gameSize: Vector2u) -> None:
        r"""\brief Set the game's logical resolution.

        - \param gameSize The new game size in pixels.
        """
        Engine.GameSize = gameSize

    @classmethod
    def isActive(cls) -> bool:
        r"""\brief Check if the game window is open and running.

        - \return True if the window is open and the game is running.
        """
        from Engine import GameRunning

        return cls._window.isOpen() and GameRunning

    @classmethod
    def shouldLoop(cls) -> bool:
        r"""\brief Check if the game loop should continue.

        - \return True if the window is open, the game is running, and scenes are active.
        """
        from Engine import GameRunning

        return cls._window.isOpen() and GameRunning and len(cls._scenes) > 0

    @classmethod
    def initWindow(cls, window: RenderWindow) -> None:
        r"""\brief Initialise the game window.

        - \param window The RenderWindow to use.
        """
        cls._window = window
        cls._window.setFramerateLimit(cls._frameRate)
        cls._window.setVerticalSyncEnabled(cls._verticalSync)
        cls._window.clear(Color.Transparent)

    @classmethod
    def getWindow(cls) -> RenderWindow:
        r"""\brief Get the game window.

        - \return The RenderWindow.
        """
        return cls._window

    @classmethod
    def initCanvas(cls, size: Vector2u) -> None:
        r"""\brief Initialise the main render canvas.

        - \param size The canvas size in pixels.
        """
        cls._canvas = RenderTexture(size)
        cls._canvas.clear(Color.Transparent)
        cls._canvasSprite = Sprite(cls._canvas.getTexture())
        cls._transition = Texture(size)
        cls._transitionTempTexture = RenderTexture(size)
        cls._transitionTempTexture.clear(Color.Transparent)
        cls._transitionSprite = Sprite(cls._transitionTempTexture.getTexture())

    @classmethod
    def clearCanvas(cls) -> None:
        r"""\brief Clear both the window and canvas to transparent."""
        cls._window.clear(Color.Transparent)
        cls._canvas.clear(Color.Transparent)

    @classmethod
    def setWindowMapView(cls) -> None:
        r"""\brief Set the canvas view to centre on the game's logical size."""
        cls._canvas.setView(View(Math.ToVector2f(Engine.GameSize / 2), Math.ToVector2f(Engine.GameSize)))

    @classmethod
    def setWindowDefaultView(cls) -> None:
        r"""\brief Reset the canvas view to its default."""
        cls._canvas.setView(cls._canvas.getDefaultView())

    @classmethod
    def draw(cls, drawable: Drawable, shader: Optional[Shader] = None) -> None:
        r"""\brief Draw a drawable object to the canvas.

        - \param drawable The drawable object to render.
        - \param shader Optional shader to apply.
        """
        states = Render.CanvasRenderStates()
        if shader:
            states.shader = shader
        cls._canvas.draw(drawable, states)

    @classmethod
    def display(cls, deltaTime: float) -> None:
        r"""\brief Display the canvas contents to the window.

        Handles graphics shaders, transitions, and freeze effect.

        - \param deltaTime Elapsed time in seconds since the previous frame.
        """
        if cls._inTransition:
            cls._transitionTimeCount = min(cls._transitionTimeCount + deltaTime, cls._transitionTime)
        cls._canvas.display()
        states = Render.CanvasRenderStates()
        finalCanvas = cls._canvas
        if len(cls.__graphicsCanvases) > 0:
            for i, canvas in enumerate(cls.__graphicsCanvases):
                canvas.clear(Color.Transparent)
                lastCanvas = cls._canvas
                if i > 0:
                    lastCanvas = cls.__graphicsCanvases[i - 1]
                cls._graphicsShaders[i].setUniform("screenTex", lastCanvas.getTexture())
                cls._graphicsShaders[i].setUniform("texSize", Math.ToVector2f(lastCanvas.getTexture().getSize()))
                states.shader = cls._graphicsShaders[i]
                tempSprite = Sprite(canvas.getTexture())
                canvas.draw(tempSprite, states)
                canvas.display()
            finalCanvas = cls.__graphicsCanvases[-1]
        cls._canvasSprite.setTexture(finalCanvas.getTexture())
        if cls._inTransition and cls._transitionShader:
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

    @classmethod
    def getMainScript(cls) -> str:
        r"""\brief Get the path of the main script.

        - \return The main script path.
        """
        return cls._mainScript

    @classmethod
    def getLanguage(cls) -> str:
        r"""\brief Get the current language code.

        - \return The language code string.
        """
        return cls._language

    @classmethod
    def setLanguage(cls, language: str) -> None:
        r"""\brief Set the current language.

        - \param language The language code to set.
        """
        cls._language = language
        cls._setIniData("language", cls._language)

    @classmethod
    def setScale(cls, scale: float) -> None:
        r"""\brief Set the game's display scale factor.

        - \param scale The new scale factor.
        """
        Engine.Scale = scale

    @classmethod
    def getScale(cls) -> float:
        r"""\brief Get the game's display scale factor.

        - \return The current scale factor.
        """
        return Engine.Scale

    @classmethod
    def getFrameRate(cls) -> int:
        r"""\brief Get the target frame rate.

        - \return The target frames per second.
        """
        return cls._frameRate

    @classmethod
    def setFrameRate(cls, frameRate: int) -> None:
        r"""\brief Set the target frame rate.

        - \param frameRate The target frames per second.
        """
        cls._frameRate = frameRate
        cls._window.setFramerateLimit(cls._frameRate)
        cls._setIniData("frameRate", cls._frameRate)

    @classmethod
    def getVerticalSync(cls) -> bool:
        r"""\brief Check if vertical sync is enabled.

        - \return True if vertical sync is enabled.
        """
        return cls._verticalSync

    @classmethod
    def setVerticalSync(cls, verticalSync: bool) -> None:
        r"""\brief Enable or disable vertical sync.

        - \param verticalSync True to enable vertical sync.
        """
        cls._verticalSync = verticalSync
        cls._window.setVerticalSyncEnabled(cls._verticalSync)
        cls._setIniData("verticalSync", cls._verticalSync)

    @classmethod
    def getMusicOn(cls) -> bool:
        r"""\brief Check if music playback is enabled.

        - \return True if music is enabled.
        """
        return cls._musicOn

    @classmethod
    def setMusicOn(cls, musicOn: bool) -> None:
        r"""\brief Enable or disable music playback.

        - \param musicOn True to enable music.
        """
        from . import Manager

        cls._musicOn = musicOn
        if not cls._musicOn:
            Manager.stopMusic("BGM")
            Manager.stopMusic("BGS")
        cls._setIniData("musicOn", cls._musicOn)

    @classmethod
    def getSoundOn(cls) -> bool:
        r"""\brief Check if sound effect playback is enabled.

        - \return True if sound effects are enabled.
        """
        return cls._soundOn

    @classmethod
    def setSoundOn(cls, soundOn: bool) -> None:
        r"""\brief Enable or disable sound effect playback.

        - \param soundOn True to enable sound effects.
        """
        from . import Manager

        cls._soundOn = soundOn
        if not cls._soundOn:
            Manager.stopSound()
        cls._setIniData("soundOn", cls._soundOn)

    @classmethod
    def getVoiceOn(cls) -> bool:
        r"""\brief Check if voice playback is enabled.

        - \return True if voice is enabled.
        """
        return cls._voiceOn

    @classmethod
    def setVoiceOn(cls, voiceOn: bool) -> None:
        r"""\brief Enable or disable voice playback.

        - \param voiceOn True to enable voice.
        """
        cls._voiceOn = voiceOn
        if not cls._voiceOn:
            pass
        cls._setIniData("voiceOn", cls._voiceOn)

    @classmethod
    def getMusicVolume(cls) -> float:
        r"""\brief Get the music volume level.

        - \return The music volume (0-100).
        """
        return cls._musicVolume

    @classmethod
    def setMusicVolume(cls, musicVolume: float) -> None:
        r"""\brief Set the music volume level.

        - \param musicVolume The music volume (0-100).
        """
        cls._musicVolume = musicVolume
        cls._setIniData("musicVolume", cls._musicVolume)

    @classmethod
    def getSoundVolume(cls) -> float:
        r"""\brief Get the sound effect volume level.

        - \return The sound volume (0-100).
        """
        return cls._soundVolume

    @classmethod
    def setSoundVolume(cls, soundVolume: float) -> None:
        r"""\brief Set the sound effect volume level.

        - \param soundVolume The sound volume (0-100).
        """
        cls._soundVolume = soundVolume
        cls._setIniData("soundVolume", cls._soundVolume)

    @classmethod
    def getVoiceVolume(cls) -> float:
        r"""\brief Get the voice volume level.

        - \return The voice volume (0-100).
        """
        return cls._voiceVolume

    @classmethod
    def setVoiceVolume(cls, voiceVolume: float) -> None:
        r"""\brief Set the voice volume level.

        - \param voiceVolume The voice volume (0-100).
        """
        cls._voiceVolume = voiceVolume
        cls._setIniData("voiceVolume", cls._voiceVolume)

    @classmethod
    def addGraphicsShader(cls, shader: Optional[Shader], uniforms: Optional[Dict[str, Any]] = None) -> None:
        r"""\brief Add a post-processing graphics shader.

        - \param shader The shader to add.
        - \param uniforms Optional uniforms to set on the shader.
        """
        cls._graphicsShaders.append(shader)
        if shader and uniforms:
            for name, value in uniforms.items():
                shader.setUniform(name, value)
        cls._applyGraphicsShadersLength()

    @classmethod
    def removeGraphicsShader(cls, shader: Optional[Shader]) -> None:
        r"""\brief Remove a post-processing graphics shader.

        - \param shader The shader to remove.
        """
        if shader in cls._graphicsShaders:
            cls._graphicsShaders.remove(shader)
        cls._applyGraphicsShadersLength()

    @classmethod
    def removeAllGraphicsShaders(cls) -> None:
        r"""\brief Remove all post-processing graphics shaders."""
        cls._graphicsShaders.clear()
        cls._applyGraphicsShadersLength()

    @classmethod
    def removeGraphicsShaderAt(cls, index: int) -> None:
        r"""\brief Remove a graphics shader at the specified index.

        - \param index The index of the shader to remove.
        """
        if index < 0 or index >= len(cls._graphicsShaders):
            return
        cls._graphicsShaders.pop(index)
        cls._applyGraphicsShadersLength()

    @classmethod
    def setTransition(cls, transitionResource: Optional[Texture] = None, transitionTime: float = 1.0) -> None:
        r"""\brief Start a scene transition effect.

        - \param transitionResource Optional texture used as the transition mask.
        - \param transitionTime Duration of the transition in seconds.
        """
        if not (cls._transitionShader):
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
        r"""\brief Freeze the current frame to use as the transition background."""
        cls._transitionFreezePending = True

    @classmethod
    def getScene(cls) -> Optional[SceneBase]:
        r"""\brief Get the current active scene from the top of the stack.

        - \return The current scene, or None if the stack is empty.
        """
        if len(cls._scenes) == 0:
            return None
        return cls._scenes[-1]

    @classmethod
    def getSceneList(cls) -> List[SceneBase]:
        r"""\brief Get the full scene stack.

        - \return A list of all active scenes.
        """
        return cls._scenes

    @classmethod
    def setScene(cls, scene: SceneBase) -> None:
        r"""\brief Replace the current scene on the top of the stack.

        - \param scene The new scene to set.
        """
        cls.freezeTransitionBackground()
        if len(cls._scenes) == 0:
            cls._scenes.append(scene)
        else:
            cls._scenes[-1] = scene

    @classmethod
    def pushScene(cls, scene: SceneBase) -> None:
        r"""\brief Push a new scene onto the top of the stack.

        - \param scene The scene to push.
        """
        cls._scenes.append(scene)

    @classmethod
    def popScene(cls) -> None:
        r"""\brief Pop the top scene from the stack, stop its logic thread, and destroy it."""
        assert len(cls._scenes) > 0
        scene = cls._scenes.pop()
        scene._stopLogicThread()
        scene.onDestroy()

    @classmethod
    def exit(cls) -> None:
        r"""\brief Pop and destroy all scenes in the stack."""
        while len(cls._scenes) > 0:
            cls.popScene()

    @classmethod
    def _setIniData(cls, key: str, value: Any) -> None:
        cls.__data.set("Main", key, str(value))
        with open(cls.__dataFilePath, "w", encoding="utf-8") as f:
            cls.__data.write(f)

    @classmethod
    def _applyGraphicsShadersLength(cls) -> None:
        if len(cls._graphicsShaders) < len(cls.__graphicsCanvases):
            cls.__graphicsCanvases = cls.__graphicsCanvases[: len(cls._graphicsShaders)]
        elif len(cls._graphicsShaders) > len(cls.__graphicsCanvases):
            cls.__graphicsCanvases += [
                RenderTexture(cls._window.getSize())
                for _ in range(len(cls._graphicsShaders) - len(cls.__graphicsCanvases))
            ]
