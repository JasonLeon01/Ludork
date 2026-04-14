# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
from typing import List, Tuple, Union, TYPE_CHECKING
from .. import TypeAdapter, Pair, IntRect, Vector2i, Vector2f, Vector2u, RenderTexture, Color, View
from ..Utils import Math
from .Base import SpriteBase, FunctionalBase

if TYPE_CHECKING:
    from Engine.UI.Base import ControlBase


class Canvas(SpriteBase, FunctionalBase):
    @TypeAdapter(rect=([tuple, list], IntRect, lambda pos, size: IntRect(Vector2i(*pos), Vector2i(*size))))
    def __init__(self, rect: Union[IntRect, Tuple[Pair[int], Pair[int]], List[List[int]]]) -> None:
        from .. import Scale

        self._size = Math.ToVector2u(rect.size)
        size = Math.ToVector2u(Math.ToVector2f(rect.size) * Scale)
        self._canvas: RenderTexture = RenderTexture(size)
        self._childrenList: List[ControlBase] = []
        self._renderQueue: List[tuple[ControlBase, object]] = []
        self._zOrder: int = 0
        SpriteBase.__init__(self, self._canvas.getTexture())
        FunctionalBase.__init__(self)
        self.setPosition(Math.ToVector2f(rect.position))

    def v_getOrigin(self) -> Pair[float]:
        origin = self.getOrigin()
        return (origin.x, origin.y)

    def getOrigin(self) -> Vector2f:
        from .. import Scale

        origin = super().getOrigin()
        return origin / Scale

    @TypeAdapter(origin=([tuple, list], Vector2f))
    def setOrigin(self, origin: Union[Vector2f, Pair[float], List[float]]) -> None:
        from .. import Scale

        super().setOrigin(origin * Scale)

    def getSize(self) -> Vector2u:
        return self._size

    def getNoTranslationRect(self) -> IntRect:
        return IntRect(Vector2i(0, 0), Math.ToVector2i(self._size))

    def getContentRect(self) -> IntRect:
        return IntRect(Vector2i(16, 16), Vector2i(self._size.x - 32, self._size.y - 32))

    def getView(self) -> View:
        from .. import Scale

        view = self._canvas.getView()
        return View(view.getCenter() / Scale, view.getSize() / Scale)

    def getDefaultView(self) -> View:
        from .. import Scale

        view = self._canvas.getDefaultView()
        return View(view.getCenter() / Scale, view.getSize() / Scale)

    def setView(self, view: View) -> None:
        from .. import Scale

        newView = View(view.getCenter() * Scale, view.getSize() * Scale)
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
        child.setParent(None)

    def setZOrder(self, zOrder: int) -> None:
        self._zOrder = zOrder

    def getZOrder(self) -> int:
        return self._zOrder

    def _appendRenderNode(self, node: ControlBase, parentStates) -> None:
        from .UI_ListView import ListView

        if hasattr(node, "applyPositions"):
            node.applyPositions()
        nodeStates = copy.copy(parentStates)
        if hasattr(node, "getRenderStates"):
            nodeStates = copy.copy(node.getRenderStates())
            nodeStates.transform *= parentStates.transform
        if not isinstance(node, ListView):
            self._renderQueue.append((node, nodeStates))
        childStates = copy.copy(parentStates)
        childStates.transform *= node._getRenderTransform()
        if hasattr(node, "getChildren"):
            for child in node.getChildren():
                if child.getVisible():
                    self._appendRenderNode(child, childStates)

    def _buildRenderQueue(self) -> None:
        from ..Utils import Render

        self._renderQueue.clear()
        baseStates = Render.CanvasRenderStates()
        for child in self._childrenList:
            if child.getVisible():
                self._appendRenderNode(child, baseStates)

    def update(self, deltaTime: float) -> None:
        for child in self._childrenList:
            if child.getActive() and child.getVisible():
                if hasattr(child, "update"):
                    child.update(deltaTime)
        FunctionalBase.update(self, deltaTime)
        self._buildRenderQueue()

    def render(self) -> None:
        self._canvas.clear(Color.Transparent)
        for node, nodeStates in self._renderQueue:
            self._canvas.draw(node, copy.copy(nodeStates))
        self._canvas.display()

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
