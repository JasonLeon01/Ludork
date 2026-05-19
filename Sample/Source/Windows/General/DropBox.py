# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Callable, List, Optional
from Engine import Image, IntRect, Vector2i, Vector2f, UI, Input
from Engine.UI import Canvas, PlainText, ListView, Rect
from Engine.UI.FunctionalUI import FPlainText
from Engine.UI.Base import FunctionalBase
from ..Base import WindowSelectable


_ROW_HEIGHT: int = 32
_FONT_SIZE: int = 20
_TEXT_PAD_X: int = 8
_WINDOW_BORDER_PAD_Y: int = 32


def _expandedOuterHeight(itemCount: int) -> int:
    return _WINDOW_BORDER_PAD_Y + _ROW_HEIGHT * max(itemCount, 1)


class _DropBoxField(Canvas):
    r"""\brief Collapsed drop-down field showing the current selection in a windowskin frame."""

    def __init__(
        self,
        items: List[str],
        selectedIndex: int,
        width: int,
        windowSkin: Image,
    ) -> None:
        r"""\brief Construct a collapsed drop-box field.

        - \param items          Option labels
        - \param selectedIndex  Initially selected index
        - \param width          Field width in logical UI units
        - \param windowSkin     Windowskin image
        """
        Canvas.__init__(self, ((0, 0), (width, _ROW_HEIGHT)))
        self._frame = Rect(IntRect(Vector2i(0, 0), Vector2i(width, _ROW_HEIGHT)), windowSkin)
        self.addChild(self._frame)
        self._valueText = PlainText(UI.DefaultFont, self._labelAt(items, selectedIndex), _FONT_SIZE)
        self._valueText.setPosition(Vector2f(_TEXT_PAD_X, self._centeredTextY(self._valueText)))
        self.addChild(self._valueText)
        self._buildRenderQueue()
        self.render()

    def getChildren(self) -> list:
        r"""\brief Render this field as a single canvas texture.

        - \return  Empty list
        """
        return []

    def setValueText(self, text: str) -> None:
        r"""\brief Update the displayed selection label.

        - \param text  New label text
        """
        self._valueText.setString(text)
        self._valueText.setPosition(Vector2f(_TEXT_PAD_X, self._centeredTextY(self._valueText)))

    def getLogicalSize(self) -> Vector2f:
        r"""\brief Get the field size in logical UI units.

        - \return  Width and height of the collapsed field
        """
        size = self.getSize()
        return Vector2f(float(size.x), float(size.y))

    def update(self, deltaTime: float) -> None:
        r"""\brief Refresh and render the collapsed field canvas without handling input.

        - \param deltaTime  Elapsed time in seconds
        """
        for child in self._childrenList:
            if child.getActive() and child.getVisible() and hasattr(child, "update"):
                child.update(deltaTime)
        self._buildRenderQueue()
        self.render()

    @staticmethod
    def _labelAt(items: List[str], index: int) -> str:
        if not items:
            return ""
        safeIndex = max(0, min(index, len(items) - 1))
        return items[safeIndex]

    @staticmethod
    def _centeredTextY(text: PlainText) -> float:
        bounds = text.getLocalBounds()
        return (float(_ROW_HEIGHT) - bounds.size.y) / 2.0


