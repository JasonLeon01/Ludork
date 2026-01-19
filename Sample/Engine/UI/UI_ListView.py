# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Tuple, Union
from .. import IntRect, RenderTarget, RenderStates
from .Base import ControlBase


class ListView(ControlBase):
    def __init__(
        self, rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]], defaultItemHeight: int = 32
    ) -> None:
        super().__init__()
        self._defaultItemHeight: int = defaultItemHeight
        self._children: List[ControlBase] = []

    def addChild(self, child: ControlBase) -> None:
        self._children.append(child)

    def removeChild(self, child: ControlBase) -> None:
        self._children.remove(child)

    def clearChildren(self) -> None:
        self._children.clear()

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        states.transform *= self.getTransform()
        for child in self._children:
            child.draw(target, states)

    def applyPosition(self) -> None:
        originY = 0
        for child in self._children:
            child.setPosition((child.getPosition().x, originY))
            if hasattr(child, "getSize"):
                originY += child.getSize().y
            else:
                originY += self._defaultItemHeight
