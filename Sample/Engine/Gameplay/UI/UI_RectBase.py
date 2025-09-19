# -*- encoding: utf-8 -*-

from typing import List
from . import (
    Sprite,
    Vector2f,
    Color,
    Texture,
    RenderTexture,
    RenderTarget,
    Utils,
)


class UI:
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
            Vector2f(canvasSize.x - 16, 0),
            Vector2f(0, canvasSize.y - 16),
            Vector2f(canvasSize.x - 16, canvasSize.y - 16),
        ]
        edgePositions = [
            Vector2f(16, 0),
            Vector2f(16, canvasSize.y - 16),
            Vector2f(0, 16),
            Vector2f(canvasSize.x - 16, 16),
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
