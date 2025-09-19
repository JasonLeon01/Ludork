# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Tuple, Union, Optional, TYPE_CHECKING
from . import (
    UI_Base,
    Drawable,
    IntRect,
    Vector2i,
    Vector2u,
    Vector2f,
    RenderTexture,
    View,
    FloatRect,
    Angle,
    degrees,
    Color,
    Utils,
)

if TYPE_CHECKING:
    from Engine.Gameplay.UI import RichText


class UI(UI_Base.UI):
    def __init__(self, rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]]) -> None:
        if not isinstance(rect, IntRect):
            if not isinstance(rect, tuple) or len(rect) != 2:
                raise TypeError("rect must be a tuple or IntRect")
            position, size = rect
            x, y = position
            w, h = size
            position = Vector2i(x, y)
            size = Vector2i(w, h)
            rect = IntRect(position, size)
        self._size = Utils.Math.ToVector2u(rect.size)
        size = Utils.Math.ToVector2u(Utils.Render.getRealSize(rect.size))
        self._canvas: RenderTexture = RenderTexture(size)
        self._internalView = View(Utils.Math.ToFloatRect(0, 0, self._size.x, self._size.y))
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
        if not self._visible:
            return
        for child in self._childrenList:
            if hasattr(child, "update"):
                child.update(deltaTime)
        self.onTick(deltaTime)
        self._canvas.clear(Color.Transparent)
        self._canvas.setView(self._internalView)
        for child in self._childrenList:
            if hasattr(child, "getVisible"):
                if not child.getVisible():
                    continue
            if isinstance(child, RichText):
                child.draw(self._canvas, Utils.Render.CanvasRenderState())
            else:
                self._canvas.draw(child, Utils.Render.CanvasRenderState())
        self._canvas.setView(self._canvas.getDefaultView())
        self._canvas.display()
        self.onLateTick(deltaTime)
