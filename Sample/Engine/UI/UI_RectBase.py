# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, TYPE_CHECKING
from . import (
    Sprite,
    Vector2f,
    Color,
    Utils,
)

if TYPE_CHECKING:
    from Engine import Texture, RenderTexture, RenderTarget


class RectBase:
    def _renderCorners(self, dst: RenderTarget, areaCaches: List[Texture], cornerPositions: List[Vector2f]) -> None:
        for i in range(4):
            cornerSprite = Sprite(areaCaches[i])
            cornerSprite.setPosition(cornerPositions[i])
            dst.draw(cornerSprite)

    def _renderEdges(self, dst: RenderTarget, areaCaches: List[Texture], edgePositions: List[Vector2f]) -> None:
        cornerW = edgePositions[0].x
        cornerH = edgePositions[2].y
        w, h = dst.getSize().x, dst.getSize().y
        for i in range(4):
            edgeSprite = Sprite(areaCaches[i])
            if i < 2:
                edgeSprite.setTextureRect(
                    Utils.Math.ToIntRect(0, 0, int(w - 2 * cornerW), int(areaCaches[i].getSize().y))
                )
            else:
                edgeSprite.setTextureRect(
                    Utils.Math.ToIntRect(0, 0, int(areaCaches[i].getSize().x), int(h - 2 * cornerH))
                )
            edgeSprite.setPosition(edgePositions[i])
            dst.draw(edgeSprite)

    def _renderSides(self, edge: RenderTexture, cachedCorners: List[Texture], cachedEdges: List[Texture]) -> None:
        edge.clear(Color.Transparent)
        canvasSize = edge.getSize()
        cornerPositions = [
            Vector2f(0, 0),
            Vector2f(canvasSize.x - cachedCorners[1].getSize().x, 0),
            Vector2f(0, canvasSize.y - cachedCorners[2].getSize().y),
            Vector2f(canvasSize.x - cachedCorners[3].getSize().x, canvasSize.y - cachedCorners[3].getSize().y),
        ]
        edgePositions = [
            Vector2f(cachedCorners[0].getSize().x, 0),
            Vector2f(cachedCorners[1].getSize().x, canvasSize.y - cachedCorners[1].getSize().y),
            Vector2f(0, cachedCorners[2].getSize().y),
            Vector2f(canvasSize.x - cachedCorners[3].getSize().x, cachedCorners[3].getSize().y),
        ]
        self._renderCorners(edge, cachedCorners, cornerPositions)
        self._renderEdges(edge, cachedEdges, edgePositions)
        edge.display()

    def _render(
        self,
        dst: RenderTexture,
        edge: RenderTexture,
        edgeSprite: Sprite,
        backSprite: Sprite,
        cachedCorners: List[Texture],
        cachedEdges: List[Texture],
    ) -> None:
        self._renderSides(edge, cachedCorners, cachedEdges)
        dst.clear(Color.Transparent)
        dst.draw(backSprite, Utils.Render.CanvasRenderState())
        dst.draw(edgeSprite, Utils.Render.CanvasRenderState())
        dst.display()
