# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Tuple, Union
from .. import Pair, IntRect, RenderTarget, RenderStates, Transform
from ..Utils import Math, Render
from .Base import ControlBase, FunctionalBase


class ListView(ControlBase, FunctionalBase):
    r"""List view control that arranges child controls in a grid layout.

    Supports multiple columns and automatic item positioning.
    """

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        defaultItemHeight: int = 32,
        fixItemHeight: bool = False,
        columns: int = 1,
    ) -> None:
        r"""\brief Construct a ListView with a given rectangle and layout options.

        - \param rect              Position and size of the list view
        - \param defaultItemHeight  Default height for items (used when item has no size)
        - \param fixItemHeight     Whether to force all items to the same height
        - \param columns           Number of columns in the grid
        """
        super().__init__()
        FunctionalBase.__init__(self)
        self.size = rect.size
        self.setPosition(Math.ToVector2f(rect.position))
        self._defaultItemHeight: int = defaultItemHeight
        self._fixItemHeight: bool = fixItemHeight
        self._columns: int = columns
        self._children: List[ControlBase] = []
        self._renderStates: RenderStates = Render.CanvasRenderStates()
        self._positionsSettled: bool = False

    def getColumns(self) -> int:
        r"""\brief Get the number of columns in the list view.

        - \return  Current column count
        """
        return self._columns

    def setColumns(self, columns: int) -> None:
        r"""\brief Set the number of columns in the list view.

        - \param columns  New column count
        """
        self._columns = columns
        self._positionsSettled = False

    def getChildren(self) -> List[ControlBase]:
        r"""\brief Get the list of child controls.

        - \return  List of child controls
        """
        return self._children

    def addChild(self, child: ControlBase) -> None:
        r"""\brief Add a child control to the list view.

        - \param child  Control to add
        """
        self._children.append(child)
        child.setParent(self)
        self._positionsSettled = False

    def removeChild(self, child: ControlBase) -> None:
        r"""\brief Remove a child control from the list view.

        - \param child  Control to remove
        """
        self._children.remove(child)
        child.setParent(None)
        self._positionsSettled = False

    def clearChildren(self) -> None:
        r"""\brief Remove all child controls from the list view."""
        for child in self._children:
            child.setParent(None)
        self._children.clear()
        self._positionsSettled = False

    def getRenderStates(self) -> RenderStates:
        r"""\brief Get the render states used when drawing this list view.

        - \return  Current render states
        """
        return self._renderStates

    def update(self, deltaTime: float) -> None:
        r"""\brief Update visible children in the list view.

        - \param deltaTime  Time elapsed since last update, in seconds
        """
        for child in self._children:
            if not (isinstance(child, FunctionalBase) and child.getVisible()):
                continue
            child.update(deltaTime)

    def lateUpdate(self, deltaTime: float) -> None:
        r"""\brief Run late update on children in the list view.

        - \param deltaTime  Time elapsed since last update, in seconds
        """
        for child in self._children:
            if not (isinstance(child, FunctionalBase) and child.getVisible()):
                continue
            child.lateUpdate(deltaTime)

    def fixedUpdate(self, fixedDelta: float) -> None:
        r"""\brief Run fixed-timestep update on children in the list view.

        - \param fixedDelta  Fixed timestep duration, in seconds
        """
        for child in self._children:
            if not (isinstance(child, FunctionalBase) and child.getVisible()):
                continue
            child.fixedUpdate(fixedDelta)

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        r"""\brief Draw the list view and its children to a render target.

        - \param target  Render target used for drawing
        - \param states  Render states used when drawing
        """
        self.applyPositions()
        states.transform *= self.getTransform()
        for child in self._children:
            target.draw(child, states)

    def applyPositions(self) -> None:
        r"""\brief Recompute child positions in the grid layout.

        Automatically called before drawing if positions have not been settled.
        """
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
            if isinstance(child, ControlBase):
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

    def _getRenderTransform(self) -> Transform:
        from .. import Scale

        transform = Transform()
        transform *= self.getTransform()
        transform.translate(self.getPosition() * (Scale - 1))
        return transform
