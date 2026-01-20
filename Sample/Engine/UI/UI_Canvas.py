# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Tuple, Union, TYPE_CHECKING
from .. import (
    IntRect,
    Vector2i,
    Vector2f,
    RenderTexture,
    RenderStates,
    Color,
    View,
)
from ..Utils import Math, Render
from .Base import SpriteBase, FunctionalBase

if TYPE_CHECKING:
    from Engine import Vector2u
    from Engine.UI.Base import ControlBase


class Canvas(SpriteBase, FunctionalBase):
    def __init__(self, rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]]) -> None:
        assert isinstance(rect, (IntRect, tuple)), "rect must be a tuple or IntRect"
        if not isinstance(rect, IntRect):
            position, size = rect
            x, y = position
            w, h = size
            position = Vector2i(x, y)
            size = Vector2i(w, h)
            rect = IntRect(position, size)
        self._size = Math.ToVector2u(rect.size)
        size = Math.ToVector2u(Render.getRealSize(rect.size))
        self._canvas: RenderTexture = RenderTexture(size)
        self._childrenList: List[ControlBase] = []
        SpriteBase.__init__(self, self._canvas.getTexture())
        FunctionalBase.__init__(self)
        self.setPosition(Math.ToVector2f(rect.position))

    def v_getOrigin(self) -> Tuple[float, float]:
        origin = self.getOrigin()
        return (origin.x, origin.y)

    def getOrigin(self) -> Vector2f:
        from Engine import System

        origin = super().getOrigin()
        return origin / System.getScale()

    def setOrigin(self, origin: Union[Vector2f, Tuple[float, float]]) -> None:
        from Engine import System

        assert isinstance(origin, (Vector2f, tuple)), "origin must be a tuple or Vector2f"
        if isinstance(origin, tuple):
            origin = Vector2f(*origin)
        super().setOrigin(origin * System.getScale())

    def getSize(self) -> Vector2u:
        return self._size

    def getNoTranslationRect(self) -> IntRect:
        return IntRect(Vector2i(0, 0), Math.ToVector2i(self._size))

    def getContentRect(self) -> IntRect:
        return IntRect(Vector2i(16, 16), Vector2i(self._size.x - 32, self._size.y - 32))

    def getView(self) -> View:
        from Engine import System

        view = self._canvas.getView()
        return View(view.getCenter() / System.getScale(), view.getSize() / System.getScale())

    def getDefaultView(self) -> View:
        from Engine import System

        view = self._canvas.getDefaultView()
        return View(view.getCenter() / System.getScale(), view.getSize() / System.getScale())

    def setView(self, view: View) -> None:
        from Engine import System

        newView = View(view.getCenter() * System.getScale(), view.getSize() * System.getScale())
        self._canvas.setView(newView)

    def getChildren(self) -> List[ControlBase]:
        return self._childrenList

    def addChild(self, child: ControlBase) -> None:
        from ..Gameplay.Actors import Actor

        assert not isinstance(child, Actor), "Cannot add Actor to UI"
        self._childrenList.append(child)
        child.setParent(self)

    def removeChild(self, child: ControlBase) -> None:
        if child not in self._childrenList:
            raise ValueError("Child not found")
        self._childrenList.remove(child)

    def update(self, deltaTime: float) -> None:
        for child in self._childrenList:
            if child.getActive() and child.getVisible():
                if hasattr(child, "update"):
                    child.update(deltaTime)
        self._canvas.clear(Color.Transparent)
        for child in self._childrenList:
            if not child.getVisible():
                continue
            params = [child]
            if hasattr(child, "getRenderStates"):
                params.append(child.getRenderStates())
            self._canvas.draw(*params)
        self._canvas.display()
        FunctionalBase.update(self, deltaTime)

    def lateUpdate(self, deltaTime: float) -> None:
        for child in self._childrenList:
            if child.getActive() and child.getVisible() and hasattr(child, "lateUpdate"):
                child.lateUpdate(deltaTime)
        FunctionalBase.lateUpdate(self, deltaTime)

    def fixedUpdate(self, fixedDelta: float) -> None:
        for child in self._childrenList:
            if child.getActive() and child.getVisible() and hasattr(child, "fixedUpdate"):
                child.fixedUpdate(fixedDelta)
        FunctionalBase.fixedUpdate(self, fixedDelta)
