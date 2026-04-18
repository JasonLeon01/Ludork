# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
from typing import Dict, List, Optional, Tuple
from .. import (
    Color,
    Font,
    Text,
    Vector2f,
    RenderTarget,
    RenderStates,
    FloatRect,
    Transform,
)
from .Base import ControlBase


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


class RichText(ControlBase):
    def __init__(
        self,
        font: Font,
        text: str,
        styleCollection: Dict[str, TextStyle],
    ) -> None:
        self._font: Font = font
        self._color: Color = Color.White
        self._string: str = ""
        self._segments: List[Tuple[Text, TextStyle]] = []
        self._localBounds: FloatRect = FloatRect(Vector2f(), Vector2f())
        self._styleCollection = styleCollection
        super().__init__()
        self.setString(text)

    def setString(self, text: str) -> None:
        self._string = text
        self._render(text, self._styleCollection)
        self._refreshSegmentColors()

    def getString(self) -> str:
        return self._string

    def setColor(self, color: Color) -> None:
        self._color = color
        self._refreshSegmentColors()

    def getColor(self) -> Color:
        return self._color

    def getLocalBounds(self) -> FloatRect:
        from .. import Scale

        return FloatRect(self._localBounds.position, self._localBounds.size / Scale)

    def getGlobalBounds(self) -> FloatRect:
        return self.getLocalBounds()

    def getSize(self) -> Vector2f:
        return self._localBounds.size

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        self._applyRenderStates(states)
        if not self.getVisible():
            return
        for text, style in self._segments:
            self._applySegmentColor(text, style)
            target.draw(text, states)

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

    def _render(self, text: str, styleCollection: Dict[str, TextStyle]) -> None:
        from . import HexColor

        style = self._createDefaultStyle()
        self._segments = []
        minX = 0.0
        minY = 0.0
        maxX = 0.0
        maxY = 0.0
        hasVisibleSegment = False

        if text.endswith("\n"):
            text = text[:-1]
        lines = text.split("\n")
        y = 0.0
        for line in lines:
            lineSegments: List[Tuple[str, TextStyle]] = []
            bufferedText = ""
            savedMark = ""
            pauseRender = False
            for char in line:
                if char != "#":
                    if pauseRender:
                        savedMark = savedMark + char
                    else:
                        bufferedText = bufferedText + char
                    continue
                if pauseRender:
                    pauseRender = False
                    targetStyle = self._resolveStyleMarker(savedMark, styleCollection, HexColor)
                    if targetStyle is None:
                        bufferedText = bufferedText + f"#{savedMark}#"
                    else:
                        if bufferedText:
                            lineSegments.append((bufferedText, copy.copy(style)))
                            bufferedText = ""
                        style.adaptStyle(targetStyle)
                    savedMark = ""
                    continue
                if bufferedText:
                    lineSegments.append((bufferedText, copy.copy(style)))
                    bufferedText = ""
                pauseRender = True
            if pauseRender:
                bufferedText = bufferedText + f"#{savedMark}"
            if bufferedText:
                lineSegments.append((bufferedText, copy.copy(style)))

            x = 0.0
            lineHeight = 0.0
            for segmentText, segmentStyle in lineSegments:
                textObj = self._buildText(segmentText, segmentStyle)
                bounds = textObj.getLocalBounds()
                textObj.setPosition(Vector2f(x, y))
                self._segments.append((textObj, segmentStyle))
                hasVisibleSegment = True
                minX = min(minX, x + bounds.position.x)
                minY = min(minY, y + bounds.position.y)
                maxX = max(maxX, x + bounds.position.x + bounds.size.x)
                maxY = max(maxY, y + bounds.position.y + bounds.size.y)
                x += self._measureAdvance(textObj)
                lineHeight = max(lineHeight, bounds.position.y + bounds.size.y)
            y += lineHeight

        if hasVisibleSegment:
            self._localBounds = FloatRect(Vector2f(minX, minY), Vector2f(maxX - minX, maxY - minY))
        else:
            self._localBounds = FloatRect(Vector2f(), Vector2f())

    def _createDefaultStyle(self) -> TextStyle:
        from . import DefaultFontSize

        defaultStyle = TextStyle(DefaultFontSize, Text.Style.Regular, Color.White, Color.Black, 0.0)
        if "default" in self._styleCollection:
            defaultStyle.adaptStyle(self._styleCollection["default"])
        return defaultStyle

    def _resolveStyleMarker(
        self, marker: str, styleCollection: Dict[str, TextStyle], hexColorFactory
    ) -> Optional[TextStyle]:
        if marker in styleCollection:
            return styleCollection[marker]
        try:
            return TextStyle(fillColor=hexColorFactory(marker))
        except Exception:
            return None

    def _buildText(self, inText: str, style: TextStyle) -> Text:
        from .. import Scale

        text = Text(self._font, inText, int(style.characterSize * Scale))
        style.enableStyle(text)
        return text

    def _measureAdvance(self, text: Text) -> float:
        shapedGlyphs = text.getShapedGlyphs()
        if shapedGlyphs:
            # SFML 3.1 shaping keeps advances correct for ligatures and complex scripts.
            return max(glyph.position.x + glyph.glyph.advance for glyph in shapedGlyphs)
        bounds = text.getLocalBounds()
        return bounds.position.x + bounds.size.x

    def _refreshSegmentColors(self) -> None:
        for text, style in self._segments:
            self._applySegmentColor(text, style)

    def _applySegmentColor(self, text: Text, style: TextStyle) -> None:
        fillColor = style.fillColor if style.fillColor is not None else Color.White
        outlineColor = style.outlineColor if style.outlineColor is not None else Color.Black
        text.setFillColor(self._modulateColor(fillColor, self._color))
        text.setOutlineColor(self._modulateColor(outlineColor, self._color))

    def _modulateColor(self, baseColor: Color, factorColor: Color) -> Color:
        return Color(
            int(baseColor.r * factorColor.r / 255),
            int(baseColor.g * factorColor.g / 255),
            int(baseColor.b * factorColor.b / 255),
            int(baseColor.a * factorColor.a / 255),
        )
