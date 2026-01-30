# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
from typing import List, Optional, Tuple, Union, TYPE_CHECKING
from .. import (
    Sprite,
    IntRect,
    Vector2i,
    Vector2f,
    Texture,
    RenderTexture,
    Utils,
)
from .Base import SpriteBase, RectBase

if TYPE_CHECKING:
    from Engine import Vector2u, Image


class Rect(SpriteBase, RectBase):
    def __init__(
        self,
        rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]], List[List[int]]],
        windowSkin: Optional[Image] = None,
        fadeSpeed: float = 96,
        opacityRange: Tuple[float, float] = (128, 255),
    ) -> None:
        assert isinstance(rect, (IntRect, Tuple, List)), "rect must be a tuple, list or IntRect"
        if isinstance(rect, (tuple, list)):
            position, size = rect
            x, y = position
            w, h = size
            position = Vector2i(x, y)
            size = Vector2i(w, h)
            rect = IntRect(position, size)
        self._size = Utils.Math.ToVector2u(rect.size)
        size = Utils.Math.ToVector2u(Utils.Render.getRealSize(rect.size))
        self._canvas: RenderTexture = RenderTexture(size)
        if windowSkin:
            self._windowSkin = windowSkin
        else:
            from .. import System, Manager

            self._windowSkin = Manager.loadSystem(System.getWindowskinName(), smooth=True).copyToImage()
        self._initUI()
        super().__init__(self._canvas.getTexture())
        self.setPosition(Utils.Math.ToVector2f(rect.position))
        self._fadeSpeed = fadeSpeed
        self._opacityRange = opacityRange
        self._fading: bool = True

    def getSize(self) -> Vector2u:
        return self._size

    def update(self, deltaTime: float) -> None:
        color = copy.copy(self.getColor())
        a = color.a
        opacityMin, opacityMax = self._opacityRange
        if self._fading:
            a = max(opacityMin, a - self._fadeSpeed * deltaTime)
            if a == opacityMin:
                self._fading = False
        else:
            a = min(opacityMax, a + self._fadeSpeed * deltaTime)
            if a == opacityMax:
                self._fading = True
        color.a = int(a)
        self.setColor(color)

    def _presave(self, target: List[Texture], area: List[IntRect]) -> None:
        target.clear()
        for i, _ in enumerate(area):
            sub_texture = Texture(self._windowSkin, False, area[i])
            target.append(sub_texture)

    def _initUI(self) -> None:
        self._windowEdge = RenderTexture(self._canvas.getSize())
        self._windowEdgeSprite = Sprite(self._windowEdge.getTexture())
        self._windowBack = Texture(self._windowSkin, False, Utils.Math.ToIntRect(132, 68, 24, 24))
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
                Utils.Math.ToIntRect(128, 64, 4, 4),
                Utils.Math.ToIntRect(156, 64, 4, 4),
                Utils.Math.ToIntRect(128, 92, 4, 4),
                Utils.Math.ToIntRect(156, 92, 4, 4),
            ],
        )
        self._presave(
            self._cachedEdges,
            [
                Utils.Math.ToIntRect(132, 64, 24, 4),
                Utils.Math.ToIntRect(132, 92, 24, 4),
                Utils.Math.ToIntRect(128, 68, 4, 24),
                Utils.Math.ToIntRect(156, 68, 4, 24),
            ],
        )
        for edge in self._cachedEdges:
            edge.setRepeated(True)
        self._render(
            self._canvas,
            self._windowEdge,
            self._windowEdgeSprite,
            self._windowBackSprite,
            self._cachedCorners,
            self._cachedEdges,
        )