class DropBoxExpanded(WindowSelectable):
    r"""\brief Expanded drop-down list showing all options with cursor selection."""

    def __init__(
        self,
        items: List[str],
        selectedIndex: int,
        width: int,
        windowSkin: Image,
        onCollapse: Optional[Callable[[Optional[int]], None]] = None,
    ) -> None:
        r"""\brief Construct an expanded drop-box window.

        - \param items          Option labels
        - \param selectedIndex  Index to highlight on open
        - \param width          Window width in logical UI units
        - \param windowSkin     Windowskin image
        - \param onCollapse     Callback with chosen index, or None when cancelled
        """
        outerHeight = _expandedOuterHeight(len(items))
        super().__init__(((0, 0), (width, outerHeight)), None, width - 32, _ROW_HEIGHT, windowSkin)
        self._items: List[str] = list(items)
        self._selectedIndex = max(0, min(selectedIndex, len(self._items) - 1)) if self._items else 0
        self._onCollapse = onCollapse
        self.index = self._selectedIndex
        listView = ListView(
            self.content.getNoTranslationRect(),
            _ROW_HEIGHT,
            True,
            1,
        )
        for index, label in enumerate(self._items):
            child = FPlainText(UI.DefaultFont, label, _FONT_SIZE)
            child.addConfirmCallback(self._makeItemConfirmCallback(index))
            self._applyItem(child)
            listView.addChild(child)
        self.setListView(listView)

    def getSelectedIndex(self) -> int:
        r"""\brief Get the currently highlighted or chosen index.

        - \return  Selected index
        """
        return self._selectedIndex

    def setSelectedIndex(self, index: int) -> None:
        r"""\brief Set the selected index and move the list cursor.

        - \param index  New selected index
        """
        if not self._items:
            self._selectedIndex = 0
            self.index = 0
            return
        self._selectedIndex = max(0, min(index, len(self._items) - 1))
        self.index = self._selectedIndex

    def refreshForOpen(self, selectedIndex: int) -> None:
        r"""\brief Sync list cursor when the drop-down is expanded.

        - \param selectedIndex  Index to select in the option list
        """
        self.setSelectedIndex(selectedIndex)

    def onKeyDown(self, kwargs: dict) -> None:
        r"""\brief Collapse on cancel without applying a new selection.

        - \param kwargs  Event arguments
        """
        if not self.getActive():
            return
        if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
            if self._onCollapse is not None:
                self._onCollapse(None)
            return
        super().onKeyDown(kwargs)

    def onTick(self, deltaTime: float) -> None:
        if not self.getActive() or not self.getVisible():
            return
        if Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True):
            if self._onCollapse is not None:
                self._onCollapse(None)

    def _makeItemConfirmCallback(self, index: int) -> Callable:
        def _onItemConfirm(obj: FunctionalBase, kwargs: dict) -> None:
            self._selectedIndex = index
            if self._onCollapse is not None:
                self._onCollapse(index)

        return _onItemConfirm


