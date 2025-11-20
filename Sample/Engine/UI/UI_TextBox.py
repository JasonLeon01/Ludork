# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
from typing import Optional, TYPE_CHECKING
from . import UI_SpriteBase, UI_Window, IntRect, RectangleShape, Vector2f, Text, RenderTexture, Sprite, Color
from ..Utils import Math

if TYPE_CHECKING:
    from Engine import Image

SpriteBase = UI_SpriteBase.SpriteBase
Window = UI_Window.Window


class TextBox(SpriteBase):
    def __init__(self, rect: IntRect, windowSkin: Optional[Image] = None) -> None:
        from .. import System

        assert rect.size.x > 16 and rect.size.y > 16
        self._realRect = copy.copy(rect)
        self._realRect.size.x -= 16
        self._realRect.size.y -= 16
        self._realRect.size = Vector2f(
            self._realRect.size.x * System.getScale(), self._realRect.size.y * System.getScale()
        )
        self._bg = Window(rect, windowSkin)
        self._rectangle = RectangleShape(Math.ToVector2f(self._realRect.size))
        self._rectangle.setPosition(Vector2f(16, 16))
        self._rectangle.setFillColor(Color(255, 255, 255, self.getColor().a))
        self._textLeft: str = ""
        self._textRight: str = ""
        self._active: bool = False
        self._textControl: Text = Text(System.getFonts()[0], "|", min(self._realRect.size.y, 30))
        self._textControl.setFillColor(Color.Black)
        self._textControlTexture: RenderTexture = RenderTexture(Math.ToVector2u(self._realRect.size))
        self._textControlSprite: Sprite = Sprite(self._textControlTexture.getTexture())
        self._textControlSprite.setPosition(Vector2f(16, 16))
        super().__init__(self._bg.getTexture())

    def getText(self) -> str:
        return self._textLeft + self._textRight

    def setText(self, text: str) -> None:
        self._textLeft = text
        self._textRight = ""

    def isActive(self) -> bool:
        return self._active

    def setActive(self, active: bool) -> None:
        self._active = active

    def update(self, deltaTime: float) -> None:
        from .. import Input

        if not self._active:
            return
        if Input.isTextEntered():
            self._textLeft += Input.getEnteredText()
        if Input.getKeyPressed(Input.Key.Backspace, True) and self._textLeft:
            self._textLeft = self._textLeft[:-1]
        if Input.getKeyPressed(Input.Key.Left, True) and self._textLeft:
            changedText = self._textLeft[-1]
            self._textRight = changedText + self._textRight
            self._textLeft = self._textLeft[:-1]
        if Input.getKeyPressed(Input.Key.Right, True) and self._textRight:
            changedText = self._textRight[0]
            self._textLeft += changedText
            self._textRight = self._textRight[1:]

        self._textControl.setString(self._textLeft + "|" + self._textRight)
        self._textControlTexture.clear(Color.Transparent)
        if self._textControl.getLocalBounds().size.x < self._realRect.size.x:
            self._textControlSprite.setPosition(Vector2f(0, 0))
        else:
            self._textControlSprite.setPosition(
                Vector2f(self._realRect.size.x - self._textControl.getLocalBounds().size.x, 0)
            )
        self._textControlTexture.draw(self._textControl)
        self._textControlTexture.display()

    def draw(self, target: RenderTexture, states: RenderStates) -> None:
        super().draw(target, states)
        target.draw(self._rectangle, states)
        target.draw(self._textControlSprite, states)
