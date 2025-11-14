# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Tuple, Union, Optional, TYPE_CHECKING
from . import (
    UI_SpriteBase,
    IntRect,
    Vector2i,
    RenderTexture,
    Color,
    Utils,
)

if TYPE_CHECKING:
    from Engine import Vector2u
    from Engine.UI import UI_ControlBase

    ControlBase = UI_ControlBase.ControlBase

SpriteBase = UI_SpriteBase.SpriteBase


class Canvas(SpriteBase):
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
        self._parent: Optional[Canvas] = None
        self._childrenList: List[ControlBase] = []
        super().__init__(self._canvas.getTexture())
        self.setPosition(rect.position)

    def getSize(self) -> Vector2u:
        return self._size

    def getParent(self) -> Optional[Canvas]:
        return self._parent

    def setParent(self, parent: Optional[Canvas]) -> None:
        self._parent = parent

    def getChildren(self) -> List[ControlBase]:
        return self._childrenList

    def addChild(self, child: ControlBase) -> None:
        from Engine.Gameplay.Actors import Actor

        assert not isinstance(child, Actor), "Cannot add Actor to UI"
        self._childrenList.append(child)
        if isinstance(child, Canvas):
            child.setParent(self)

    def removeChild(self, child: ControlBase) -> None:
        if child not in self._childrenList:
            raise ValueError("Child not found")
        self._childrenList.remove(child)

    def onTick(self, deltaTime: float) -> None:
        pass

    def onLateTick(self, deltaTime: float) -> None:
        pass

    def update(self, deltaTime: float) -> None:
        if not self._visible:
            return
        for child in self._childrenList:
            if hasattr(child, "update"):
                child.update(deltaTime)
        self.onTick(deltaTime)
        self._canvas.clear(Color.Transparent)
        for child in self._childrenList:
            if not child.getVisible():
                continue
            if hasattr(child, "getRenderStates"):
                self._canvas.draw(child, child.getRenderStates())
            else:
                self._canvas.draw(child)
        self._canvas.display()
        self.onLateTick(deltaTime)
