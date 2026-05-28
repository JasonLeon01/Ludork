# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, Tuple, Dict, Any
from Engine import Pair, Image, IntRect, Vector2f, Vector2i, Input, View, FloatRect
from Engine.UI import Rect, ListView
from Engine.Utils import Math
from Engine.UI.Base import ControlBase, FunctionalBase
from Global import Manager, System
from .WindowBase import WindowBase
from ...System import System as GameSystem


_INACTIVE_SELECTION_RECT_OPACITY_MULTIPLIER = 0.35


class WindowSelectable(WindowBase):
    r"""\brief Window with cursor-navigable selectable items.

    Supports keyboard and mouse navigation, scrolling, and item selection.
    """

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        listView: Optional[ListView] = None,
        rectWidth: Optional[int] = None,
        rectHeight: int = 32,
        windowSkin: Optional[Image] = None,
        repeated: bool = False,
    ) -> None:
        r"""\brief Construct a selectable window.

        - \param rect The window rectangle.
        - \param listView Optional ListView for selectable items.
        - \param rectWidth Optional fixed width for the selection rectangle.
        - \param rectHeight Height of each selection item.
        - \param windowSkin Optional window skin image.
        - \param repeated Whether the window skin is repeated.
        """
        super().__init__(rect, windowSkin, repeated)
        self._oldIndex: Optional[int] = None
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
            ),
            self._windowSkin,
        )

    def getListView(self) -> Optional[ListView]:
        r"""\brief Get the current list view.

        - \return The ListView, or None.
        """
        return self._listView

    def setListView(self, listView: Optional[ListView] = None) -> None:
        r"""\brief Set the list view for selectable items.

        - \param listView The ListView to use, or None to clear.
        """
        if not self._listView is None:
            self.content.removeChild(self._listView)
        if not listView is None:
            self.content.addChild(listView)
        self._listView = listView

    def update(self, deltaTime: float) -> None:
        r"""\brief Update selection rectangle position and handle hover.

        - \param deltaTime Elapsed time in seconds.
        """
        active = self.getActive()
        self._rect.setVisible((not self.index is None and self._itemCount() > 0))
        if self.index is not None:
            self._updateScroll()
            if self._rectWidth != self._getRectWidth():
                self._rectWidth = self._getRectWidth()
                self._rect = Rect(
                    IntRect(
                        Math.ToVector2i(self._getRectPosition()),
                        Vector2i(self._rectWidth, self._rectHeight),
                    ),
                    self._windowSkin,
                )
        self._rect.setOpacityMultiplier(1.0 if active else _INACTIVE_SELECTION_RECT_OPACITY_MULTIPLIER)
        if self._rect.getParent() is None:
            self.content.addChild(self._rect)
        if active and self._isHovered and self._listView:
            for index, child in enumerate(self._listView.getChildren()):
                if isinstance(child, FunctionalBase):
                    if child.isHovered():
                        self.index = index
        if active and self._listView:
            self._confirmMouseSelection()
        if active and self._listView and Input.isTouchBegan():
            beganPos = Input.getTouchBeganPosition()
            if beganPos is not None:
                touchLocal = Math.ToVector2f(beganPos)
                if self._confirmSelectionAt(touchLocal):
                    Input.isTouchBegan(handled=True)
        if self._oldIndex is None:
            self._oldIndex = self.index
        if self.index != self._oldIndex:
            self._oldIndex = self.index
            Manager.playSE(GameSystem.getCursorSE())
        super().update(deltaTime)

    def onMouseWheelScrolled(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Handle mouse wheel scrolling.

        - \param kwargs Event data containing delta.
        """
        if not (self._listView and len(self._listView.getChildren()) > 0 and not self.index is None):
            return
        delta = kwargs["delta"]
        if delta > 0:
            self.index = max(0, self.index - 1)
        elif delta < 0:
            self.index = min(self._itemCount() - 1, self.index + 1)
        self._updateScroll()
        targetChild = self._listView.getChildren()[self.index]
        if isinstance(targetChild, ControlBase):
            bounds: FloatRect = targetChild.getAbsoluteBounds()
            Input.setMousePosition(Math.ToVector2i(bounds.position + bounds.size / 2), System.getWindow())

    def onMouseMoved(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Handle mouse movement events.

        - \param kwargs Event data.
        """
        pass

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Handle keyboard navigation and confirmation.

        Direction keys use repeat mode: immediate first press, then
        after ~0.4 s they fire every ~0.1 s while held.

        - \param kwargs Event data.
        """
        if not (self._listView and len(self._listView.getChildren()) > 0 and not self.index is None):
            return

        if Input.isActionTriggered(Input.getConfirmKeys(), handled=True):
            children = self._listView.getChildren()
            if 0 <= self.index < len(children):
                child = children[self.index]
                if isinstance(child, FunctionalBase):
                    child.onConfirm({})
            return

        _REPEAT_DELAY = 0.4
        _REPEAT_INTERVAL = 0.1

        columns = self._getColumns()
        if Input.isActionTriggered(Input.getUpKeys(), handled=True, repeatDelay=_REPEAT_DELAY, repeatInterval=_REPEAT_INTERVAL):
            if columns == 1:
                self.index = (self.index - 1) % self._itemCount()
            else:
                self.index = max(0, self.index - columns)
        elif Input.isActionTriggered(Input.getDownKeys(), handled=True, repeatDelay=_REPEAT_DELAY, repeatInterval=_REPEAT_INTERVAL):
            if columns == 1:
                self.index = (self.index + 1) % self._itemCount()
            else:
                self.index = min(self._itemCount() - 1, self.index + columns)
        elif Input.isActionTriggered(Input.getLeftKeys(), handled=True, repeatDelay=_REPEAT_DELAY, repeatInterval=_REPEAT_INTERVAL):
            if columns != 1:
                self.index = max(0, self.index - 1)
        elif Input.isActionTriggered(Input.getRightKeys(), handled=True, repeatDelay=_REPEAT_DELAY, repeatInterval=_REPEAT_INTERVAL):
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
        if isinstance(item, ControlBase):
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

    def _confirmMouseSelection(self) -> bool:
        if not Input.isMouseInputMode():
            return False
        if not Input.isMouseButtonTriggered(Input.Mouse.Button.Left):
            return False
        if self._confirmSelectionAt(Math.ToVector2f(Input.getMousePosition())):
            Input.isMouseButtonTriggered(Input.Mouse.Button.Left, handled=True)
            return True
        return False

    def _confirmSelectionAt(self, position: Vector2f) -> bool:
        if self._listView is None:
            return False
        if not self.getAbsoluteBounds().contains(position):
            return False
        for index, child in enumerate(self._listView.getChildren()):
            if not isinstance(child, FunctionalBase):
                continue
            if not isinstance(child, ControlBase):
                continue
            childBounds: FloatRect = child.getAbsoluteBounds()
            if childBounds.contains(position):
                self.index = index
                child.onConfirm({})
                return True
        return False
