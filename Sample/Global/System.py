# -*- encoding: utf-8 -*-
r"""\brief Global system: window management, scene transitions, rendering pipeline, and game loop."""

from __future__ import annotations
import json
import logging
import os
import random
import sys
import threading
from typing import Any, Dict, List, Optional, TYPE_CHECKING
import configparser
import Engine
from Engine import (
    Color,
    RenderWindow,
    RenderTexture,
    Texture,
    View,
    Vector2f,
    Vector2u,
    Vector4f,
    Sprite,
    Shader,
    Drawable,
)
from Engine.Utils import Math, Render
from Engine.Utils.Inner import IS_IOS_PLATFORM, warnIosShaderSkippedOnce
from .SystemConfigBase import SystemConfigBase

if TYPE_CHECKING:
    from .SceneBase import SceneBase


class System(SystemConfigBase):
    r"""\brief Global system managing the game window, scenes, transitions, and rendering.

    All methods are classmethods operating on shared class-level state.
    """

    __graphicsCanvases: List[RenderTexture] = []
    _window: RenderWindow
    _canvas: RenderTexture
    _canvasSprite: Sprite
    _transition: Texture
    _transitionTempTexture: RenderTexture
    _transitionSprite: Sprite
    _graphicsShaders: List[Shader] = []
    _transitionShader: Optional[Shader] = None
    _transitionResource: Optional[Texture] = None
    _inTransition: bool = False
    _transitionTimeCount: float = 0.0
    _transitionTime: float = 0.0
    _transitionFrozen: bool = False
    _transitionFreezePending: bool = False
    _flashShader: Optional[Shader] = None
    _flashColour: Vector4f = Vector4f(1.0, 1.0, 1.0, 1.0)
    _flashDuration: float = 0.0
    _flashTimeCount: float = 0.0
    _flashActive: bool = False
    _toneShader: Optional[Shader] = None
    _toneCurrentColour: Vector4f = Vector4f(0.0, 0.0, 0.0, 0.0)
    _toneStartColour: Vector4f = Vector4f(0.0, 0.0, 0.0, 0.0)
    _toneTargetColour: Vector4f = Vector4f(0.0, 0.0, 0.0, 0.0)
    _toneDuration: float = 0.0
    _toneTimeCount: float = 0.0
    _toneActive: bool = False
    _toneBuffer: Optional[RenderTexture] = None
    _toneBufferSprite: Optional[Sprite] = None
    _shakePower: float = 0.0
    _shakeSpeed: float = 0.0
    _shakeDuration: float = 0.0
    _shakeTimeCount: float = 0.0
    _shakeActive: bool = False
    _shakeOffset: Vector2f = Vector2f(0.0, 0.0)
    _shakeNextUpdate: float = 0.0
    _scenes: List[SceneBase] = []
    _pendingSceneReplace: Optional[Any] = None
    _pendingSceneLock: threading.Lock = threading.Lock()
    _pendingTransition: Optional[tuple[Optional[str], float]] = None
    _pendingTransitionLock: threading.Lock = threading.Lock()
    _sceneOpThreadIdent: Optional[int] = None
    _debugMode: bool = False
    _performanceMonitorEnabled: bool = False
    _performanceFPSAccumulator: float = 0.0
    _performanceFPSCount: int = 0

    @classmethod
    def init(cls, inData: configparser.ConfigParser, dataFilePath: str) -> None:
        r"""\brief Initialise the global system from configuration data.

        - \param inData ConfigParser instance with game settings.
        - \param dataFilePath Path to the configuration file for saving changes.
        """
        super().init(inData, dataFilePath)
        cls.__graphicsCanvases = []
        cls._scenes = []
        with cls._pendingSceneLock:
            cls._pendingSceneReplace = None
        cls._sceneOpThreadIdent = None
        if IS_IOS_PLATFORM:
            cls._transitionShader = None
            warnIosShaderSkippedOnce(
                "System.transitionShader",
                "iOS: shaders are disabled; skipped loading transition shader",
            )
        else:
            cls._transitionShader = Shader("./Assets/Shaders/Global/Transition.frag", Shader.Type.Fragment)

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
    def setPerformanceMonitorEnabled(cls, enabled: bool) -> None:
        r"""\brief Enable or disable performance streaming for the editor monitor.

        - \param enabled True to stream performance samples to the editor.
        """
        cls._performanceMonitorEnabled = bool(enabled)
        cls._performanceFPSAccumulator = 0.0
        cls._performanceFPSCount = 0

    @classmethod
    def isPerformanceMonitorEnabled(cls) -> bool:
        r"""\brief Check whether the editor performance monitor is receiving samples.

        - \return True if performance samples should be streamed.
        """
        return cls._performanceMonitorEnabled

    @classmethod
    def recordPerformance(cls, fps: float) -> None:
        r"""\brief Record and stream a frame's performance values.

        - \param fps The frame's FPS value.
        """
        if not cls._debugMode or not cls._performanceMonitorEnabled:
            return
        cls._performanceFPSAccumulator += fps
        cls._performanceFPSCount += 1
        if cls._performanceFPSCount >= 30:
            sample = cls._performanceFPSAccumulator / cls._performanceFPSCount
            payload = {"fps": sample, "memory": cls._getProcessMemoryMB()}
            sys.stdout.write(f"__LUDORK_PERF__:{json.dumps(payload, separators=(',', ':'))}\n")
            sys.stdout.flush()
            cls._performanceFPSAccumulator = 0.0
            cls._performanceFPSCount = 0

    @classmethod
    def _getProcessMemoryMB(cls) -> float:
        try:
            import psutil  # type: ignore

            process = psutil.Process(os.getpid())
            return float(process.memory_info().rss) / 1024.0 / 1024.0
        except Exception:
            return 0.0

    @classmethod
    def getGameSize(cls) -> Vector2u:
        r"""\brief Get the game's logical resolution.

        - \return The game size in pixels.
        """
        return Engine.GameSize

    @classmethod
    @Meta(Vector2uVars=["gameSize"])
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
    @Meta(Vector2uVars=["size"])
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
    @Meta(Vector2fVars=["offset"])
    def setWindowMapView(cls, offset: Vector2f = Vector2f(0.0, 0.0)) -> None:
        r"""\brief Set the map canvas view to the game's logical size with a screen offset.

        - \param offset Screen-space offset for map drawing on the main canvas.
        """
        cls._canvas.setView(
            View(
                Math.ToVector2f(Engine.GameSize / 2) - offset,
                Math.ToVector2f(Engine.GameSize),
            )
        )

    @classmethod
    def setWindowDefaultView(cls) -> None:
        r"""\brief Reset the canvas view to its default."""
        cls._canvas.setView(cls._canvas.getDefaultView())

    @classmethod
    def getCanvas(cls) -> RenderTexture:
        r"""\brief Get the main game render canvas.

        - \return The active RenderTexture used for world and UI drawing.
        """
        return cls._canvas

    @classmethod
    def setWeather(cls, weatherType, power: int, maxCount: int) -> None:
        r"""\brief Set RMXP-style screen weather (rain, storm, or snow).

        - \param weatherType WeatherType enum, name, or localized DropBox label.
        - \param power Effect strength from 0 to 100.
        - \param maxCount Density cap from 0 to 100.
        """
        from .Weather import WeatherController

        WeatherController.setWeather(weatherType, power, maxCount)

    @classmethod
    def clearWeather(cls) -> None:
        r"""\brief Clear any active screen weather effect."""
        from .Weather import WeatherController

        WeatherController.clearWeather()

    @classmethod
    def updateWeather(cls, deltaTime: float) -> None:
        r"""\brief Update active weather timers and storm flashes.

        - \param deltaTime Elapsed time in seconds.
        """
        from .Weather import WeatherController

        WeatherController.update(deltaTime)

    @classmethod
    def updateFog(cls, deltaTime: float) -> None:
        r"""\brief Update active map fog scroll offset.

        - \param deltaTime Elapsed time in seconds.
        """
        from .Fog import FogController

        FogController.update(deltaTime)

    @classmethod
    def clearFog(cls) -> None:
        r"""\brief Clear active map fog."""
        from .Fog import FogController

        FogController.clearFog()

    @classmethod
    def applyFogFromMapData(cls, mapData) -> None:
        r"""\brief Apply fog settings from loaded map data.

        - \param mapData Serialized map dictionary.
        """
        from .Fog import FogController

        FogController.applyFromMapData(mapData)

    @classmethod
    def draw(cls, drawable: Drawable, shader: Optional[Shader] = None) -> None:
        r"""\brief Draw a drawable object to the canvas.

        - \param drawable The drawable object to render.
        - \param shader Optional shader to apply.
        """
        states = Render.CanvasRenderStates()
        if shader:
            states.shader = shader
        from Engine.UI.Base import ControlBase

        if isinstance(drawable, ControlBase):
            drawable.draw(cls._canvas, states)
        else:
            cls._canvas.draw(drawable, states)

    @classmethod
    def applyScreenTonePass(cls) -> None:
        r"""\brief Apply the active screen tone to the main canvas before UI is drawn.

        Tone is intentionally applied below UI so windows and HUD stay unaffected.
        """
        if IS_IOS_PLATFORM or not cls._toneActive or cls._toneShader is None:
            return
        if cls._isNeutralTone(cls._toneCurrentColour):
            return

        cls._canvas.display()
        sourceTex = cls._canvas.getTexture()
        size = cls._canvas.getSize()
        buffer = cls._ensureToneBuffer(size)
        sprite = cls._ensureToneBufferSprite()
        sprite.setTexture(sourceTex, True)
        sprite.setPosition(Vector2f(0.0, 0.0))
        sprite.setScale(Vector2f(1.0, 1.0))
        cls._toneShader.setUniform("screenTex", sourceTex)
        cls._toneShader.setUniform("texSize", Math.ToVector2f(size))
        cls._applyScreenToneUniform()
        buffer.clear(Color.Transparent)
        toneStates = Render.CanvasRenderStates()
        toneStates.shader = cls._toneShader
        buffer.draw(sprite, toneStates)
        buffer.display()
        savedView = cls._canvas.getView()
        cls._canvas.clear(Color.Transparent)
        cls._canvas.setView(cls._canvas.getDefaultView())
        sprite.setTexture(buffer.getTexture(), True)
        cls._canvas.draw(sprite, Render.CanvasRenderStates())
        cls._canvas.setView(savedView)

    @classmethod
    def display(cls, deltaTime: float) -> None:
        r"""\brief Display the canvas contents to the window.

        Handles graphics shaders, transitions, freeze effect, and screen shake.

        - \param deltaTime Elapsed time in seconds since the previous frame.
        """
        if cls._inTransition:
            cls._transitionTimeCount = min(cls._transitionTimeCount + deltaTime, cls._transitionTime)
        cls._updateFlash(deltaTime)
        cls._updateScreenTone(deltaTime)
        cls._updateShake(deltaTime)
        cls.updateWeather(deltaTime)
        cls.updateFog(deltaTime)
        cls.applyPendingTransition()
        cls._canvas.display()
        states = Render.CanvasRenderStates()
        finalCanvas = cls._canvas
        if len(cls.__graphicsCanvases) > 0:
            for i, canvas in enumerate(cls.__graphicsCanvases):
                canvas.clear(Color.Transparent)
                lastCanvas = cls._canvas
                if i > 0:
                    lastCanvas = cls.__graphicsCanvases[i - 1]
                postShader = cls._graphicsShaders[i] if i < len(cls._graphicsShaders) else None
                if postShader is None or postShader is cls._toneShader:
                    if IS_IOS_PLATFORM:
                        warnIosShaderSkippedOnce(
                            "System.display.postProcessShader",
                            "iOS: shaders are disabled; skipped post-process shader pass",
                        )
                    continue
                postShader.setUniform("screenTex", lastCanvas.getTexture())
                postShader.setUniform("texSize", Math.ToVector2f(lastCanvas.getTexture().getSize()))
                postStates = Render.CanvasRenderStates()
                postStates.shader = postShader
                tempSprite = Sprite(lastCanvas.getTexture())
                canvas.draw(tempSprite, postStates)
                canvas.display()
            finalCanvas = cls.__graphicsCanvases[-1]
        cls._canvasSprite.setTexture(finalCanvas.getTexture())
        if cls._shakeActive:
            canvasTexSize = finalCanvas.getSize()
            pad = cls._shakePower
            cls._canvasSprite.setScale(
                Vector2f(
                    (canvasTexSize.x + pad * 2) / canvasTexSize.x,
                    (canvasTexSize.y + pad * 2) / canvasTexSize.y,
                )
            )
            cls._canvasSprite.setPosition(
                Vector2f(
                    -pad + cls._shakeOffset.x,
                    -pad + cls._shakeOffset.y,
                )
            )
        if cls._inTransition and cls._transitionShader:
            cls._transitionTempTexture.clear(Color.Transparent)
            cls._transitionTempTexture.draw(cls._canvasSprite, Render.CanvasRenderStates())
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
            cls._window.draw(cls._canvasSprite, Render.CanvasRenderStates())
        if cls._shakeActive:
            cls._canvasSprite.setScale(Vector2f(1.0, 1.0))
            cls._canvasSprite.setPosition(Vector2f(0.0, 0.0))
        cls._window.display()
        if cls._transitionFreezePending:
            cls._transition.update(cls._window)
            cls._transitionFreezePending = False
            cls._transitionFrozen = True
        if cls._inTransition:
            if cls._transitionTimeCount >= cls._transitionTime:
                cls._inTransition = False

    @classmethod
    def addGraphicsShader(cls, shader: Optional[Shader], uniforms: Optional[Dict[str, Any]] = None) -> None:
        r"""\brief Add a post-processing graphics shader.

        - \param shader The shader to add.
        - \param uniforms Optional uniforms to set on the shader.
        """
        if IS_IOS_PLATFORM:
            if shader is not None:
                warnIosShaderSkippedOnce(
                    "System.addGraphicsShader",
                    "iOS: shaders are disabled; ignored addGraphicsShader",
                )
            return
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
    def flashScreen(cls, color: Color = Color.White, duration: float = 0.5) -> None:
        r"""\brief Trigger an RMXP-style screen flash effect.

        Loads (and caches) the Flash.frag post-processing shader, attaches it
        to the graphics shader pipeline, and fades its intensity from 1.0 to
        0.0 over the given duration. The shader is removed automatically when
        the flash finishes.

        - \param color Flash color (alpha controls peak strength, 255 = full).
        - \param duration Flash duration in seconds; values <= 0 cancel the flash.
        """
        if duration <= 0.0:
            cls.stopFlash()
            return
        if IS_IOS_PLATFORM:
            warnIosShaderSkippedOnce(
                "System.flashScreen",
                "iOS: shaders are disabled; skipped screen flash effect",
            )
            return
        if cls._flashShader is None:
            from . import Manager

            try:
                cls._flashShader = Manager.ShaderManager.load("Global/Flash.frag")
            except Exception:
                cls._flashShader = None
                logging.error("%s", LOC("FLASH_SHADER_LOAD_FAILED"))
                return
            if cls._flashShader is None:
                return
        cls._flashColour = Vector4f(
            float(color.r) / 255.0,
            float(color.g) / 255.0,
            float(color.b) / 255.0,
            float(color.a) / 255.0,
        )
        cls._flashDuration = float(duration)
        cls._flashTimeCount = 0.0
        if not cls._flashActive:
            cls.addGraphicsShader(cls._flashShader)
            cls._flashActive = True
        cls._flashShader.setUniform("flashColor", cls._flashColour)
        cls._flashShader.setUniform("intensity", 1.0)

    @classmethod
    def stopFlash(cls) -> None:
        r"""\brief Cancel any in-progress screen flash effect."""
        if cls._flashActive and cls._flashShader is not None:
            cls.removeGraphicsShader(cls._flashShader)
        cls._flashActive = False
        cls._flashTimeCount = 0.0
        cls._flashDuration = 0.0

    @classmethod
    def isFlashing(cls) -> bool:
        r"""\brief Check whether a screen flash effect is currently active.

        - \return True if a flash is in progress.
        """
        return cls._flashActive

    @classmethod
    def changeScreenTone(
        cls,
        red: float = 0.0,
        green: float = 0.0,
        blue: float = 0.0,
        gray: float = 0.0,
        duration: float = 0.0,
    ) -> None:
        r"""\brief Change the screen tone over time using RMXP-style tone values.

        Red, green, and blue shift the rendered screen by -255 to 255. Gray
        desaturates the screen from 0 to 255. A duration of 0 applies the tone
        immediately.

        - \param red Red tone adjustment.
        - \param green Green tone adjustment.
        - \param blue Blue tone adjustment.
        - \param gray Gray/desaturation amount.
        - \param duration Time in seconds to reach the target tone.
        """
        if IS_IOS_PLATFORM:
            warnIosShaderSkippedOnce(
                "System.changeScreenTone",
                "iOS: shaders are disabled; skipped screen tone effect",
            )
            return
        if not cls._ensureToneShader():
            return

        targetColour = cls._makeToneColour(red, green, blue, gray)
        cls._toneStartColour = cls._toneCurrentColour
        cls._toneTargetColour = targetColour
        cls._toneDuration = max(0.0, float(duration))
        cls._toneTimeCount = 0.0

        if not cls._toneActive:
            cls._toneActive = True

        if cls._toneDuration <= 0.0:
            cls._toneCurrentColour = targetColour
            cls._applyScreenToneUniform()
            if cls._isNeutralTone(targetColour):
                cls.stopScreenTone()
        else:
            cls._applyScreenToneUniform()

    @classmethod
    def clearScreenTone(cls, duration: float = 0.0) -> None:
        r"""\brief Fade the screen tone back to normal.

        - \param duration Time in seconds to return to the normal tone.
        """
        cls.changeScreenTone(0.0, 0.0, 0.0, 0.0, duration)

    @classmethod
    def stopScreenTone(cls) -> None:
        r"""\brief Immediately remove any active screen tone effect."""
        cls._toneCurrentColour = Vector4f(0.0, 0.0, 0.0, 0.0)
        cls._toneStartColour = Vector4f(0.0, 0.0, 0.0, 0.0)
        cls._toneTargetColour = Vector4f(0.0, 0.0, 0.0, 0.0)
        cls._toneDuration = 0.0
        cls._toneTimeCount = 0.0
        cls._toneActive = False

    @classmethod
    def isScreenToneActive(cls) -> bool:
        r"""\brief Check whether a screen tone effect is currently active.

        - \return True if a non-neutral tone or tone transition is active.
        """
        return cls._toneActive

    @classmethod
    def isScreenToneTransitionComplete(cls) -> bool:
        r"""\brief Check whether the current screen tone transition has finished.

        - \return True when no timed tone transition is in progress.
        """
        return not cls._toneActive or cls._toneDuration <= 0.0

    @classmethod
    def startShake(cls, power: float = 4.0, speed: float = 10.0, duration: float = 0.5) -> None:
        r"""\brief Start an RMXP-style screen shake effect.

        The screen offsets randomly within the remaining power range,
        updating at the given speed, fading linearly over the duration.

        - \param power Maximum shake amplitude in pixels.
        - \param speed Shake update frequency (updates per second).
        - \param duration Shake duration in seconds; values <= 0 cancel the shake.
        """
        if duration <= 0.0:
            cls.stopShake()
            return
        cls._shakePower = float(power)
        cls._shakeSpeed = float(speed)
        cls._shakeDuration = float(duration)
        cls._shakeTimeCount = 0.0
        cls._shakeActive = True
        cls._shakeNextUpdate = 0.0
        cls._shakeOffset = Vector2f(0.0, 0.0)

    @classmethod
    def stopShake(cls) -> None:
        r"""\brief Cancel any in-progress screen shake effect."""
        cls._shakeActive = False
        cls._shakeTimeCount = 0.0
        cls._shakeDuration = 0.0
        cls._shakeOffset = Vector2f(0.0, 0.0)

    @classmethod
    def isShaking(cls) -> bool:
        r"""\brief Check whether a screen shake effect is currently active.

        - \return True if a shake is in progress.
        """
        return cls._shakeActive

    @classmethod
    def _updateShake(cls, deltaTime: float) -> None:
        if not cls._shakeActive:
            return
        cls._shakeTimeCount = min(cls._shakeTimeCount + deltaTime, cls._shakeDuration)
        if cls._shakeTimeCount >= cls._shakeDuration:
            cls.stopShake()
            return
        remainingPower = cls._shakePower * (1.0 - cls._shakeTimeCount / cls._shakeDuration)
        cls._shakeNextUpdate -= deltaTime
        if cls._shakeNextUpdate <= 0.0:
            cls._shakeNextUpdate = 1.0 / cls._shakeSpeed
            cls._shakeOffset = Vector2f(
                random.uniform(-remainingPower, remainingPower),
                random.uniform(-remainingPower, remainingPower),
            )

    @classmethod
    def setTransition(cls, transitionResource: Optional[Texture] = None, transitionTime: float = 1.0) -> None:
        r"""\brief Start a scene transition effect.

        - \param transitionResource Optional texture used as the transition mask.
        - \param transitionTime Duration of the transition in seconds.
        """
        if not cls._transitionShader:
            cls._transitionResource = transitionResource
            cls._inTransition = False
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
    def isTransitionBackgroundFrozen(cls) -> bool:
        r"""\brief Check whether the transition background is ready."""
        return cls._transitionFrozen

    @classmethod
    def isTransitionBackgroundFreezePending(cls) -> bool:
        r"""\brief Check whether a transition background freeze is queued."""
        return cls._transitionFreezePending

    @classmethod
    def cancelTransitionBackgroundFreeze(cls) -> None:
        r"""\brief Cancel a pending or frozen transition background."""
        cls._transitionFreezePending = False
        cls._transitionFrozen = False

    @classmethod
    def requestTransition(cls, transitionName: Optional[str] = None, transitionTime: float = 1.0) -> None:
        r"""\brief Queue a transition to be started on the render thread.

        - \param transitionName Optional transition texture filename.
        - \param transitionTime Duration of the transition in seconds.
        """
        with cls._pendingTransitionLock:
            cls._pendingTransition = (transitionName, float(transitionTime))

    @classmethod
    def cancelPendingTransition(cls) -> None:
        r"""\brief Cancel a queued transition that has not started yet."""
        with cls._pendingTransitionLock:
            cls._pendingTransition = None

    @classmethod
    def isTransitionPending(cls) -> bool:
        r"""\brief Check whether a transition request is queued."""
        with cls._pendingTransitionLock:
            return cls._pendingTransition is not None

    @classmethod
    def isInTransition(cls) -> bool:
        r"""\brief Check whether a transition effect is currently running."""
        return cls._inTransition

    @classmethod
    def applyPendingTransition(cls) -> None:
        r"""\brief Start any queued transition on the render thread."""
        pending: Optional[tuple[Optional[str], float]] = None
        with cls._pendingTransitionLock:
            if cls._pendingTransition is not None:
                pending = cls._pendingTransition
                cls._pendingTransition = None
        if pending is None:
            return
        transitionName, transitionTime = pending
        transitionResource = None
        if transitionName:
            from . import Manager

            transitionResource = Manager.loadTransition(transitionName)
        cls.setTransition(transitionResource, transitionTime)

    @classmethod
    def bindSceneOperationThread(cls) -> None:
        r"""\brief Record the current thread as the only thread that may replace scenes synchronously.

        Call once from the same thread that runs SceneBase.main (window / OpenGL). Embedded
        runtimes may not match threading.main_thread(); use this instead.
        """
        cls._sceneOpThreadIdent = threading.get_ident()

    @classmethod
    def applyPendingSceneReplace(cls) -> None:
        r"""\brief Apply a scene replacement queued from a non-scene-operation thread.

        Window and transition updates must run on the thread bound via bindSceneOperationThread.
        """
        if cls._sceneOpThreadIdent is not None and threading.get_ident() != cls._sceneOpThreadIdent:
            return
        pending: Optional[Any] = None
        with cls._pendingSceneLock:
            if cls._pendingSceneReplace is not None:
                pending = cls._pendingSceneReplace
                cls._pendingSceneReplace = None
        if pending is not None:
            cls._applySetScene(pending)

    @classmethod
    def _applySetScene(cls, scene: SceneBase) -> None:
        cls.freezeTransitionBackground()
        if len(cls._scenes) == 0:
            cls._scenes.append(scene)
        else:
            cls._scenes[-1] = scene

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

        When called from a thread other than the one bound by bindSceneOperationThread, the
        replacement is deferred until applyPendingSceneReplace runs on the bound thread.

        - \param scene The new scene to set.
        """
        tid = threading.get_ident()
        if cls._sceneOpThreadIdent is None or tid == cls._sceneOpThreadIdent:
            cls._applySetScene(scene)
        else:
            with cls._pendingSceneLock:
                cls._pendingSceneReplace = scene

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
    def _updateFlash(cls, deltaTime: float) -> None:
        if not cls._flashActive or cls._flashShader is None:
            return
        cls._flashTimeCount = min(cls._flashTimeCount + deltaTime, cls._flashDuration)
        if cls._flashDuration > 0.0:
            intensity = max(0.0, 1.0 - cls._flashTimeCount / cls._flashDuration)
        else:
            intensity = 0.0
        cls._flashShader.setUniform("flashColor", cls._flashColour)
        cls._flashShader.setUniform("intensity", intensity)
        if cls._flashTimeCount >= cls._flashDuration:
            cls.removeGraphicsShader(cls._flashShader)
            cls._flashActive = False

    @classmethod
    def _updateScreenTone(cls, deltaTime: float) -> None:
        if not cls._toneActive or cls._toneShader is None:
            return
        if cls._toneDuration > 0.0:
            cls._toneTimeCount = min(cls._toneTimeCount + deltaTime, cls._toneDuration)
            ratio = min(1.0, cls._toneTimeCount / cls._toneDuration)
            cls._toneCurrentColour = Vector4f(
                cls._toneStartColour.x + (cls._toneTargetColour.x - cls._toneStartColour.x) * ratio,
                cls._toneStartColour.y + (cls._toneTargetColour.y - cls._toneStartColour.y) * ratio,
                cls._toneStartColour.z + (cls._toneTargetColour.z - cls._toneStartColour.z) * ratio,
                cls._toneStartColour.w + (cls._toneTargetColour.w - cls._toneStartColour.w) * ratio,
            )
        cls._applyScreenToneUniform()
        if cls._toneDuration > 0.0 and cls._toneTimeCount >= cls._toneDuration:
            cls._toneDuration = 0.0
            if cls._isNeutralTone(cls._toneCurrentColour):
                cls.stopScreenTone()

    @classmethod
    def _ensureToneShader(cls) -> bool:
        if cls._toneShader is not None:
            return True
        from . import Manager

        try:
            cls._toneShader = Manager.ShaderManager.load("Global/Tone.frag")
        except Exception:
            cls._toneShader = None
            logging.error("%s", LOC("TONE_SHADER_LOAD_FAILED"))
            return False
        return cls._toneShader is not None

    @classmethod
    def _applyScreenToneUniform(cls) -> None:
        if cls._toneShader is not None:
            cls._toneShader.setUniform("toneColor", cls._toneCurrentColour)

    @classmethod
    def _ensureToneBuffer(cls, size) -> RenderTexture:
        if cls._toneBuffer is None or cls._toneBuffer.getSize() != size:
            cls._toneBuffer = RenderTexture(size)
            cls._toneBufferSprite = Sprite(cls._toneBuffer.getTexture())
        return cls._toneBuffer

    @classmethod
    def _ensureToneBufferSprite(cls) -> Sprite:
        if cls._toneBufferSprite is None:
            cls._toneBufferSprite = Sprite()
        return cls._toneBufferSprite

    @classmethod
    def _makeToneColour(cls, red: float, green: float, blue: float, gray: float) -> Vector4f:
        return Vector4f(
            cls._clamp(float(red), -255.0, 255.0) / 255.0,
            cls._clamp(float(green), -255.0, 255.0) / 255.0,
            cls._clamp(float(blue), -255.0, 255.0) / 255.0,
            cls._clamp(float(gray), 0.0, 255.0) / 255.0,
        )

    @classmethod
    def _isNeutralTone(cls, toneColour: Vector4f) -> bool:
        return (
            abs(toneColour.x) <= 0.0001
            and abs(toneColour.y) <= 0.0001
            and abs(toneColour.z) <= 0.0001
            and abs(toneColour.w) <= 0.0001
        )

    @classmethod
    def _clamp(cls, value: float, minimum: float, maximum: float) -> float:
        return max(minimum, min(maximum, value))

    @classmethod
    def _applyGraphicsShadersLength(cls) -> None:
        if len(cls._graphicsShaders) < len(cls.__graphicsCanvases):
            cls.__graphicsCanvases = cls.__graphicsCanvases[: len(cls._graphicsShaders)]
        elif len(cls._graphicsShaders) > len(cls.__graphicsCanvases):
            cls.__graphicsCanvases += [
                RenderTexture(cls._window.getSize())
                for _ in range(len(cls._graphicsShaders) - len(cls.__graphicsCanvases))
            ]
