# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
import random
from enum import IntEnum
from typing import TYPE_CHECKING, Callable, List, Optional

import Engine
from Engine import (
    Color,
    Particle,
    ParticleInfo,
    ParticleSystem,
    RenderTexture,
    Shader,
    Sprite,
    Vector2f,
    degrees,
)
from Engine.Utils import Math, Render
from Engine.Utils.Inner import IS_IOS_PLATFORM, warnIosShaderSkippedOnce

from .. import Manager
from ..System import System

if TYPE_CHECKING:
    from ..Camera import Camera


class WeatherType(IntEnum):
    r"""\brief RMXP-compatible weather presets."""

    NONE = 0
    RAIN = 1
    STORM = 2
    SNOW = 3

    @classmethod
    def coerce(cls, value) -> WeatherType:
        r"""\brief Resolve enum, int, or blueprint combo string to a weather type.

        - \param value WeatherType, int, enum name, localized label, or digit string.
        - \return Matching WeatherType, or NONE if unrecognised.
        """
        if isinstance(value, cls):
            return value
        if isinstance(value, int):
            try:
                return cls(value)
            except ValueError:
                return cls.NONE
        if isinstance(value, str):
            stripped = value.strip()
            if stripped.isdigit():
                try:
                    return cls(int(stripped))
                except ValueError:
                    return cls.NONE
            try:
                return cls[stripped]
            except KeyError:
                pass
            try:
                from Engine.Locale import LOC

                for member in cls:
                    if stripped == LOC(f"WEATHER_TYPE_{member.name}"):
                        return member
            except Exception:
                pass
            return cls.NONE
        return cls.NONE

    @classmethod
    def dropBoxItems(cls) -> List[str]:
        r"""\brief Localized labels for blueprint DropBox weather type parameter."""
        try:
            from Engine.Locale import LOC

            return [LOC(f"WEATHER_TYPE_{member.name}") for member in cls]
        except Exception:
            return [member.name for member in cls]


_WEATHER_PARTICLE_PATH = os.path.join("./Assets", "Icons", "Potion-1-1-1-1.png")
_STORM_FLASH_COLOUR = Color(210, 220, 255, 120)


