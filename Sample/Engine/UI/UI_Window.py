# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Optional, Tuple, Union, TYPE_CHECKING
from . import (
    UI_SpriteBase,
    UI_RectBase,
    Sprite,
    IntRect,
    Vector2i,
    Vector2f,
    Texture,
    RenderTexture,
    View,
    Utils,
)

if TYPE_CHECKING:
    from Engine import Vector2u, Image
    from Engine.UI import Canvas

SpriteBase = UI_SpriteBase.SpriteBase
RectBase = UI_RectBase.RectBase


class Window(SpriteBase, RectBase):
    def __init__(
        self,
        rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]],
        windowSkin: Optional[Image] = None,
        repeated: bool = False,
    ) -> None:
        from Engine import System

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
            from Engine import Manager

            self._windowSkin = Manager.loadSystem(System.getWindowskinName(), smooth=True)
        self._repeated = repeated
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
        self._windowBack = Texture(self._windowSkin, False, Utils.Math.ToIntRect(0, 0, 128, 128))
        self._windowBack.setRepeated(self._repeated)
        self._windowBackSprite = Sprite(self._windowBack)
        if self._repeated:
            self._windowBackSprite.setTextureRect(
                IntRect(Vector2i(0, 0), Utils.Math.ToVector2i(self.getLocalBounds().size()))
            )
        else:
            canvasSize = self._canvas.getSize()
            self._windowBackSprite.setScale(Vector2f(canvasSize.x / 128.0, canvasSize.y / 128.0))
        self._windowHintSprites: List[Sprite] = []
        self._pauseHintSprite: Sprite = None
        self._cachedCorners: List[Texture] = []
        self._cachedEdges: List[Texture] = []
        self._presave(
            self._cachedCorners,
            [
                Utils.Math.ToIntRect(128, 0, 16, 16),
                Utils.Math.ToIntRect(176, 0, 16, 16),
                Utils.Math.ToIntRect(128, 48, 16, 16),
                Utils.Math.ToIntRect(176, 48, 16, 16),
            ],
        )
        self._presave(
            self._cachedEdges,
            [
                Utils.Math.ToIntRect(144, 0, 24, 16),
                Utils.Math.ToIntRect(144, 48, 24, 16),
                Utils.Math.ToIntRect(128, 16, 16, 24),
                Utils.Math.ToIntRect(176, 16, 16, 24),
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
