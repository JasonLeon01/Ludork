# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Dict, List, TYPE_CHECKING
from .. import (
    Color,
    Text,
    Vector2u,
    Vector2f,
    RenderTexture,
    RenderTarget,
    RenderStates,
)
from .Base import ControlBase, SpriteBase

if TYPE_CHECKING:
    from Engine import Font


class PlainText(ControlBase):
    def __init__(
        self,
        font: Font,
        text: str,
        characterSize: int,
        style: Text.Style = Text.Style.Regular,
        fillColor: Color = Color.White,
    ) -> None:
        from .. import System

        self._characterSize = characterSize
        super().__init__()
        self._text = Text(font, text, int(characterSize * System.getScale()))
        self._text.setStyle(style)
        self._text.setFillColor(fillColor)

    def getCharacterSize(self) -> int:
        return self._characterSize

    def setCharacterSize(self, characterSize: int) -> None:
        from .. import System

        self._characterSize = characterSize
        self._text.setCharacterSize(int(characterSize * System.getScale()))

    def setString(self, text: str) -> None:
        self._text.setString(text)

    def getString(self) -> str:
        return self._text.getString()

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        states.transform *= self.getTransform()
        if self.getVisible():
            target.draw(self._text, states)


class TextStyle:
    def __init__(
        self,
        characterSize: int = 30,
        style: Text.Style = Text.Style.Regular,
        fillColor: Color = Color.White,
        outlineColor: Color = Color.Transparent,
        outlineThickness: float = 0.0,
    ) -> None:
        self.characterSize = characterSize
        self.style = style
        self.fillColor = fillColor
        self.outlineColor = outlineColor
        self.outlineThickness = outlineThickness

    def enableStyle(self, text: Text):
        from .. import System

        text.setCharacterSize(int(self.characterSize * System.getScale()))
        text.setStyle(self.style)
        text.setFillColor(self.fillColor)
        text.setOutlineColor(self.outlineColor)
        text.setOutlineThickness(self.outlineThickness)


class RichText(SpriteBase):
    def __init__(
        self,
        font: Font,
        text: str,
        styleCollection: Dict[str, TextStyle],
    ) -> None:
        self._texture: RenderTexture = None
        self._font: Font = font
        self._style: TextStyle = TextStyle()
        if "default" in styleCollection:
            self._style = styleCollection["default"]
        self._render(text, styleCollection)
        super().__init__(self._texture.getTexture())

    def _render(self, text: str, styleCollection: Dict[str, Color]) -> None:
        def modelText(inText: str):
            text = Text(self._font, inText, self._style.characterSize)
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
                        if not savedMark in styleCollection:
                            texts[-1].append(modelText(f"#{savedMark}#"))
                        else:
                            self._style = styleCollection[savedMark]
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
        self._texture = RenderTexture(Vector2u(maxWidth, maxHeight))
        self._texture.clear(Color.Transparent)
        y = 0
        for line in texts:
            x = 0
            maxHeight = 0
            for text in line:
                text.setPosition(Vector2f(x, y))
                self._texture.draw(text)
                x += text.getLocalBounds().size.x + text.getLocalBounds().position.x
                maxHeight = max(maxHeight, text.getLocalBounds().size.y + text.getLocalBounds().position.y)
            y += maxHeight
        self._texture.display()
