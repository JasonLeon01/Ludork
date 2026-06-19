# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Tuple, Union
from .. import Angle, Pair, IntRect, RenderTarget, RenderStates, Transform, TypeAdapter, Vector2f, Vector2i, degrees
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

    def v_getPosition(self) -> Pair[float]:
        r"""\brief Get the list view position as a plain pair.

        - \return  Position as (x, y)
        """
        result = super().getPosition()
        return (result.x, result.y)

    @TypeAdapter(position=([tuple, list], Vector2f))
    def setPosition(self, position: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the list view position.

        - \param position  New position in logical UI units
        """
        super().setPosition(position)

    @TypeAdapter(offset=([tuple, list], Vector2f))
    def move(self, offset: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Move the list view by an offset.

        - \param offset  Offset to add to the current position
        """
        super().move(offset)

    def v_getRotation(self) -> float:
        r"""\brief Get the list view rotation as a plain number.

        - \return  Rotation in degrees
        """
        result = super().getRotation()
        return result.asDegrees()

    def setRotation(self, angle: Union[Angle, float]) -> None:
        r"""\brief Set the list view rotation.

        - \param angle  Rotation (Angle object or degrees)
        """
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        super().setRotation(angle)

    def rotate(self, angle: Union[Angle, float]) -> None:
        r"""\brief Rotate the list view by an offset.

        - \param angle  Rotation offset (Angle object or degrees)
        """
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        super().rotate(angle)

    def v_getScale(self) -> Pair[float]:
        r"""\brief Get the list view scale as a plain pair.

        - \return  Scale as (x, y)
        """
        result = super().getScale()
        return (result.x, result.y)

    @TypeAdapter(scale=([tuple, list], Vector2f))
    def setScale(self, scale: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the list view scale.

        - \param scale  New scale
        """
        super().setScale(scale)

    @TypeAdapter(factor=([tuple, list], Vector2f))
    def scale(self, factor: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Scale the list view by a factor.

        - \param factor  Scale factor to multiply by
        """
        super().scale(factor)

    def v_getOrigin(self) -> Pair[float]:
        r"""\brief Get the list view origin as a plain pair.

        - \return  Origin as (x, y) in logical UI units
        """
        origin = self.getOrigin()
        return (origin.x, origin.y)

    def getOrigin(self) -> Vector2f:
        r"""\brief Get the list view origin in logical UI units.

        - \return  Origin position in logical UI units
        """
        from .. import Scale

        origin = super().getOrigin()
        return origin / Scale

    @TypeAdapter(origin=([tuple, list], Vector2f))
    def setOrigin(self, origin: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the list view origin in logical UI units.

        - \param origin  Origin position in logical UI units
        """
        from .. import Scale

        super().setOrigin(origin * Scale)

    def getColumns(self) -> int:
        r"""\brief Get the number of columns in the list view.

        - \return  Current column count
        """
        return self._columns

    def setSize(self, size: Vector2i) -> None:
        r"""\brief Set the list view size and invalidate child layout.

        - \param size  New list view size
        """
        self.size = size
        self._positionsSettled = False

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
            colCentre = 16 + col * colWidth + colWidth / 2
            originX = child.getOrigin().x
            scaleX = child.getScale().x
            posX = colCentre - itemWidth / 2 + originX * scaleX
            currentRowHeights.append(itemHeight)
            rowHeight = max(rowHeight, itemHeight)
            child.setPosition((posX, currentY))

    def _getRenderTransform(self) -> Transform:
        from .. import Scale

        transform = Transform()
        transform *= self.getTransform()
        transform.translate(self.getPosition() * (Scale - 1))
        return transform
