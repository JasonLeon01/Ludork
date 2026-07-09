# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
from typing import ClassVar, Dict, List, Optional, Tuple, Union, TYPE_CHECKING
from .. import (
    TypeAdapter,
    Pair,
    Sprite,
    IntRect,
    Vector2i,
    Vector2f,
    Texture,
    RenderTexture,
    RectBase,
    Curve,
)
from ..Utils import Math, Render
from .Base import SpriteBase

if TYPE_CHECKING:
    from Engine import Vector2u, Image


SELECTION_RECT_OPACITY_CURVE_KEY = "UI/SelectionRectOpacity"
_FALLBACK_FADE_SPEED = 96.0
_FALLBACK_OPACITY_RANGE: Tuple[float, float] = (128.0, 255.0)


class Rect(SpriteBase):
    r"""Rectangle widget with a fading opacity effect and skin-based rendering.

    Renders a rectangular area using a window skin texture with support
    for opacity fading animation driven by a curve asset.
    """

    _opacityCurves: ClassVar[Dict[str, Curve]] = {}

    @TypeAdapter(rect=([tuple, list], IntRect, lambda pos, size: IntRect(Vector2i(*pos), Vector2i(*size))))
    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]], List[List[int]]],
        windowSkin: Image,
        opacityCurveKey: str = SELECTION_RECT_OPACITY_CURVE_KEY,
    ) -> None:
        r"""\brief Construct a Rect widget with curve-driven opacity.

        - \param rect             Logical position and size of the rectangle
        - \param windowSkin       Texture used for rendering the rectangle skin
        - \param opacityCurveKey  Curve asset key under Data/Curves
        """
        from .. import Scale

        self._size = Math.ToVector2u(rect.size)
        size = Math.ToVector2u(Math.ToVector2f(rect.size) * Scale)
        self._canvas: RenderTexture = RenderTexture(size)
        self._windowSkin = windowSkin
        self._rectImpl = RectBase()
        self._initUI()
        SpriteBase.__init__(self, self._canvas.getTexture())
        self.setPosition(Math.ToVector2f(rect.position))
        self._opacityCurveKey = opacityCurveKey
        self._opacityTime = 0.0
        self._opacity = float(self.getColour().a)
        self._opacityMultiplier = 1.0
        self._fading = True

    def setOpacityMultiplier(self, multiplier: float) -> None:
        r"""\brief Set a multiplier applied to the animated opacity.

        - \param multiplier  Opacity multiplier clamped between 0 and 1
        """
        self._opacityMultiplier = max(0.0, min(1.0, multiplier))
        self._applyOpacity()

    def getSize(self) -> Vector2u:
        r"""\brief Get the rectangle size in logical UI units.

        - \return  Rectangle size in logical UI units
        """
        return self._size

    def update(self, deltaTime: float) -> None:
        r"""\brief Update the curve-driven opacity animation.

        - \param deltaTime  Time elapsed since last update, in seconds
        """
        curve = self._getOpacityCurve()
        if curve is not None and curve.keys:
            duration = self._getOpacityDuration(curve)
            if duration > 0.0:
                self._opacityTime = (self._opacityTime + deltaTime) % duration
                sampleTime = float(curve.keys[0]["time"]) + self._opacityTime
                self._opacity = curve.evaluate(sampleTime)
            else:
                self._opacity = curve.evaluate(float(curve.keys[0]["time"]))
        else:
            self._updateFallbackOpacity(deltaTime)
        self._applyOpacity()

    def _getOpacityCurve(self) -> Optional[Curve]:
        if self._opacityCurveKey in Rect._opacityCurves:
            return Rect._opacityCurves[self._opacityCurveKey]
        try:
            from Source import Data

            curve = Data.getCurve(self._opacityCurveKey)
        except KeyError:
            return None
        Rect._opacityCurves[self._opacityCurveKey] = curve
        return curve

    def _getOpacityDuration(self, curve: Curve) -> float:
        if len(curve.keys) < 2:
            return 0.0
        return float(curve.keys[-1]["time"]) - float(curve.keys[0]["time"])

    def _updateFallbackOpacity(self, deltaTime: float) -> None:
        opacity = self._opacity
        opacityMin, opacityMax = _FALLBACK_OPACITY_RANGE
        if self._fading:
            opacity = max(opacityMin, opacity - _FALLBACK_FADE_SPEED * deltaTime)
            if opacity == opacityMin:
                self._fading = False
        else:
            opacity = min(opacityMax, opacity + _FALLBACK_FADE_SPEED * deltaTime)
            if opacity == opacityMax:
                self._fading = True
        self._opacity = opacity

    def _applyOpacity(self) -> None:
        colour = copy.copy(self.getColour())
        colour.a = int(max(0.0, min(255.0, self._opacity * self._opacityMultiplier)))
        self.setColour(colour)

    def _presave(self, target: List[Texture], area: List[IntRect]) -> None:
        target.clear()
        for i, _ in enumerate(area):
            sub_texture = Texture(self._windowSkin, False, area[i])
            target.append(sub_texture)

    def _initUI(self) -> None:
        self._windowEdge = RenderTexture(self._canvas.getSize())
        self._windowEdgeSprite = Sprite(self._windowEdge.getTexture())
        self._windowBack = Texture(self._windowSkin, False, Math.ToIntRect(132, 68, 24, 24))
        self._windowBackSprite = Sprite(self._windowBack)
        canvasSize = self._canvas.getSize()
        self._windowBackSprite.setScale(Vector2f(canvasSize.x / 24.0, canvasSize.y / 24.0))
        self._windowHintSprites: List[Sprite] = []
        self._pauseHintSprite: Sprite = None
        self._cachedCorners: List[Texture] = []
        self._cachedEdges: List[Texture] = []
        self._presave(
            self._cachedCorners,
            [
                Math.ToIntRect(128, 64, 4, 4),
                Math.ToIntRect(156, 64, 4, 4),
                Math.ToIntRect(128, 92, 4, 4),
                Math.ToIntRect(156, 92, 4, 4),
            ],
        )
        self._presave(
            self._cachedEdges,
            [
                Math.ToIntRect(132, 64, 24, 4),
                Math.ToIntRect(132, 92, 24, 4),
                Math.ToIntRect(128, 68, 4, 24),
                Math.ToIntRect(156, 68, 4, 24),
            ],
        )
        for edge in self._cachedEdges:
            edge.setRepeated(True)
        self._rectImpl.render(
            self._canvas,
            self._windowEdge,
            self._windowEdgeSprite,
            self._windowBackSprite,
            self._cachedCorners,
            self._cachedEdges,
            Render.CanvasRenderStates(),
        )