class DropBox:
    r"""\brief Drop-down coordinator toggling between collapsed field and expanded list."""

    def __init__(
        self,
        items: List[str],
        selectedIndex: int = 0,
        width: int = 200,
        windowSkin: Optional[Image] = None,
        onLayoutChanged: Optional[Callable[[], None]] = None,
        onExpandedChanged: Optional[Callable[[bool], None]] = None,
    ) -> None:
        r"""\brief Construct a drop-box coordinator.

        - \param items              Option labels
        - \param selectedIndex      Initial selection index
        - \param width              Field width in logical UI units
        - \param windowSkin         Windowskin image
        - \param onLayoutChanged    Callback when expanded height changes
        - \param onExpandedChanged  Callback when expanded state changes
        """
        from Global import Manager

        self._width = width
        if windowSkin is None:
            windowSkin = Manager.loadSystem(UI.DefaultWindowskinName, smooth=True).copyToImage()
        self._windowSkin = windowSkin
        self._onLayoutChanged = onLayoutChanged
        self._onExpandedChanged = onExpandedChanged
        self._onSelectedIndexChanged: Optional[Callable[[int], None]] = None
        self._items: List[str] = list(items)
        self._selectedIndex = max(0, min(selectedIndex, len(self._items) - 1)) if self._items else 0
        self._expanded = False
        self._field = _DropBoxField(
            self._items,
            self._selectedIndex,
            width,
            windowSkin,
        )
        self._expandedView = DropBoxExpanded(
            self._items,
            self._selectedIndex,
            width,
            windowSkin,
            self._onExpandedCollapse,
        )
        self._expandedView.setVisible(False)
        self._expandedView.setActive(False)

    def setOnExpandedChanged(self, onExpandedChanged: Optional[Callable[[bool], None]]) -> None:
        r"""\brief Register a callback for expanded-state changes.

        - \param onExpandedChanged  Callback invoked with the new expanded flag
        """
        self._onExpandedChanged = onExpandedChanged

    def setOnSelectedIndexChanged(self, onSelectedIndexChanged: Optional[Callable[[int], None]]) -> None:
        r"""\brief Register a callback for selected-index changes.

        - \param onSelectedIndexChanged  Callback invoked with the new selected index
        """
        self._onSelectedIndexChanged = onSelectedIndexChanged

    def getItems(self) -> List[str]:
        r"""\brief Get option labels.

        - \return  Copy of option labels
        """
        return list(self._items)

    def setItems(self, items: List[str]) -> None:
        r"""\brief Replace option labels and rebuild child views.

        - \param items  New option labels
        """
        self._items = list(items)
        if self._items:
            self._selectedIndex = max(0, min(self._selectedIndex, len(self._items) - 1))
        else:
            self._selectedIndex = 0
        self._rebuildViews()

    def getSelectedIndex(self) -> int:
        r"""\brief Get the selected index.

        - \return  Current selection index
        """
        return self._selectedIndex

    def setSelectedIndex(self, index: int) -> None:
        r"""\brief Set the selected index and refresh visible widgets.

        - \param index  New selection index
        """
        if not self._items:
            self._selectedIndex = 0
            self._field.setValueText("")
            return
        newIndex = max(0, min(index, len(self._items) - 1))
        changed = self._selectedIndex != newIndex
        self._selectedIndex = newIndex
        label = self._items[self._selectedIndex]
        self._field.setValueText(label)
        self._expandedView.setSelectedIndex(self._selectedIndex)
        if changed and self._onSelectedIndexChanged is not None:
            self._onSelectedIndexChanged(self._selectedIndex)

    def getSelectedItem(self) -> str:
        r"""\brief Get the label of the selected item.

        - \return  Selected label or empty string
        """
        if not self._items:
            return ""
        return self._items[self._selectedIndex]

    def isExpanded(self) -> bool:
        r"""\brief Check whether the list is expanded.

        - \return  True when expanded
        """
        return self._expanded

    def setExpanded(self, expanded: bool) -> None:
        r"""\brief Show the field or expanded list.

        - \param expanded  True to expand, False to collapse
        """
        if self._expanded == expanded:
            return
        self._expanded = expanded
        if expanded:
            self._expandedView.refreshForOpen(self._selectedIndex)
        self._applyVisibility()
        self._notifyLayoutChanged()
        self._notifyExpandedChanged()

    def getSize(self) -> Vector2f:
        r"""\brief Get the current logical size of the drop-box.

        - \return  Size in logical UI units
        """
        if self._expanded:
            return Vector2f(float(self._width), float(_expandedOuterHeight(len(self._items))))
        return self._field.getLogicalSize()

    def getField(self) -> _DropBoxField:
        r"""\brief Get the collapsed field window.

        - \return  Collapsed field widget
        """
        return self._field

    def getExpandedView(self) -> DropBoxExpanded:
        r"""\brief Get the expanded list window.

        - \return  Expanded list widget
        """
        return self._expandedView

    def getWidgets(self) -> List:
        r"""\brief Get both field and expanded widgets for parenting into a row canvas.

        - \return  Field and expanded widgets
        """
        return [self._field, self._expandedView]

    def _onExpandedCollapse(self, selectedIndex: Optional[int]) -> None:
        if selectedIndex is not None:
            self.setSelectedIndex(selectedIndex)
        self.setExpanded(False)

    def _applyVisibility(self) -> None:
        self._field.setVisible(not self._expanded)
        self._field.setActive(not self._expanded)
        self._expandedView.setVisible(self._expanded)
        self._expandedView.setActive(self._expanded)

    def _notifyLayoutChanged(self) -> None:
        if self._onLayoutChanged is not None:
            self._onLayoutChanged()

    def _notifyExpandedChanged(self) -> None:
        if self._onExpandedChanged is not None:
            self._onExpandedChanged(self._expanded)

    def _rebuildViews(self) -> None:
        wasExpanded = self._expanded
        parentField = self._field.getParent()
        parentExpanded = self._expandedView.getParent()
        fieldPos = self._field.getPosition()
        expandedPos = self._expandedView.getPosition()
        if parentField is not None:
            parentField.removeChild(self._field)
        if parentExpanded is not None:
            parentExpanded.removeChild(self._expandedView)
        self._field = _DropBoxField(
            self._items,
            self._selectedIndex,
            self._width,
            self._windowSkin,
        )
        self._expandedView = DropBoxExpanded(
            self._items,
            self._selectedIndex,
            self._width,
            self._windowSkin,
            self._onExpandedCollapse,
        )
        self._field.setPosition(fieldPos)
        self._expandedView.setPosition(expandedPos)
        if parentField is not None:
            parentField.addChild(self._field)
            parentField.addChild(self._expandedView)
        self._applyVisibility()
        if wasExpanded:
            self._expanded = False
            self.setExpanded(True)
        self._notifyLayoutChanged()
