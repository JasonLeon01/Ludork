# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Tuple, Union, Optional, TYPE_CHECKING
from . import (
    UI_SpriteBase,
    IntRect,
    Vector2i,
    Vector2f,
    RenderTexture,
    Color,
    Utils,
)

if TYPE_CHECKING:
    from Engine import Drawable, Vector2u
    from Engine.Gameplay.UI import RichText

SpriteBase = UI_SpriteBase.UI


class UI(SpriteBase):
    def __init__(self, rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]]) -> None:
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
        self._parent: Optional[UI] = None
        self._childrenList: List[Drawable] = []
        super().__init__(self._canvas.getTexture())
        self.setPosition(rect.position)

    def getSize(self) -> Vector2u:
        return self._size

    def getParent(self) -> Optional[UI]:
        return self._parent

    def setParent(self, parent: Optional[UI]) -> None:
        self._parent = parent

    def getChildren(self) -> List[Drawable]:
        return self._childrenList

    def addChild(self, child: Union[Drawable, RichText]) -> None:
        from Engine.Gameplay import Actor

        assert not isinstance(child, Actor), "Cannot add Actor to UI"
        self._childrenList.append(child)
        if isinstance(child, UI):
            child.setParent(self)

    def removeChild(self, child: Union[Drawable, RichText]) -> None:
        if child not in self._childrenList:
            raise ValueError("Child not found")
        self._childrenList.remove(child)

    def onTick(self, deltaTime: float) -> None:
        pass

    def onLateTick(self, deltaTime: float) -> None:
        pass

    def update(self, deltaTime: float) -> None:
        from Engine import System

        if not self._visible:
            return
        for child in self._childrenList:
            if hasattr(child, "update"):
                child.update(deltaTime)
        self.onTick(deltaTime)
        self._canvas.clear(Color.Transparent)
        for child in self._childrenList:
            if hasattr(child, "getVisible"):
                if not child.getVisible():
                    continue
            if isinstance(child, SpriteBase):
                self._canvas.draw(child, child.getRenderState())
            else:
                defaultState = Utils.Render.CanvasRenderState()
                defaultState.transform.scale(Vector2f(System.getScale(), System.getScale()))
                self._canvas.draw(child, defaultState)
        self._canvas.display()
        self.onLateTick(deltaTime)
