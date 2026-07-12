# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Dict, List, Optional, Union, Tuple
from Engine import Pair, Image, IntRect, Vector2f, Vector2i, Input, View, FloatRect
from Engine.UI import Rect, ListView
from Engine.UI.Rect import SELECTION_RECT_OPACITY_CURVE_KEY
from Engine.Utils import Math
from Engine.UI.Base import ControlBase, Direction, FunctionalBase
from Global import Manager
from .WindowBase import WindowBase
from ...System import System as GameSystem


_INACTIVE_SELECTION_RECT_OPACITY_MULTIPLIER = 0.35
_REPEAT_DELAY = 0.4
_REPEAT_INTERVAL = 0.1


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
        hitRectWidth: Optional[int] = None,
        hitRectHeight: Optional[int] = None,
    ) -> None:
        r"""\brief Construct a selectable window.

        - \param rect The window rectangle.
        - \param listView Optional ListView for selectable items.
        - \param rectWidth Optional fixed width for the selection rectangle.
        - \param rectHeight Height of each selection item.
        - \param windowSkin Optional window skin image.
        - \param repeated Whether the window skin is repeated.
        - \param hitRectWidth Override hit detection width; defaults to selection rect width.
        - \param hitRectHeight Override hit detection height; defaults to selection rect height.
        """
        super().__init__(rect, windowSkin, repeated)
        self._oldIndex: Optional[int] = None
        self.index: Optional[int] = 0
        self.setCanReceiveFocus(True)
        if not listView is None:
            self.content.addChild(listView)
        self._listView = listView
        if rectWidth is None:
            rectWidth = self._getRectWidth()
        self._rectWidth = rectWidth
        self._rectHeight = rectHeight
        self._hitRectWidth: Optional[int] = hitRectWidth
        self._hitRectHeight: Optional[int] = hitRectHeight
        self._rect = Rect(
            IntRect(
                Math.ToVector2i(self._getRectPosition()),
                Vector2i(self._rectWidth, self._rectHeight),
            ),
            self._windowSkin,
            SELECTION_RECT_OPACITY_CURVE_KEY,
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

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update selection rectangle position and handle hover.

        - \param deltaTime Elapsed time in seconds.
        """
        active = self.getActive()
        focused = self._hasCursorFocus()
        self._rect.setVisible((not self.index is None and self._itemCount() > 0 and focused))
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
                    SELECTION_RECT_OPACITY_CURVE_KEY,
                )
        self._rect.update(deltaTime)
        self._rect.setOpacityMultiplier(1.0 if focused else _INACTIVE_SELECTION_RECT_OPACITY_MULTIPLIER)
        if self._rect.getParent() is None:
            self.content.addChild(self._rect)
        if self.canReceiveFocus() and self._isHovered and self._listView and Input.isMouseInputMode() and Input.isMouseMoved():
            self.requestKeyboardFocus()
            mousePos = Math.ToVector2f(Input.getMousePosition())
            for hoverIndex, child in enumerate(self._listView.getChildren()):
                if isinstance(child, ControlBase):
                    if self._getItemHitAbsoluteBounds(hoverIndex).contains(mousePos):
                        self.index = hoverIndex
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
        super().onTick(deltaTime)

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

        if Input.isActionTriggered(Input.getConfirmKeys(), handled=False):
            children = self._listView.getChildren()
            if 0 <= self.index < len(children):
                child = children[self.index]
                if isinstance(child, FunctionalBase):
                    child.onConfirm({})
                    Input.isActionTriggered(Input.getConfirmKeys(), handled=True)
            return

        if self._handleDirectionalAction(Direction.UP, Input.getUpKeys()):
            return
        if self._handleDirectionalAction(Direction.DOWN, Input.getDownKeys()):
            return
        if self._handleDirectionalAction(Direction.LEFT, Input.getLeftKeys()):
            return
        self._handleDirectionalAction(Direction.RIGHT, Input.getRightKeys())

    def onDirectionalKey(self, direction: Direction) -> bool:
        r"""\brief Handle directional cursor movement.

        - \param direction Direction pressed by keyboard or gamepad.

        - \return True if the direction was handled inside this window.
        """
        if self.index is None or self._itemCount() <= 0:
            return False
        columns = self._getColumns()
        if direction == Direction.UP:
            if columns == 1:
                self.index = (self.index - 1) % self._itemCount()
                return True
            return self._setIndexIfChanged(max(0, self.index - columns))
        if direction == Direction.DOWN:
            if columns == 1:
                self.index = (self.index + 1) % self._itemCount()
                return True
            return self._setIndexIfChanged(min(self._itemCount() - 1, self.index + columns))
        if direction == Direction.LEFT:
            if columns == 1 or self.index % columns == 0:
                return False
            return self._setIndexIfChanged(self.index - 1)
        if direction == Direction.RIGHT:
            if columns == 1 or self.index % columns == columns - 1 or self.index + 1 >= self._itemCount():
                return False
            return self._setIndexIfChanged(self.index + 1)
        return False

    def _getRectPositionForIndex(self, index: int) -> Vector2f:
        r"""\brief Compute the selection rectangle position for a given item index.

        - \param index  Zero-based item index.
        - \return  Top-left position of the selection rectangle in content space.
        """
        columns = self._getColumns()
        x = (index % columns) * self._rectWidth + 16
        y = (index // columns) * self._rectHeight
        return Vector2f(x, y)

    def _getRectPosition(self) -> Optional[Vector2f]:
        if self.index is None:
            return None
        return self._getRectPositionForIndex(self.index)

    def _getItemHitSize(self) -> Vector2i:
        r"""\brief Get the hit detection size for each selectable item.

        - \return  Width and height used for mouse/touch hit testing.
        """
        w = self._hitRectWidth if self._hitRectWidth is not None else self._rectWidth
        h = self._hitRectHeight if self._hitRectHeight is not None else self._rectHeight
        return Vector2i(w, h)

    def _getItemHitAbsoluteBounds(self, index: int) -> FloatRect:
        r"""\brief Get the screen-space hit rectangle for a given item index.

        Temporarily repositions the selection rect sprite to the target cell,
        reads absolute screen bounds through the content canvas transform
        (including scroll and Scale), then restores the original position.

        - \param index  Zero-based item index.
        - \return  Absolute screen-space bounds used for hit testing.
        """
        savedPos = self._rect.v_getPosition()
        self._rect.setPosition(self._getRectPositionForIndex(index))
        rectAbs = self._rect.getAbsoluteBounds()
        self._rect.setPosition(savedPos)
        hitSize = self._getItemHitSize()
        if hitSize.x == self._rectWidth and hitSize.y == self._rectHeight:
            return rectAbs
        scaleX = rectAbs.size.x / float(self._rectWidth) if self._rectWidth != 0 else 1.0
        scaleY = rectAbs.size.y / float(self._rectHeight) if self._rectHeight != 0 else 1.0
        return FloatRect(rectAbs.position, Vector2f(hitSize.x * scaleX, hitSize.y * scaleY))

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

    def _hasCursorFocus(self) -> bool:
        if self.ownsKeyboardCursorFocus():
            return True
        return self.getActive() and self.shouldDispatchKeyboardInput()

    def _handleDirectionalAction(self, direction: Direction, actionKeys: List[Any]) -> bool:
        if not Input.isActionTriggered(
            actionKeys,
            handled=False,
            repeatDelay=_REPEAT_DELAY,
            repeatInterval=_REPEAT_INTERVAL,
        ):
            return False
        handled = self.onDirectionalKey(direction)
        if not handled:
            handled = self.requestDirectionalFocusMove(direction)
        if handled:
            Input.isActionTriggered(
                actionKeys,
                handled=True,
                repeatDelay=_REPEAT_DELAY,
                repeatInterval=_REPEAT_INTERVAL,
            )
        return handled

    def _setIndexIfChanged(self, index: int) -> bool:
        if self.index == index:
            return False
        self.index = index
        return True

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
        if not Input.isMouseButtonTriggered(Input.Mouse.Button.Left, handled=False):
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
            if self._getItemHitAbsoluteBounds(index).contains(position):
                self.index = index
                child.onConfirm({})
                return True
        return False
