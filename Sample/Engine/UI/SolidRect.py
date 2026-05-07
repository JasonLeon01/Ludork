# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Tuple, Union
from .. import (
    TypeAdapter,
    Pair,
    RectangleShape,
    Vector2f,
    Color,
    FloatRect,
    RenderTarget,
    RenderStates,
    Transform,
)
from .Base import ControlBase


class SolidRect(ControlBase):
    @TypeAdapter(size=([tuple, list], Vector2f))
    def __init__(
        self,
        size: Union[Vector2f, Pair[float], List[float]],
        fillColor: Color = Color.White,
        outlineColor: Color = Color.Transparent,
        outlineThickness: float = 0.0,
    ) -> None:
        r"""Construct a solid rectangle control with logical-size scaling support.

        - \param size              Rectangle size in logical UI units
        - \param fillColor         Fill color of the rectangle
        - \param outlineColor      Outline color of the rectangle
        - \param outlineThickness  Outline thickness in logical UI units
        """
        from .. import Scale

        super().__init__()
        self._size = Vector2f(size.x, size.y)
        self._shape = RectangleShape(self._size * Scale)
        self._shape.setFillColor(fillColor)
        self._shape.setOutlineColor(outlineColor)
        self._shape.setOutlineThickness(outlineThickness * Scale)

    def getSize(self) -> Vector2f:
        r"""Get the rectangle size in logical UI units.

        - \return Current rectangle size
        """
        return Vector2f(self._size.x, self._size.y)

    @TypeAdapter(size=([tuple, list], Vector2f))
    def setSize(self, size: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""Set the rectangle size in logical UI units.

        - \param size  New rectangle size in logical UI units
        """
        from .. import Scale

        self._size = Vector2f(size.x, size.y)
        self._shape.setSize(self._size * Scale)

    def getFillColor(self) -> Color:
        r"""Get the fill color of this rectangle.

        - \return Current fill color
        """
        return self._shape.getFillColor()

    def setFillColor(self, color: Color) -> None:
        r"""Set the fill color of this rectangle.

        - \param color  New fill color
        """
        self._shape.setFillColor(color)

    def getOutlineColor(self) -> Color:
        r"""Get the outline color of this rectangle.

        - \return Current outline color
        """
        return self._shape.getOutlineColor()

    def setOutlineColor(self, color: Color) -> None:
        r"""Set the outline color of this rectangle.

        - \param color  New outline color
        """
        self._shape.setOutlineColor(color)

    def getOutlineThickness(self) -> float:
        r"""Get the outline thickness in logical UI units.

        - \return Current outline thickness in logical UI units
        """
        from .. import Scale

        return self._shape.getOutlineThickness() / Scale

    def setOutlineThickness(self, thickness: float) -> None:
        r"""Set the outline thickness in logical UI units.

        - \param thickness  New outline thickness in logical UI units
        """
        from .. import Scale

        self._shape.setOutlineThickness(thickness * Scale)

    def getLocalBounds(self) -> FloatRect:
        r"""Get local bounds in logical UI units.

        - \return Local bounds of this rectangle
        """
        from .. import Scale

        bounds = self._shape.getLocalBounds()
        return FloatRect(bounds.position, bounds.size / Scale)

    def getGlobalBounds(self) -> FloatRect:
        r"""Get global bounds in logical UI units.

        - \return Global bounds of this rectangle
        """
        from .. import Scale

        bounds = self._shape.getGlobalBounds()
        return FloatRect(bounds.position, bounds.size / Scale)

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        r"""Draw this rectangle control to the given render target.

        - \param target  Render target used for drawing
        - \param states  Render states used when drawing
        """
        self._applyRenderStates(states)
        if self.getVisible():
            target.draw(self._shape, states)

    def _applyRenderStates(self, states: RenderStates) -> None:
        from .. import Scale

        states.transform *= self.getTransform()
        states.transform.translate(self.getPosition() * (Scale - 1))

    def _getRenderTransform(self) -> Transform:
        from .. import Scale

        transform = Transform()
        transform *= self.getTransform()
        transform.translate(self.getPosition() * (Scale - 1))
        return transform

    def getAbsoluteBounds(self) -> FloatRect:
        r"""Get absolute screen-space bounds after parent transforms.

        - \return Absolute bounds in screen space
        """
        from .. import Scale

        transform = self._getScreenRenderTransform()
        bounds = self.getLocalBounds()
        realBounds = FloatRect(bounds.position * Scale, bounds.size * Scale)
        return transform.transformRect(realBounds)
