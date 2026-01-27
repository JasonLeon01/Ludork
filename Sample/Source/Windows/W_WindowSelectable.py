# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, Tuple, Dict, Any
from Engine import Image, IntRect, Vector2f, Vector2i, Input, View, FloatRect
from Engine.UI import Rect, ListView
from Engine.Utils import Math
from Engine.UI.Base import ControlBase, FunctionalBase
from .W_WindowBase import WindowBase


class WindowSelectable(WindowBase):
    def __init__(
        self,
        rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]],
        listView: Optional[ListView] = None,
        rectWidth: Optional[int] = None,
        rectHeight: int = 32,
        windowSkin: Optional[Image] = None,
        repeated: bool = False,
    ) -> None:
        super().__init__(rect, windowSkin, repeated)
        self.index: Optional[int] = 0
        if not listView is None:
            self.content.addChild(listView)
        self._listView = listView
        if rectWidth is None:
            rectWidth = self._getRectWidth()
        self._rectWidth = rectWidth
        self._rectHeight = rectHeight
        self._rect = Rect(
            IntRect(
                Math.ToVector2i(self._getRectPosition()),
                Vector2i(self._rectWidth, self._rectHeight),
            )
        )

    def getListView(self) -> Optional[ListView]:
        return self._listView

    def setListView(self, listView: Optional[ListView] = None) -> None:
        if not self._listView is None:
            self.content.removeChild(self._listView)
        if not listView is None:
            self.content.addChild(listView)
        self._listView = listView

    def update(self, deltaTime: float) -> None:
        self._rect.setVisible((not self.index is None and self._itemCount() > 0))
        if self.index is not None:
            self._updateScroll()
            if self._rectWidth != self._getRectWidth():
                self._rectWidth = self._getRectWidth()
                self._rect = Rect(
                    IntRect(
                        Math.ToVector2i(self._getRectPosition()),
                        Vector2i(self._rectWidth, self._rectHeight),
                    )
                )
        if self._rect.getParent() is None:
            self.content.addChild(self._rect)
        if self._isHovered:
            for index, child in enumerate(self._listView.getChildren()):
                if isinstance(child, FunctionalBase):
                    if child.isHovered():
                        self.index = index
            for index, child in enumerate(self._listView.getChildren()):
                if self.index == index:
                    if isinstance(child, FunctionalBase):
                        if self._judgeIfConfirm(child):
                            child.onConfirm({})
        super().update(deltaTime)

    def onMouseWheelScrolled(self, kwargs: Dict[str, Any]):
        if not (self._listView and len(self._listView.getChildren()) > 0 and not self.index is None):
            return
        delta = kwargs["delta"]
        if delta > 0:
            self.index = max(0, self.index - 1)
        elif delta < 0:
            self.index = min(self._itemCount() - 1, self.index + 1)
        targetChild = self._listView.getChildren()[self.index]
        if hasattr(targetChild, "getAbsoluteBounds"):
            bounds: FloatRect = targetChild.getAbsoluteBounds()
            Input.setMousePosition(Math.ToVector2i(bounds.position + bounds.size / 2))

    def onMouseMoved(self, kwargs: Dict[str, Any]):
        pass

    def onKeyDown(self, kwargs: Dict[str, Any]):
        if not (self._listView and len(self._listView.getChildren()) > 0 and not self.index is None):
            return

        if Input.isActionTriggered(Input.getConfirmKeys(), handled=True):
            children = self._listView.getChildren()
            if 0 <= self.index < len(children):
                child = children[self.index]
                if isinstance(child, FunctionalBase):
                    child.onConfirm({})
            return

        columns = self._getColumns()
        if Input.isActionTriggered(Input.getUpKeys(), handled=True):
            if columns == 1:
                self.index = (self.index - 1) % self._itemCount()
            else:
                self.index = max(0, self.index - columns)
        elif Input.isActionTriggered(Input.getDownKeys(), handled=True):
            if columns == 1:
                self.index = (self.index + 1) % self._itemCount()
            else:
                self.index = min(self._itemCount() - 1, self.index + columns)
        elif Input.isActionTriggered(Input.getLeftKeys(), handled=True):
            if columns != 1:
                self.index = max(0, self.index - 1)
        elif Input.isActionTriggered(Input.getRightKeys(), handled=True):
            if columns != 1:
                self.index = min(self._itemCount() - 1, self.index + 1)

    def _getRectPosition(self) -> Optional[Vector2f]:
        if self.index is None:
            return None
        columns = self._getColumns()
        x = (self.index % columns) * self._rectWidth + 16
        y = (self.index // columns) * self._rectHeight
        return Vector2f(x, y)

    def _getRectWidth(self) -> int:
        columns = self._getColumns()
        return int((self.content.getSize().x - 32) / columns)

    def _itemCount(self) -> int:
        if self._listView is None:
            return 0
        return len(self._listView.getChildren())

    def _getColumns(self) -> int:
        if self._listView is None:
            return 1
        return self._listView.getColumns()

    def _applyItem(self, item: ControlBase) -> None:
        if hasattr(item, "getLocalBounds"):
            bounds = item.getLocalBounds()
            origin = Vector2f(bounds.position.x + bounds.size.x / 2, 0)
            item.setOrigin(origin)

    def _updateScroll(self) -> None:
        self._rect.setPosition(self._getRectPosition())
        centre = self.content.getView().getCenter()
        viewSize = self.content.getView().getSize()
        origin = centre - viewSize / 2
        originX = origin.x
        originY = origin.y
        posX, posY = self._rect.v_getPosition()
        size = self._rect.getSize()
        if posX + size.x > originX + self.content.getSize().x:
            originX = posX + size.x - self.content.getSize().x
        if posY + size.y > originY + self.content.getSize().y:
            originY = posY + size.y - self.content.getSize().y
        if posX < originX:
            originX = posX
        if posY < originY:
            originY = posY
        self.content.setView(View(Vector2f(originX, originY) + viewSize / 2, viewSize))

    def _judgeIfConfirm(self, target: FunctionalBase):
        if target.isHovered() and Input.isMouseInputMode() and Input.isMouseButtonTriggered(Input.Mouse.Button.Left):
            return True
        return False
