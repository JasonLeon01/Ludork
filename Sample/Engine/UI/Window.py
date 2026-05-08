# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Tuple, Union
from .. import (
    TypeAdapter,
    Pair,
    Sprite,
    IntRect,
    Vector2i,
    Vector2f,
    Texture,
    RenderTexture,
    Vector2u,
    Image,
    RectBase,
)
from ..Utils import Math, Render
from .Base import SpriteBase


class Window(SpriteBase):
    r"""Window widget rendered with a skin texture and optional repeated background.

    Provides a framed window area backed by a RenderTexture.
    """

    @TypeAdapter(
        rect=(
            [tuple, list],
            IntRect,
            lambda pos, size: IntRect(Vector2i(*pos), Vector2i(*size)),
        )
    )
    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]], List[List[int]]],
        windowSkin: Image,
        repeated: bool = False,
    ) -> None:
        r"""\brief Construct a Window with a given rectangle and skin.

        - \param rect        Logical position and size of the window
        - \param windowSkin  Texture used for rendering the window skin
        - \param repeated    Whether the background texture should be repeated
        """
        from .. import Scale

        self._size = Math.ToVector2u(rect.size)
        size = Math.ToVector2u(Math.ToVector2f(rect.size) * Scale)
        self._canvas: RenderTexture = RenderTexture(size)
        self._windowSkin = windowSkin
        self._repeated = repeated
        self._rectImpl = RectBase()
        self._initUI()
        SpriteBase.__init__(self, self._canvas.getTexture())
        self.setPosition(Math.ToVector2f(rect.position))

    def getSize(self) -> Vector2u:
        r"""\brief Get the window size in logical UI units.

        - \return  Window size in logical UI units
        """
        return self._size

    def _presave(self, target: List[Texture], area: List[IntRect]) -> None:
        target.clear()
        for i, _ in enumerate(area):
            sub_texture = Texture(self._windowSkin, False, area[i])
            target.append(sub_texture)

    def _initUI(self) -> None:
        self._windowEdge = RenderTexture(self._canvas.getSize())
        self._windowEdgeSprite = Sprite(self._windowEdge.getTexture())
        self._windowBack = Texture(
            self._windowSkin, False, Math.ToIntRect(0, 0, 128, 128)
        )
        self._windowBack.setRepeated(self._repeated)
        self._windowBackSprite = Sprite(self._windowBack)
        if self._repeated:
            self._windowBackSprite.setTextureRect(
                IntRect(Vector2i(0, 0), Math.ToVector2i(self.getLocalBounds().size))
            )
        else:
            canvasSize = self._canvas.getSize()
            self._windowBackSprite.setScale(
                Vector2f(canvasSize.x / 128.0, canvasSize.y / 128.0)
            )
        self._windowHintSprites: List[Sprite] = []
        self._pauseHintSprite: Sprite = None
        self._cachedCorners: List[Texture] = []
        self._cachedEdges: List[Texture] = []
        self._presave(
            self._cachedCorners,
            [
                Math.ToIntRect(128, 0, 16, 16),
                Math.ToIntRect(176, 0, 16, 16),
                Math.ToIntRect(128, 48, 16, 16),
                Math.ToIntRect(176, 48, 16, 16),
            ],
        )
        self._presave(
            self._cachedEdges,
            [
                Math.ToIntRect(144, 0, 24, 16),
                Math.ToIntRect(144, 48, 24, 16),
                Math.ToIntRect(128, 16, 16, 24),
                Math.ToIntRect(176, 16, 16, 24),
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
