# -*- encoding: utf-8 -*-

from __future__ import annotations
import math
from typing import List, Union
from . import Color, Transformable, Text, FloatRect, Font, RenderStates, RenderTarget, Vector2f


class TextStroke:
    def __init__(self, fill: Color = Color.White, outline: Color = Color.Transparent, thickness: float = 0.0) -> None:
        self.fill = fill
        self.outline = outline
        self.thickness = thickness

    def __repr__(self) -> str:
        return f"TextStroke(fill={self.fill}, outline={self.outline}, thickness={self.thickness})"

    def __eq__(self, other) -> bool:
        if not isinstance(other, TextStroke):
            return False
        return self.fill == other.fill and self.outline == other.outline and self.thickness == other.thickness

    def __ne__(self, other) -> bool:
        return not self.__eq__(other)


class Outline:
    def __init__(self, outline: Color = Color.Transparent, thickness: float = 0.0) -> None:
        self.outline = outline
        self.thickness = thickness

    def __repr__(self) -> str:
        return f"Outline(outline={self.outline}, thickness={self.thickness})"

    def __eq__(self, other) -> bool:
        if not isinstance(other, Outline):
            return False
        return self.outline == other.outline and self.thickness == other.thickness

    def __ne__(self, other) -> bool:
        return not self.__eq__(other)


