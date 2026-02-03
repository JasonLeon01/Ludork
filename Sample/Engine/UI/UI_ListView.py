# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Tuple, Union
from .. import Pair, IntRect, RenderTarget, RenderStates
from ..Utils import Math, Render
from .Base import ControlBase


class ListView(ControlBase):
    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        defaultItemHeight: int = 32,
        fixItemHeight: bool = False,
        columns: int = 1,
    ) -> None:
        super().__init__()
        self.size = rect.size
        self.setPosition(Math.ToVector2f(rect.position))
        self._defaultItemHeight: int = defaultItemHeight
        self._fixItemHeight: bool = fixItemHeight
        self._columns: int = columns
        self._children: List[ControlBase] = []
        self._renderStates: RenderStates = Render.CanvasRenderStates()
        self._positionsSettled: bool = False

    def getColumns(self) -> int:
        return self._columns

    def setColumns(self, columns: int) -> None:
        self._columns = columns
        self._positionsSettled = False

    def getChildren(self) -> List[ControlBase]:
        return self._children

    def addChild(self, child: ControlBase) -> None:
        self._children.append(child)
        child.setParent(self)
        self._positionsSettled = False

    def removeChild(self, child: ControlBase) -> None:
        self._children.remove(child)
        child.setParent(None)
        self._positionsSettled = False

    def clearChildren(self) -> None:
        for child in self._children:
            child.setParent(None)
        self._children.clear()
        self._positionsSettled = False

    def getRenderStates(self) -> RenderStates:
        return self._renderStates

    def update(self, deltaTime: float) -> None:
        for child in self._children:
            if not (child._visible and child._active):
                continue
            if hasattr(child, "update"):
                child.update(deltaTime)

    def lateUpdate(self, deltaTime: float) -> None:
        for child in self._children:
            if hasattr(child, "lateUpdate"):
                child.lateUpdate(deltaTime)

    def fixedUpdate(self, fixedDelta: float) -> None:
        for child in self._children:
            if hasattr(child, "fixedUpdate"):
                child.fixedUpdate(fixedDelta)

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        self.applyPositions()
        states.transform *= self.getTransform()
        for child in self._children:
            target.draw(child, states)

    def applyPositions(self) -> None:
        if self._positionsSettled:
            return
        self._positionsSettled = True
        colWidth = (self.size.x - 32) / self._columns
        rowHeight = 0
        currentY = 0
        currentRowHeights = []

        for index, child in enumerate(self._children):
            col = index % self._columns
            row = index // self._columns
            if col == 0 and index > 0:
                currentY += rowHeight if rowHeight > 0 else self._defaultItemHeight
                rowHeight = 0
                currentRowHeights = []
            itemHeight = 0
            itemWidth = 0
            if hasattr(child, "getSize"):
                size = child.getSize()
                itemHeight = size.y
                itemWidth = size.x
            itemHeight = max(itemHeight, self._defaultItemHeight)
            if self._fixItemHeight:
                itemHeight = self._defaultItemHeight
            colCenter = 16 + col * colWidth + colWidth / 2
            originX = child.getOrigin().x
            scaleX = child.getScale().x
            posX = colCenter - itemWidth / 2 + originX * scaleX
            currentRowHeights.append(itemHeight)
            rowHeight = max(rowHeight, itemHeight)
            child.setPosition((posX, currentY))
