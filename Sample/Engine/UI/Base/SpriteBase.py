# -*- encoding: utf-8 -*-

from __future__ import annotations
import logging
from typing import Optional
from ... import (
    Sprite,
    Vector2f,
    RenderStates,
    Texture,
    IntRect,
    FloatRect,
    Transform,
    Texture,
    IntRect,
    RenderStates,
    Color,
    FloatRect,
    RenderTarget,
)
from .ControlBase import ControlBase


class SpriteBase(ControlBase):
    """UI control backed by a textured sprite with bounds and color support."""

    def __init__(self, texture: Texture, rect: Optional[IntRect] = None) -> None:
        """Construct a sprite-based control from a texture and optional sub-rectangle."""
        from ...Utils import Render

        self._sprite: Sprite
        self._texture = texture
        self._renderStates: RenderStates = Render.CanvasRenderStates()
        if rect:
            self._sprite = Sprite(texture, rect)
        else:
            self._sprite = Sprite(texture)
        super().__init__()

    def setTexture(self, texture: Texture, resetRect: bool = False) -> None:
        r"""\brief Set the texture used by this sprite.

        - \param texture    New texture to use
        - \param resetRect  Whether to reset the texture rectangle
        """
        self._texture = texture
        self._sprite.setTexture(texture, resetRect)

    def getTexture(self) -> Texture:
        r"""\brief Get the texture used by this sprite.

        - \return  Current texture
        """
        return self._sprite.getTexture()

    def setTextureRect(self, rect: IntRect) -> None:
        r"""\brief Set the sub-rectangle of the texture to display.

        - \param rect  Sub-rectangle in texture coordinates
        """
        self._sprite.setTextureRect(rect)

    def getTextureRect(self) -> IntRect:
        r"""\brief Get the sub-rectangle of the texture being displayed.

        - \return  Current texture sub-rectangle
        """
        return self._sprite.getTextureRect()

    def setColour(self, colour: Color) -> None:
        r"""\brief Set the colour of this sprite.

        - \param colour  New colour
        """
        self._sprite.setColor(colour)

    def getColour(self) -> Color:
        r"""\brief Get the colour of this sprite.

        - \return  Current colour
        """
        return self._sprite.getColor()

    def getSize(self) -> Vector2f:
        r"""\brief Get the size of this sprite in logical UI units.

        - \return  Size as (width, height)
        """
        return self._sprite.getGlobalBounds().size

    def getLocalBounds(self) -> FloatRect:
        r"""\brief Get the local bounds of this sprite in logical UI units.

        - \return  Local bounds rectangle
        """
        from ... import Scale

        bounds = self._sprite.getLocalBounds()
        newBounds = FloatRect(bounds.position, bounds.size / Scale)
        return newBounds

    def getGlobalBounds(self) -> FloatRect:
        r"""\brief Get the global bounds of this sprite in logical UI units.

        - \return  Global bounds rectangle
        """
        from ... import Scale

        bounds = self._sprite.getGlobalBounds()
        newBounds = FloatRect(bounds.position, bounds.size / Scale)
        return newBounds

    def getRenderStates(self) -> RenderStates:
        r"""\brief Get the render states used for drawing.

        - \return  Current render states
        """
        return self._renderStates

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        r"""\brief Draw this sprite control to the given render target.

        - \param target  Render target used for drawing
        - \param states  Render states used when drawing
        """
        self._applyRenderStates(states)
        target.draw(self._sprite, states)

    def _applyRenderStates(self, states: RenderStates) -> None:
        from ... import Scale

        states.transform.translate(self.getPosition() * (Scale - 1))
        states.transform *= self.getTransform()

    def _getRenderTransform(self) -> Transform:
        from ... import Scale

        transform = Transform()
        transform.translate(self.getPosition() * (Scale - 1))
        transform *= self.getTransform()
        return transform

    def __del__(self) -> None:
        r"""\brief Destructor for SpriteBase.

        Logs at debug level when the object is collected (e.g. scene change).
        """
        logging.debug(f"SpriteBase {self} deleted")
