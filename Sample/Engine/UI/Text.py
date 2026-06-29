# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
from typing import Dict, List, Optional, Tuple, Union
from .. import (
    Color,
    Font,
    Pair,
    Text,
    Vector2f,
    RenderTarget,
    RenderStates,
    FloatRect,
)
from .Base import ControlBase


class PlainText(ControlBase):
    r"""Plain text control rendered with a single font and style.

    Wraps an SFML Text object and provides logical-size scaling support.
    """

    def __init__(
        self,
        font: Font,
        text: str,
        characterSize: Optional[int] = None,
        style: Text.Style = Text.Style.Regular,
        fillColor: Color = Color.White,
    ) -> None:
        r"""\brief Construct a PlainText control.

        - \param font           Font used for rendering
        - \param text           Initial text string
        - \param characterSize  Character size in logical UI units (uses default if None)
        - \param style          Text style (regular, bold, italic, etc.)
        - \param fillColor      Fill colour of the text
        """
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
        r"""\brief Get the character size of this text.

        - \return  Character size in logical UI units
        """
        return self._characterSize

    def setCharacterSize(self, characterSize: int) -> None:
        r"""\brief Set the character size of this text.

        - \param characterSize  New character size in logical UI units
        """
        from .. import Scale

        self._characterSize = characterSize
        self._text.setCharacterSize(int(characterSize * Scale))

    def setString(self, text: str) -> None:
        r"""\brief Set the text string.

        - \param text  New text string
        """
        self._text.setString(text)

    def setLineAlignment(self, lineAlignment: Text.LineAlignment) -> None:
        r"""\brief Set the line alignment for multi-line text.

        - \param lineAlignment New line alignment
        """
        self._text.setLineAlignment(lineAlignment)

    def getString(self) -> str:
        r"""\brief Get the current text string.

        - \return  Current text string
        """
        return self._text.getString()

    def getLocalBounds(self) -> FloatRect:
        r"""\brief Get the local bounds of this text in logical UI units.

        - \return  Local bounds rectangle
        """
        from .. import Scale

        bounds = self._text.getLocalBounds()
        newBounds = FloatRect(bounds.position, bounds.size / Scale)
        return newBounds

    def getGlobalBounds(self) -> FloatRect:
        r"""\brief Get the global bounds of this text in logical UI units.

        - \return  Global bounds rectangle
        """
        from .. import Scale

        bounds = self._text.getGlobalBounds()
        newBounds = FloatRect(bounds.position, bounds.size / Scale)
        return newBounds

    def getSize(self) -> Vector2f:
        r"""\brief Get the size of this text in logical UI units.

        - \return  Size as (width, height)
        """
        from .. import Scale

        return self._text.getGlobalBounds().size / Scale

    def getOrigin(self) -> Vector2f:
        r"""\brief Get the origin of this text in logical UI units.

        - \return  Origin position in logical UI units
        """
        from .. import Scale

        origin = super().getOrigin()
        return origin / Scale

    @TypeAdapter(origin=([tuple, list], Vector2f))
    def setOrigin(self, origin: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the origin of this text in logical UI units.

        - \param origin  New origin in logical UI units
        """
        from .. import Scale

        super().setOrigin(origin * Scale)

    def getColour(self) -> Color:
        r"""\brief Get the fill colour of this text.

        - \return  Current fill colour
        """
        return self._text.getFillColor()

    def setColour(self, colour: Color) -> None:
        r"""\brief Set the fill colour of this text.

        - \param colour  New fill colour
        """
        self._text.setFillColor(colour)

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        r"""\brief Draw this text control to the given render target.

        - \param target  Render target used for drawing
        - \param states  Render states used when drawing
        """
        self._applyRenderStates(states)
        if self.getVisible():
            target.draw(self._text, states)


class TextStyle:
    r"""Text rendering style descriptor.

    Holds optional character size, font style, fill/outline colour,
    and outline thickness for rich-text rendering.
    """

    def __init__(
        self,
        characterSize: Optional[int] = None,
        style: Optional[Text.Style] = None,
        fillColor: Optional[Color] = None,
        outlineColor: Optional[Color] = None,
        outlineThickness: Optional[float] = None,
    ) -> None:
        r"""\brief Construct a TextStyle with optional overrides.

        - \param characterSize   Character size in logical UI units (None = use default)
        - \param style           Text style (regular, bold, italic, etc.)
        - \param fillColor       Fill colour of the text
        - \param outlineColor    Outline colour of the text
        - \param outlineThickness  Outline thickness in logical UI units
        """
        self.characterSize = characterSize
        self.style = style
        self.fillColor = fillColor
        self.outlineColor = outlineColor
        self.outlineThickness = outlineThickness

    def enableStyle(self, text: Text) -> None:
        r"""\brief Apply this style to an SFML Text object.

        - \param text  Text object to modify
        """
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
        r"""\brief Merge non-None fields from another TextStyle.

        - \param inStyle  Source style to adapt from
        """
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
    r"""Rich-text control supporting multiple styles and colours.

    Parses a marked-up string and renders it as multiple SFML Text
    segments with per-segment style and colour control.
    """

    def __init__(
        self,
        font: Font,
        text: str,
        styleCollection: Dict[str, TextStyle],
    ) -> None:
        r"""\brief Construct a RichText control.

        - \param font              Font used for rendering
        - \param text              Initial rich-text string
        - \param styleCollection  Named styles used by the markup
        """
        self._font: Font = font
        self._colour: Color = Color.White
        self._string: str = ""
        self._segments: List[Tuple[Text, TextStyle]] = []
        self._localBounds: FloatRect = FloatRect(Vector2f(), Vector2f())
        self._styleCollection = styleCollection
        super().__init__()
        self.setString(text)

    def setString(self, text: str) -> None:
        r"""\brief Set the rich-text string and re-render.

        - \param text  New rich-text string (may include style markers)
        """
        self._string = text
        self._render(text, self._styleCollection)
        self._refreshSegmentColours()

    def getString(self) -> str:
        r"""\brief Get the current rich-text string.

        - \return  Current rich-text string
        """
        return self._string

    def setColour(self, colour: Color) -> None:
        r"""\brief Set the modulation colour for all segments.

        - \param colour  New modulation colour
        """
        self._colour = colour
        self._refreshSegmentColours()

    def getColour(self) -> Color:
        r"""\brief Get the current modulation colour.

        - \return  Current modulation colour
        """
        return self._colour

    def getLocalBounds(self) -> FloatRect:
        r"""\brief Get the local bounds of this rich text in logical UI units.

        - \return  Local bounds rectangle
        """
        from .. import Scale

        return FloatRect(self._localBounds.position, self._localBounds.size / Scale)

    def getGlobalBounds(self) -> FloatRect:
        r"""\brief Get the global bounds of this rich text.

        - \return  Global bounds rectangle
        """
        return self.getLocalBounds()

    def getSize(self) -> Vector2f:
        r"""\brief Get the size of this rich text in logical UI units.

        - \return  Size as (width, height)
        """
        from .. import Scale

        return self._localBounds.size / Scale

    def getOrigin(self) -> Vector2f:
        r"""\brief Get the origin of this rich text in logical UI units.

        - \return  Origin position in logical UI units
        """
        from .. import Scale

        origin = super().getOrigin()
        return origin / Scale

    @TypeAdapter(origin=([tuple, list], Vector2f))
    def setOrigin(self, origin: Union[Vector2f, Pair[float], List[float]]) -> None:
        r"""\brief Set the origin of this rich text in logical UI units.

        - \param origin  New origin in logical UI units
        """
        from .. import Scale

        super().setOrigin(origin * Scale)

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        r"""\brief Draw this rich text control to the given render target.

        - \param target  Render target used for drawing
        - \param states  Render states used when drawing
        """
        self._applyRenderStates(states)
        if not self.getVisible():
            return
        for text, style in self._segments:
            self._applySegmentColour(text, style)
            target.draw(text, states)

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

            pendingSegments: List[Tuple[Text, TextStyle, FloatRect, float]] = []
            for segmentText, segmentStyle in lineSegments:
                textObj = self._buildText(segmentText, segmentStyle)
                bounds = textObj.getLocalBounds()
                baseline = self._getBaseline(textObj)
                pendingSegments.append((textObj, segmentStyle, bounds, baseline))

            if not pendingSegments:
                continue

            lineBaseline = max(baseline for _, _, _, baseline in pendingSegments)

            x = 0.0
            lineHeight = 0.0
            for textObj, segmentStyle, bounds, baseline in pendingSegments:
                segmentY = y + lineBaseline - baseline
                textObj.setPosition(Vector2f(x, segmentY))
                self._segments.append((textObj, segmentStyle))
                hasVisibleSegment = True
                minX = min(minX, x + bounds.position.x)
                minY = min(minY, segmentY + bounds.position.y)
                maxX = max(maxX, x + bounds.position.x + bounds.size.x)
                maxY = max(maxY, segmentY + bounds.position.y + bounds.size.y)
                lineHeight = max(lineHeight, segmentY + bounds.position.y + bounds.size.y - y)
                x += self._measureAdvance(textObj)
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
        self, marker: str, styleCollection: Dict[str, TextStyle], hexColourFactory
    ) -> Optional[TextStyle]:
        if marker in styleCollection:
            return styleCollection[marker]
        try:
            return TextStyle(fillColor=hexColourFactory(marker))
        except Exception:
            return None

    def _buildText(self, inText: str, style: TextStyle) -> Text:
        from .. import Scale

        assert style.characterSize
        text = Text(self._font, inText, int(style.characterSize * Scale))
        style.enableStyle(text)
        return text

    def _getBaseline(self, text: Text) -> float:
        shapedGlyphs = text.getShapedGlyphs()
        if shapedGlyphs:
            return max(glyph.position.y for glyph in shapedGlyphs)
        bounds = text.getLocalBounds()
        return bounds.position.y + bounds.size.y

    def _measureAdvance(self, text: Text) -> float:
        shapedGlyphs = text.getShapedGlyphs()
        if shapedGlyphs:
            return max(glyph.position.x + glyph.glyph.advance for glyph in shapedGlyphs)
        bounds = text.getLocalBounds()
        return bounds.position.x + bounds.size.x

    def _refreshSegmentColours(self) -> None:
        for text, style in self._segments:
            self._applySegmentColour(text, style)

    def _applySegmentColour(self, text: Text, style: TextStyle) -> None:
        fillColor = style.fillColor if style.fillColor is not None else Color.White
        outlineColor = style.outlineColor if style.outlineColor is not None else Color.Black
        text.setFillColor(self._modulateColour(fillColor, self._colour))
        text.setOutlineColor(self._modulateColour(outlineColor, self._colour))

    def _modulateColour(self, baseColour: Color, factorColour: Color) -> Color:
        return Color(
            int(baseColour.r * factorColour.r / 255),
            int(baseColour.g * factorColour.g / 255),
            int(baseColour.b * factorColour.b / 255),
            int(baseColour.a * factorColour.a / 255),
        )
