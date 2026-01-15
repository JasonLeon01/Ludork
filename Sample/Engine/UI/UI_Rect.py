# -*- encoding: utf-8 -*-

from __future__ import annotations
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
from .UI_SpriteBase import SpriteBase
from .UI_RectBase import RectBase

if TYPE_CHECKING:
    from Engine import Vector2u, Image
    from Engine.UI import Canvas


class Rect(SpriteBase, RectBase):
    def __init__(
        self,
        rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]],
        windowSkin: Optional[Image] = None,
    ) -> None:
        assert isinstance(rect, (IntRect, tuple)), "rect must be a tuple or IntRect"
        if not isinstance(rect, IntRect):
            position, size = rect
            x, y = position
            w, h = size
            position = Vector2i(x, y)
            size = Vector2i(w, h)
            rect = IntRect(position, size)
        self._size = Utils.Math.ToVector2u(rect.size)
        size = Utils.Math.ToVector2u(Utils.Render.getRealSize(rect.size))
        self._canvas: RenderTexture = RenderTexture(size)
        self._parent: Optional[Canvas] = None
        if windowSkin:
            self._windowSkin = windowSkin
        else:
            from .. import System, Manager

            self._windowSkin = Manager.loadSystem(System.getWindowskinName(), smooth=True)
        self._initUI()
        super().__init__(self._canvas.getTexture())
        self.setPosition(Utils.Math.ToVector2f(rect.position))

    def getSize(self) -> Vector2u:
        return self._size

    def setParent(self, parent: Optional[Canvas]) -> None:
        self._parent = parent

    def getParent(self) -> Optional[Canvas]:
        return self._parent

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
