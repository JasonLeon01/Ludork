# -*- encoding: utf-8 -*-
r"""\brief Full-screen map fog with RMXP-style scrolling and optional PC shader distortion."""

from __future__ import annotations

from typing import Any, Dict, Optional

import Engine
from Engine import Color, IntRect, RenderTexture, Shader, Sprite, Texture, Vector2f, Vector2i
from Engine.Utils import Math, Render
from Engine.Utils.Inner import IS_IOS_PLATFORM, warnIosShaderSkippedOnce

from .. import Manager
from ..System import System


class FogController:
    r"""\brief Map fog driven by map data: texture, strength, scroll speed, and PC-only distortion."""

    _graphic: str = ""
    _power: float = 0.0
    _scroll: Vector2f = Vector2f(0.0, 0.0)
    _distort: float = 0.0
    _offset: Vector2f = Vector2f(0.0, 0.0)
    _time: float = 0.0
    _active: bool = False
    _fogTexture: Optional[Texture] = None
    _fogSprite: Optional[Sprite] = None
    _fogShader: Optional[Shader] = None
    _shaderFailed: bool = False
    _fogBuffer: Optional[RenderTexture] = None
    _bufferSprite: Optional[Sprite] = None

    @classmethod
    def applyFromMapData(cls, mapData: Dict[str, Any]) -> None:
        r"""\brief Configure fog from map settings.

        - \param mapData Serialized map dictionary.
        """
        cls.clearFog()
        graphic = str(mapData.get("fog", "")).strip()
        power = int(mapData.get("fogPower", 0))
        if not graphic or power <= 0:
            return
        cls._graphic = graphic
        cls._power = float(Math.Clamp(power, 0, 100))
        cls._scroll = Vector2f(float(mapData.get("fogOx", 0)), float(mapData.get("fogOy", 0)))
        cls._distort = float(Math.Clamp(int(mapData.get("fogDistort", 0)), 0, 100))
        cls._offset = Vector2f(0.0, 0.0)
        cls._time = 0.0
        if not cls._loadFogTexture():
            cls.clearFog()
            return
        cls._active = True
        if IS_IOS_PLATFORM:
            cls._ensureIosSprite()
        else:
            cls._ensureShader()

    @classmethod
    def clearFog(cls) -> None:
        r"""\brief Disable fog and release runtime resources."""
        cls._active = False
        cls._graphic = ""
        cls._power = 0.0
        cls._scroll = Vector2f(0.0, 0.0)
        cls._distort = 0.0
        cls._offset = Vector2f(0.0, 0.0)
        cls._time = 0.0
        cls._fogTexture = None
        cls._fogSprite = None

    @classmethod
    def update(cls, deltaTime: float) -> None:
        r"""\brief Advance fog scroll offset.

        - \param deltaTime Elapsed time in seconds.
        """
        if not cls._active or cls._power <= 0.0:
            return
        cls._time += deltaTime
        cls._offset.x += cls._scroll.x * deltaTime
        cls._offset.y += cls._scroll.y * deltaTime
        if IS_IOS_PLATFORM and cls._fogSprite is not None and cls._fogTexture is not None:
            cls._updateIosSprite()

    @classmethod
    def drawOverlay(cls) -> None:
        r"""\brief Draw fog on the main canvas after the lit map pass."""
        if not cls._active or cls._power <= 0.0 or cls._fogTexture is None:
            return
        canvas = System.getCanvas()
        if canvas is None:
            return
        if IS_IOS_PLATFORM or cls._fogShader is None:
            cls._drawIosOverlay(canvas)
            return
        cls._drawShaderOverlay(canvas)

    @classmethod
    def _loadFogTexture(cls) -> bool:
        try:
            texture = Manager.loadFog(cls._graphic, smooth=True)
            texture.setRepeated(True)
            cls._fogTexture = texture
            return True
        except Exception as exc:
            print(f"Warning: Failed to load fog texture '{cls._graphic}': {exc}")
            return False

    @classmethod
    def _ensureShader(cls) -> None:
        if cls._fogShader is not None or cls._shaderFailed:
            return
        if IS_IOS_PLATFORM:
            cls._shaderFailed = True
            return
        try:
            cls._fogShader = Manager.ShaderManager.load("Fog.frag")
        except Exception:
            cls._fogShader = None
            cls._shaderFailed = True
            warnIosShaderSkippedOnce(
                "FogController.shader",
                "Fog shader failed to load; falling back to sprite fog",
            )

    @classmethod
    def _ensureIosSprite(cls) -> None:
        if cls._fogTexture is None:
            return
        gameSize = Math.ToVector2u(System.getGameSize())
        if cls._fogSprite is None:
            cls._fogSprite = Sprite(cls._fogTexture)
        else:
            cls._fogSprite.setTexture(cls._fogTexture, True)
        cls._fogSprite.setTextureRect(IntRect(Vector2i(0, 0), Vector2i(int(gameSize.x), int(gameSize.y))))
        cls._updateIosSprite()

    @classmethod
    def _updateIosSprite(cls) -> None:
        if cls._fogSprite is None or cls._fogTexture is None:
            return
        texSize = cls._fogTexture.getSize()
        tw = max(1, int(texSize.x))
        th = max(1, int(texSize.y))
        ox = cls._offset.x % float(tw)
        oy = cls._offset.y % float(th)
        if ox < 0.0:
            ox += float(tw)
        if oy < 0.0:
            oy += float(th)
        cls._fogSprite.setPosition(Vector2f(-ox, -oy))
        alpha = int(255.0 * cls._power / 100.0)
        cls._fogSprite.setColor(Color(255, 255, 255, alpha))

    @classmethod
    def _drawIosOverlay(cls, canvas: RenderTexture) -> None:
        if cls._fogSprite is None:
            cls._ensureIosSprite()
        if cls._fogSprite is None:
            return
        canvas.draw(cls._fogSprite, Render.CanvasRenderStates())

    @classmethod
    def _drawShaderOverlay(cls, canvas: RenderTexture) -> None:
        if cls._fogShader is None or cls._fogTexture is None:
            cls._drawIosOverlay(canvas)
            return
        size = canvas.getSize()
        buffer = cls._ensureBuffer(size)
        sprite = cls._ensureBufferSprite()
        sourceTex = canvas.getTexture()
        sprite.setTexture(sourceTex, True)
        sprite.setPosition(Vector2f(0.0, 0.0))
        sprite.setScale(Vector2f(1.0, 1.0))
        texSize = cls._fogTexture.getSize()
        fogScroll = Vector2f(
            cls._offset.x / max(1.0, float(texSize.x)),
            cls._offset.y / max(1.0, float(texSize.y)),
        )
        shader = cls._fogShader
        shader.setUniform("screenTex", sourceTex)
        shader.setUniform("fogTex", cls._fogTexture)
        shader.setUniform("texSize", Math.ToVector2f(size))
        shader.setUniform("fogScroll", fogScroll)
        shader.setUniform("power", cls._power / 100.0)
        shader.setUniform("distort", cls._distort / 100.0)
        shader.setUniform("time", cls._time)
        buffer.clear(Color.Transparent)
        states = Render.CanvasRenderStates()
        states.shader = shader
        buffer.draw(sprite, states)
        buffer.display()
        savedView = canvas.getView()
        canvas.clear(Color.Transparent)
        canvas.setView(canvas.getDefaultView())
        sprite.setTexture(buffer.getTexture(), True)
        canvas.draw(sprite, Render.CanvasRenderStates())
        canvas.setView(savedView)
        canvas.display()

    @classmethod
    def _ensureBuffer(cls, size) -> RenderTexture:
        if cls._fogBuffer is None or cls._fogBuffer.getSize() != size:
            cls._fogBuffer = RenderTexture(size)
            cls._bufferSprite = Sprite(cls._fogBuffer.getTexture())
        return cls._fogBuffer

    @classmethod
    def _ensureBufferSprite(cls) -> Sprite:
        if cls._bufferSprite is None:
            cls._bufferSprite = Sprite()
        return cls._bufferSprite
