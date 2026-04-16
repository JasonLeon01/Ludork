# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
from typing import Dict, List, Optional
from .. import (
    Color,
    Font,
    Text,
    Vector2u,
    Vector2f,
    RenderTexture,
    RenderTarget,
    RenderStates,
    FloatRect,
    Transform,
)
from .Base import ControlBase, SpriteBase


class PlainText(ControlBase):
    def __init__(
        self,
        font: Font,
        text: str,
        characterSize: Optional[int] = None,
        style: Text.Style = Text.Style.Regular,
        fillColor: Color = Color.White,
    ) -> None:
        from . import DefaultFontSize
        from .. import Scale

        if characterSize is None:
            characterSize = DefaultFontSize
        self._characterSize = characterSize
        super().__init__()
        self._text = Text(font, text, int(characterSize * Scale))
        self._text.setStyle(style)
        self._text.setFillColor(fillColor)

    def getCharacterSize(self) -> int:
        return self._characterSize

    def setCharacterSize(self, characterSize: int) -> None:
        from .. import Scale

        self._characterSize = characterSize
        self._text.setCharacterSize(int(characterSize * Scale))

    def setString(self, text: str) -> None:
        self._text.setString(text)

    def getString(self) -> str:
        return self._text.getString()

    def getLocalBounds(self) -> FloatRect:
        from .. import Scale

        bounds = self._text.getLocalBounds()
        newBounds = FloatRect(bounds.position, bounds.size / Scale)
        return newBounds

    def getGlobalBounds(self) -> FloatRect:
        from .. import Scale

        bounds = self._text.getGlobalBounds()
        newBounds = FloatRect(bounds.position, bounds.size / Scale)
        return newBounds

    def getSize(self) -> Vector2f:
        return self._text.getGlobalBounds().size

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        self._applyRenderStates(states)
        if self.getVisible():
            target.draw(self._text, states)

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
        from .. import Scale

        transform = self._getScreenRenderTransform()
        bounds = self.getLocalBounds()
        realBounds = FloatRect(bounds.position * Scale, bounds.size * Scale)
        return transform.transformRect(realBounds)


class TextStyle:
    def __init__(
        self,
        characterSize: Optional[int] = None,
        style: Optional[Text.Style] = None,
        fillColor: Optional[Color] = None,
        outlineColor: Optional[Color] = None,
        outlineThickness: Optional[float] = None,
    ) -> None:
        self.characterSize = characterSize
        self.style = style
        self.fillColor = fillColor
        self.outlineColor = outlineColor
        self.outlineThickness = outlineThickness

    def enableStyle(self, text: Text) -> None:
        from .. import Scale

        if not self.characterSize is None:
            text.setCharacterSize(int(self.characterSize * Scale))
        if not self.style is None:
            text.setStyle(self.style)
        if not self.fillColor is None:
            text.setFillColor(self.fillColor)
        if not self.outlineColor is None:
            text.setOutlineColor(self.outlineColor)
        if not self.outlineThickness is None:
            text.setOutlineThickness(self.outlineThickness)

    def adaptStyle(self, inStyle: TextStyle) -> None:
        if not inStyle.characterSize is None:
            self.characterSize = inStyle.characterSize
        if not inStyle.style is None:
            self.style = inStyle.style
        if not inStyle.fillColor is None:
            self.fillColor = inStyle.fillColor
        if not inStyle.outlineColor is None:
            self.outlineColor = inStyle.outlineColor
        if not inStyle.outlineThickness is None:
            self.outlineThickness = inStyle.outlineThickness


class RichText(SpriteBase):
    def __init__(
        self,
        font: Font,
        text: str,
        styleCollection: Dict[str, TextStyle],
    ) -> None:
        self._textTexture: RenderTexture = None
        self._font: Font = font
        self._style: TextStyle = TextStyle()
        self._styleCollection = styleCollection
        if "default" in styleCollection:
            self._style = copy.copy(styleCollection["default"])
        self._render(text, styleCollection)
        super().__init__(self._textTexture.getTexture())

    def setString(self, text: str) -> None:
        self._render(text, self._styleCollection)
        self.setTexture(self._textTexture.getTexture(), True)

    def _render(self, text: str, styleCollection: Dict[str, TextStyle]) -> None:
        from . import HexColor
        from .. import Scale

        def modelText(inText: str) -> Text:
            text = Text(self._font, inText, int(self._style.characterSize * Scale))
            self._style.enableStyle(text)
            return text

        if text.endswith("\n"):
            text = text[:-1]
        lines = text.split("\n")
        texts: List[Text] = []
        maxWidth = 0
        maxHeight = 0
        for line in lines:
            texts.append([])
            savedMark = ""
            pauseRender = False
            lineWidth = 0
            lineHeight = 0
            for char in line:
                if char == "#":
                    if not pauseRender:
                        pauseRender = True
                    else:
                        pauseRender = False
                        targetStyle: TextStyle = None
                        if savedMark in styleCollection:
                            targetStyle = styleCollection[savedMark]
                        else:
                            targetStyle = eval(f"TextStyle(fillColor=HexColor('{savedMark}'))")
                        self._style.adaptStyle(targetStyle)
                        savedMark = ""
                else:
                    if pauseRender:
                        savedMark = savedMark + char
                    else:
                        textObj = modelText(char)
                        texts[-1].append(textObj)
                        lineWidth += textObj.getLocalBounds().size.x + textObj.getLocalBounds().position.x
                        lineHeight = max(
                            lineHeight, textObj.getLocalBounds().size.y + textObj.getLocalBounds().position.y
                        )
            if pauseRender:
                textObj = modelText(f"#{savedMark}")
                texts[-1].append(textObj)
                lineWidth += textObj.getLocalBounds().size.x + textObj.getLocalBounds().position.x
                lineHeight = max(lineHeight, textObj.getLocalBounds().size.y + textObj.getLocalBounds().position.y)
            maxWidth = int(max(maxWidth, lineWidth))
            maxHeight = int(maxHeight + lineHeight)
        if maxWidth == 0 and maxHeight == 0 and self._textTexture is None:
            self._textTexture = RenderTexture(Vector2u(1, 1))
        else:
            if self._textTexture is None:
                self._textTexture = RenderTexture(Vector2u(maxWidth, maxHeight))
            else:
                self._textTexture.resize(Vector2u(maxWidth, maxHeight))
        self._textTexture.clear(Color.Transparent)
        y = 0
        for line in texts:
            x = 0
            maxHeight = 0
            for text in line:
                text.setPosition(Vector2f(x, y))
                self._textTexture.draw(text)
                x += text.getLocalBounds().size.x + text.getLocalBounds().position.x
                maxHeight = max(maxHeight, text.getLocalBounds().size.y + text.getLocalBounds().position.y)
            y += maxHeight
        self._textTexture.display()