class WeatherController:
    r"""\brief Manages active weather state, shader overlay, and particle fallback."""

    _weatherType: WeatherType = WeatherType.NONE
    _power: float = 0.0
    _max: float = 50.0
    _time: float = 0.0
    _stormFlashCooldown: float = 0.0
    _weatherShader: Optional[Shader] = None
    _shaderFailed: bool = False
    _useParticles: bool = IS_IOS_PLATFORM
    _weatherBuffer: Optional[RenderTexture] = None
    _bufferSprite: Optional[Sprite] = None
    _particles: List[Particle] = []
    _particleSystem: Optional[ParticleSystem] = None

    @classmethod
    def setWeather(cls, weatherType: WeatherType, power: int, maxCount: int) -> None:
        r"""\brief Activate weather with RMXP-style parameters.

        - \param weatherType Weather preset (NONE, RAIN, STORM, or SNOW).
        - \param power Effect strength from 0 to 100.
        - \param maxCount Particle density cap from 0 to 100.
        """
        cls.clearWeather()
        resolved = WeatherType.coerce(weatherType)
        cls._weatherType = resolved
        cls._power = float(Math.Clamp(int(power), 0, 100))
        cls._max = float(Math.Clamp(int(maxCount), 0, 100))
        if cls._weatherType == WeatherType.NONE or cls._power <= 0.0:
            cls._weatherType = WeatherType.NONE
            return
        cls._useParticles = IS_IOS_PLATFORM or cls._shaderFailed
        if not cls._useParticles:
            cls._ensureShader()
            if cls._weatherShader is None:
                cls._useParticles = True
        if cls._useParticles and cls._particleSystem is not None:
            cls._spawnParticles()

    @classmethod
    def clearWeather(cls) -> None:
        r"""\brief Stop weather and release particles."""
        cls._weatherType = WeatherType.NONE
        cls._power = 0.0
        cls._clearParticles()

    @classmethod
    def registerParticleSystem(cls, particleSystem: ParticleSystem) -> None:
        r"""\brief Bind the map particle system used for iOS fallback rendering.

        - \param particleSystem Target particle system owned by the active map.
        """
        if cls._particleSystem is particleSystem:
            return
        cls._clearParticles()
        cls._particleSystem = particleSystem
        if cls._useParticles and cls._weatherType != WeatherType.NONE and cls._power > 0.0:
            cls._spawnParticles()

    @classmethod
    def update(cls, deltaTime: float) -> None:
        r"""\brief Advance weather timers and storm flashes.

        - \param deltaTime Elapsed time in seconds.
        """
        if cls._weatherType == WeatherType.NONE or cls._power <= 0.0:
            return
        cls._time += deltaTime
        if cls._weatherType == WeatherType.STORM and not cls._useParticles:
            cls._stormFlashCooldown = max(0.0, cls._stormFlashCooldown - deltaTime)
            if cls._stormFlashCooldown <= 0.0 and random.random() < 0.02 * (cls._power / 100.0):
                flashDuration = 0.08 + random.random() * 0.07
                System.flashScreen(_STORM_FLASH_COLOUR, flashDuration)
                cls._stormFlashCooldown = 0.35 + random.random() * 0.65

    @classmethod
    def drawShaderOverlay(cls, camera: Camera) -> None:
        r"""\brief Apply weather shader on the camera render target before blitting to the canvas.

        - \param camera Active map camera; must have finished rendering the current frame.
        """
        if cls._useParticles or cls._weatherType == WeatherType.NONE or cls._power <= 0.0:
            return
        if cls._weatherShader is None or camera is None:
            return
        rt = camera.getRenderTexture()
        size = rt.getSize()
        buffer = cls._ensureBuffer(size)
        sprite = cls._ensureBufferSprite()
        sourceTex = rt.getTexture()
        sprite.setTexture(sourceTex, True)
        sprite.setPosition(Vector2f(0.0, 0.0))
        sprite.setScale(Vector2f(1.0, 1.0))
        shader = cls._weatherShader
        shader.setUniform("screenTex", sourceTex)
        shader.setUniform("texSize", Math.ToVector2f(size))
        shader.setUniform("time", cls._time)
        shader.setUniform("weatherType", float(cls._weatherType))
        shader.setUniform("power", cls._power / 100.0)
        shader.setUniform("maxScale", max(0.1, cls._max / 50.0))
        buffer.clear(Color.Transparent)
        states = Render.CanvasRenderStates()
        states.shader = shader
        buffer.draw(sprite, states)
        buffer.display()
        savedView = rt.getView()
        rt.clear(Color.Transparent)
        rt.setView(rt.getDefaultView())
        sprite.setTexture(buffer.getTexture(), True)
        rt.draw(sprite, Render.CanvasRenderStates())
        rt.setView(savedView)
        rt.display()

    @classmethod
    def getWeatherType(cls) -> WeatherType:
        r"""\brief Get the active weather preset."""
        return cls._weatherType

    @classmethod
    def _ensureShader(cls) -> None:
        if cls._weatherShader is not None or cls._shaderFailed:
            return
        if IS_IOS_PLATFORM:
            cls._shaderFailed = True
            return
        try:
            cls._weatherShader = Manager.ShaderManager.load("Weather.frag")
        except Exception:
            cls._weatherShader = None
            cls._shaderFailed = True
            warnIosShaderSkippedOnce(
                "WeatherController.shader",
                "Weather shader failed to load; falling back to particle weather",
            )

    @classmethod
    def _ensureBuffer(cls, size) -> RenderTexture:
        if cls._weatherBuffer is None or cls._weatherBuffer.getSize() != size:
            cls._weatherBuffer = RenderTexture(size)
            cls._bufferSprite = Sprite(cls._weatherBuffer.getTexture())
        return cls._weatherBuffer

    @classmethod
    def _ensureBufferSprite(cls) -> Sprite:
        if cls._bufferSprite is None:
            cls._bufferSprite = Sprite()
        return cls._bufferSprite

    @classmethod
    def _clearParticles(cls) -> None:
        if not cls._particles:
            return
        for particle in cls._particles:
            try:
                particle.destroy()
            except Exception:
                pass
        cls._particles.clear()

    @classmethod
    def _spawnParticles(cls) -> None:
        if cls._particleSystem is None:
            return
        cls._clearParticles()
        count = cls._particleCount()
        if count <= 0:
            return
        width = float(Engine.GameSize.x)
        height = float(Engine.GameSize.y)
        mover = cls._buildMover(width, height)
        color, scale, rotation = cls._particleStyle()
        for _ in range(count):
            info = ParticleInfo()
            info.position = Vector2f(random.uniform(0.0, width), random.uniform(-height * 0.2, height))
            info.color = color
            info.rotation = rotation
            info.scale = scale
            particle = Particle(
                cls._particleSystem,
                mover,
                random.uniform(0.0, 2.0),
                _WEATHER_PARTICLE_PATH,
                info,
            )
            cls._particleSystem.addParticle(particle)
            cls._particles.append(particle)

    @classmethod
    def _particleCount(cls) -> int:
        powerNorm = cls._power / 100.0
        maxNorm = max(cls._max, 1.0) / 100.0
        base = 12.0 + maxNorm * 188.0
        if cls._weatherType == WeatherType.STORM:
            base *= 1.35
        elif cls._weatherType == WeatherType.SNOW:
            base *= 0.85
        return int(base * powerNorm)

    @classmethod
    def _particleStyle(cls):
        if cls._weatherType == WeatherType.SNOW:
            alpha = int(140 + 80 * (cls._power / 100.0))
            return (
                Color(245, 248, 255, alpha),
                Vector2f(0.12 + 0.08 * random.random(), 0.12 + 0.08 * random.random()),
                degrees(random.uniform(0.0, 360.0)),
            )
        alpha = int(120 + 90 * (cls._power / 100.0))
        stretch = 0.55 if cls._weatherType == WeatherType.STORM else 0.45
        return (
            Color(180, 200, 235, alpha),
            Vector2f(0.05 + 0.03 * random.random(), stretch + 0.15 * random.random()),
            degrees(-20.0 + random.uniform(-8.0, 8.0)),
        )

    @classmethod
    def _buildMover(cls, width: float, height: float) -> Callable:
        if cls._weatherType == WeatherType.SNOW:
            wind = 18.0
            fall = 55.0 + 35.0 * (cls._power / 100.0)

            def mover(deltaTime: float, _: float, particle: Particle) -> None:
                info = particle.info
                info.position.x += wind * deltaTime
                info.position.y += fall * deltaTime
                if info.position.y > height + 16.0:
                    info.position.y = random.uniform(-24.0, -4.0)
                    info.position.x = random.uniform(0.0, width)
                if info.position.x > width + 16.0:
                    info.position.x = -16.0
                elif info.position.x < -16.0:
                    info.position.x = width + 16.0

            return mover

        wind = -70.0 if cls._weatherType == WeatherType.STORM else -55.0
        fall = 360.0 if cls._weatherType == WeatherType.STORM else 280.0
        fall *= 0.75 + 0.5 * (cls._power / 100.0)

        def mover(deltaTime: float, _: float, particle: Particle) -> None:
            info = particle.info
            info.position.x += wind * deltaTime
            info.position.y += fall * deltaTime
            if info.position.y > height + 24.0:
                info.position.y = random.uniform(-48.0, -8.0)
                info.position.x = random.uniform(0.0, width)

        return mover
