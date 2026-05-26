# -*- encoding: utf-8 -*-

from __future__ import annotations
import logging
from typing import Optional, List, Union, TYPE_CHECKING
from ... import TypeAdapter, Pair, Drawable, Transformable, Vector2f, Angle, degrees, Transform, FloatRect

if TYPE_CHECKING:
    from Engine.UI import Canvas, ListView


class ControlBase(Drawable, Transformable):
    """Base class for all UI controls.

    Provides visibility, activity state, naming, parent-child relationships,
    and transform operations (position, rotation, scale, origin).
    """

    def __init__(self) -> None:
        """Construct an empty UI control with default visibility and activity."""
        Drawable.__init__(self)
        Transformable.__init__(self)
        self._visible: bool = True
        self._name: str = ""
        self._parent: Optional[Union[Canvas, ListView]] = None

    def getVisible(self) -> bool:
        r"""\brief Check whether this control is visible.

        - \return  True if visible, False otherwise
        """
        return self._visible

    def setVisible(self, visible: bool) -> None:
        r"""\brief Set the visibility of this control.

        - \param visible  True to make visible, False to hide
        """
        self._visible = visible

    def getName(self) -> str:
        r"""\brief Get the name of this control.

        - \return  Control name
        """
        return self._name

    def setName(self, name: str) -> None:
        r"""\brief Set the name of this control.

        - \param name  New control name
        """
        self._name = name

    def getParent(self) -> Optional[Union[Canvas, ListView]]:
        r"""\brief Get the parent control.

        - \return  Parent control, or None if no parent
        """
        return self._parent

    def setParent(self, parent: Optional[Union[Canvas, ListView]]) -> None:
        r"""\brief Set the parent control.

        - \param parent  New parent control, or None to detach
        """
        self._parent = parent

    def getChildren(self) -> List["ControlBase"]:
        r"""\brief Get child controls.

        - \return  Empty list for leaf controls
        """
        return []

    def getSize(self) -> Vector2f:
        r"""\brief Get the control size.

        - \return  Zero size for controls without explicit bounds
        """
        return Vector2f()

    def getLocalBounds(self) -> FloatRect:
        r"""\brief Get local bounds.

        - \return  Bounds derived from the default size
        """
        return FloatRect(Vector2f(), self.getSize())

    def getAbsoluteBounds(self) -> FloatRect:
        r"""\brief Get absolute screen-space bounds after parent transforms.

        - \return  Absolute bounds in screen space
        """
        from ... import Scale

        transform = self._getScreenRenderTransform()
        bounds = self.getLocalBounds()
        realBounds = FloatRect(bounds.position * Scale, bounds.size * Scale)
        return transform.transformRect(realBounds)

    def getRenderStates(self):
        r"""\brief Get default render states.

        - \return  Default canvas render states
        """
        from ...Utils import Render

        return Render.CanvasRenderStates()

    def v_getPosition(self) -> Pair[float]:
        r"""\brief Get the position as a plain pair.

        - \return  Position as (x, y)
        """
        result = super().getPosition()
        return (result.x, result.y)

    @TypeAdapter(position=([tuple, list], Vector2f))
    def setPosition(self, position: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the position of this control.

        - \param position  New position in logical UI units
        """
        super().setPosition(position)

    @TypeAdapter(offset=([tuple, list], Vector2f))
    def move(self, offset: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Move the control by an offset.

        - \param offset  Offset to add to the current position
        """
        super().move(offset)

    def v_getRotation(self) -> float:
        r"""\brief Get the rotation as a plain number (degrees).

        - \return  Rotation in degrees
        """
        result = super().getRotation()
        return result.asDegrees()

    def setRotation(self, angle: Union[Angle, float]) -> None:
        r"""\brief Set the rotation of this control.

        - \param angle  Rotation (Angle object or degrees)
        """
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        super().setRotation(angle)

    def rotate(self, angle: Union[Angle, float]) -> None:
        r"""\brief Rotate the control by an offset.

        - \param angle  Rotation offset (Angle object or degrees)
        """
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        super().rotate(angle)

    def v_getScale(self) -> Pair[float]:
        r"""\brief Get the scale as a plain pair.

        - \return  Scale as (x, y)
        """
        result = super().getScale()
        return (result.x, result.y)

    @TypeAdapter(scale=([tuple, list], Vector2f))
    def setScale(self, scale: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the scale of this control.

        - \param scale  New scale in logical UI units
        """
        super().setScale(scale)

    @TypeAdapter(factor=([tuple, list], Vector2f))
    def scale(self, factor: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Scale the control by a factor.

        - \param factor  Scale factor to multiply by
        """
        super().scale(factor)

    def v_getOrigin(self) -> Pair[float]:
        r"""\brief Get the origin as a plain pair.

        - \return  Origin as (x, y)
        """
        result = super().getOrigin()
        return (result.x, result.y)

    @TypeAdapter(origin=([tuple, list], Vector2f))
    def setOrigin(self, origin: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the origin of this control.

        - \param origin  New origin in logical UI units
        """
        return super().setOrigin(origin)

    def _getScreenTransform(self) -> Transform:
        transform = self.getTransform()
        if self._parent:
            return self._parent._getScreenTransform() * transform
        return transform

    def _getRenderTransform(self) -> Transform:
        return self.getTransform()

    def _getScreenRenderTransform(self) -> Transform:
        transform = self._getRenderTransform()
        if self._parent:
            return self._parent._getScreenRenderTransform() * transform
        return transform

    def __del__(self) -> None:
        r"""\brief Destructor for ControlBase.

        Logs at debug level when the object is collected.
        """
        logging.debug(f"ControlBase {self} deleted")