class UI(Transformable):
    class Line(Transformable):
        def __init__(self) -> None:
            super().__init__()
            self._texts: List[Text] = []
            self._bounds: FloatRect = FloatRect()

        def setCharacterColor(self, pos: int, color: Color) -> None:
            assert pos >= 0 and pos < len(self), "pos is out of range"
            self._isolateCharacter(pos)
            stringToFormat = self._convertLinePosToLocal(pos)
            self._texts[stringToFormat].setFillColor(color)
            self._updateGeometry()

        def setCharacterStyle(self, pos: int, style: Text.Style) -> None:
            assert pos >= 0 and pos < len(self), "pos is out of range"
            self._isolateCharacter(pos)
            stringToFormat = self._convertLinePosToLocal(pos)
            self._texts[stringToFormat].setStyle(style)
            self._updateGeometry()

        def setCharacter(self, pos: int, character: str) -> None:
            assert pos >= 0 and pos < len(self), "pos is out of range"
            text: Text = self._texts[self._convertLinePosToLocal(pos)]
            string = text.getString()
            string = string[:pos] + character + string[pos + 1 :]
            text.setString(string)
            self._updateGeometry()

        def setCharacterSize(self, size: int) -> None:
            for text in self._texts:
                text.setCharacterSize(size)
            self._updateGeometry()

        def setFont(self, font: Font) -> None:
            for text in self._texts:
                text.setFont(font)
            self._updateGeometry()

        def getCharacterColor(self, pos: int) -> Color:
            assert pos >= 0 and pos < len(self), "pos is out of range"
            return self._texts[self._convertLinePosToLocal(pos)].getFillColor()

        def getCharacterStyle(self, pos: int) -> Text.Style:
            assert pos >= 0 and pos < len(self), "pos is out of range"
            return self._texts[self._convertLinePosToLocal(pos)].getStyle()

        def getCharacter(self, pos: int) -> str:
            assert pos >= 0 and pos < len(self), "pos is out of range"
            return self._texts[self._convertLinePosToLocal(pos)].getString()[pos]

        def getTexts(self) -> List[Text]:
            return self._texts

        def appendText(self, text: Text) -> None:
            self._updateTextAndGeometry(text)
            self._texts.append(text)

        def getLocalBounds(self) -> FloatRect:
            return self._bounds

        def getGlobalBounds(self) -> FloatRect:
            return self.getTransform().transformRect(self.getLocalBounds())

        def draw(self, target: RenderTarget, states: RenderStates) -> None:
            states.transform *= self.getTransform()
            for text in self._texts:
                target.draw(text, states)

        def _convertLinePosToLocal(self, pos: int) -> int:
            assert pos >= 0 and pos < len(self), "pos is out of range"
            arrayIndex = 0
            while pos >= len(self._texts[arrayIndex].getString()):
                pos -= len(self._texts[arrayIndex].getString())
                arrayIndex += 1
            return arrayIndex

        def _isolateCharacter(self, pos: int) -> None:
            def clone_text(source_text: Text):
                new_text = Text(self._texts[0].getFont(), "", self._texts[0].getCharacterSize())
                new_text.setFillColor(source_text.getFillColor())
                new_text.setOutlineColor(source_text.getOutlineColor())
                new_text.setOutlineThickness(source_text.getOutlineThickness())
                new_text.setStyle(source_text.getStyle())
                return new_text

            localPos = pos
            index = self._convertLinePosToLocal(localPos)
            temp = clone_text(self._texts[index])
            string = temp.getString()
            if len(string) == 1:
                return
            self._texts.pop(index)
            if localPos != len(string):
                temp.setString(string[localPos + 1 :])
                self._texts.insert(index, temp)
            temp.setString(string[localPos])
            self._texts.insert(index, temp)
            if localPos != 0:
                temp.setString(string[:localPos])
                self._texts.insert(index, temp)

        def _updateGeometry(self) -> None:
            self._bounds = FloatRect()
            for text in self._texts:
                self._updateTextAndGeometry(text)

        def _updateTextAndGeometry(self, text: Text) -> None:
            text.setPosition(Vector2f(self._bounds.size.x, 0))
            lineSpacing = math.floor(text.getFont().getLineSpacing(text.getCharacterSize()))
            self._bounds.size.x += text.getLocalBounds().size.x
            self._bounds.size.y = max(self._bounds.size.y, lineSpacing)

        def __len__(self) -> int:
            result = 0
            for text in self._texts:
                result += len(text.getString())
            return result

    def __init__(self, font: Font) -> None:
        super().__init__()
        self._lines: List[UI.Line] = []
        self._font: Font = font
        self._characterSize: int = 22
        self._bounds: FloatRect = FloatRect()
        self._currentStroke: TextStroke = TextStroke(Color.White, Color.Transparent)
        self._currentStyle: Text.Style = Text.Style.Regular

    def setCharacterColor(self, line: int, pos: int, color: Color) -> None:
        assert line >= 0 and line < len(self._lines), "line is out of range"
        self._lines[line].setCharacterColor(pos, color)
        self._updateGeometry()

    def setCharacterStyle(self, line: int, pos: int, style: Text.Style) -> None:
        assert line >= 0 and line < len(self._lines), "line is out of range"
        self._lines[line].setCharacterStyle(pos, style)
        self._updateGeometry()

    def setCharacter(self, line: int, pos: int, character: str) -> None:
        assert line >= 0 and line < len(self._lines), "line is out of range"
        self._lines[line].setCharacter(pos, character)
        self._updateGeometry()

    def setCharacterSize(self, size: int) -> None:
        if self._characterSize == size:
            return
        self._characterSize = size
        for line in self._lines:
            line.setCharacterSize(size)
        self._updateGeometry()

    def setFont(self, font: Font) -> None:
        if self._font == font:
            return
        self._font = font
        for line in self._lines:
            line.setFont(font)
        self._updateGeometry()

    def clear(self) -> None:
        self._lines.clear()
        self._bounds = FloatRect()

    def getCharacterColor(self, line: int, pos: int) -> Color:
        assert line >= 0 and line < len(self._lines), "line is out of range"
        return self._lines[line].getCharacterColor(pos)

    def getCharacterStyle(self, line: int, pos: int) -> Text.Style:
        assert line >= 0 and line < len(self._lines), "line is out of range"
        return self._lines[line].getCharacterStyle(pos)

    def getCharacter(self, line: int, pos: int) -> str:
        assert line >= 0 and line < len(self._lines), "line is out of range"
        return self._lines[line].getCharacter(pos)

    def getLines(self) -> List[Line]:
        return self._lines

    def getCharacterSize(self) -> int:
        return self._characterSize

    def getFont(self) -> Font:
        return self._font

    def getLocalBounds(self) -> FloatRect:
        return self._bounds

    def getGlobalBounds(self) -> FloatRect:
        return self.getTransform().transformRect(self.getLocalBounds())

    def draw(self, target: RenderTarget, states: RenderStates = None) -> None:
        if not states:
            states = RenderStates.Default()
        states.transform *= self.getTransform()
        for line in self._lines:
            line.draw(target, states)

    def _createText(self, string: str) -> Text:
        text = Text(self._font, string, self._characterSize)
        text.setFillColor(self._currentStroke.fill)
        text.setOutlineColor(self._currentStroke.outline)
        text.setOutlineThickness(self._currentStroke.thickness)
        text.setStyle(self._currentStyle)
        return text

    def _updateGeometry(self) -> None:
        self._bounds = FloatRect()
        for line in self._lines:
            line.setPosition(Vector2f(0, self._bounds.size.y))
            self._bounds.size.x = max(self._bounds.size.x, line.getGlobalBounds().size.x)
            self._bounds.size.y += line.getGlobalBounds().size.y

    def __lshift__(self, arg: Union[TextStroke, Outline, Color, Text.Style, str]):
        def explode(inString: str, inDelimiter: str) -> List[str]:
            if not inString:
                return []
            result: List[str] = []
            buffer = ""
            for char in inString:
                if char == inDelimiter:
                    result.append(buffer)
                    buffer = ""
                else:
                    buffer += char
            if buffer or inString[len(inString) - 1] == inDelimiter:
                result.append(buffer)
            return result

        if isinstance(arg, TextStroke):
            stroke = arg
            self._currentStroke = stroke
        elif isinstance(arg, Outline):
            outline = arg
            self._currentStroke.outline = outline.outline
            self._currentStroke.thickness = outline.thickness
        elif isinstance(arg, Color):
            color = arg
            self._currentStroke.fill = color
        elif isinstance(arg, Text.Style):
            style = arg
            self._currentStyle = style
        elif isinstance(arg, str):
            string = arg
            if not string:
                return self
            subStrings = explode(string, "\n")
            if subStrings:
                subString = subStrings[0]
                if not self._lines:
                    self._lines.append(UI.Line())
                line = self._lines[-1]
                self._bounds.size.y -= line.getGlobalBounds().size.y
                line.appendText(self._createText(subString))
                self._bounds.size.x = max(self._bounds.size.x, line.getGlobalBounds().size.x)
                self._bounds.size.y += line.getGlobalBounds().size.y
            index = 1
            while index < len(subStrings):
                line = UI.Line()
                line.setPosition(Vector2f(0, self._bounds.size.y))
                line.appendText(self._createText(subStrings[index]))
                self._lines.append(line)
                self._bounds.size.x = max(self._bounds.size.x, line.getGlobalBounds().size.x)
                self._bounds.size.y += line.getGlobalBounds().size.y
                index += 1
        else:
            raise TypeError("Invalid type")
        return self
