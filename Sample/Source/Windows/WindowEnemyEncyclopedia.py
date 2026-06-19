# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
from typing import Any, Callable, Dict, Optional, Tuple, Union
from Engine import Input, IntRect, Pair, Text, Texture, UI, Vector2f
from Engine.UI.FunctionalUI import FImage, FPlainText
from Global import Manager
from .Base import WindowBase
from ..System import System as GameSystem


_WINDOW_WIDTH = 640
_WINDOW_HEIGHT = 480
_PORTRAIT_AREA_HEIGHT = 160
_NAME_TEXT_SIZE = 20
_NAME_TOP_MARGIN = 8
_INFO_TEXT_SIZE = 16
_INFO_TOP_MARGIN = 8
_INFO_COLUMN_GAP = 203
_INFO_VALUE_OFFSET = 176
_INFO_ROW_GAP = 32
_INFO_LABEL_WIDTH = 96
_INFO_VALUE_WIDTH = 104
_SPECIAL_TOP_MARGIN = 16
_SPECIAL_NAME_X = 0
_SPECIAL_DESC_X = 64
_SPECIAL_NAME_WIDTH = 60


class WindowEnemyEncyclopedia(WindowBase):
    r"""\brief Enemy encyclopedia detail window with an animated portrait."""

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        onClose: Optional[Callable[[], None]] = None,
    ) -> None:
        r"""\brief Construct the enemy encyclopedia window.

        - \param rect Window rectangle.
        - \param onClose Optional callback invoked when the window closes.
        """
        super().__init__(rect)
        self._onCloseCallback = onClose
        self._portrait: Optional[FImage] = None
        self._nameText: Optional[FPlainText] = None
        self._infoTexts: list[FPlainText] = []
        self._texture: Optional[Texture] = None
        self._rect: Optional[IntRect] = None
        self._animatable = False
        self._switchInterval = 0.2
        self._switchTimer = 0.0
        self.setActive(False)
        self.setVisible(False)

    def open(self, entry: Dict[str, Any]) -> None:
        r"""\brief Open the detail window for an enemy-book entry.

        - \param entry Prepared enemy display data.
        """
        self._clearEnemyControls()
        self._texture = entry.get("texture")
        self._rect = copy.copy(entry.get("rect"))
        self._animatable = bool(entry.get("animatable", False))
        self._switchInterval = float(entry.get("switchInterval", 0.2))
        self._switchTimer = 0.0
        self._buildPortrait(entry)
        nameBottom = self._buildName(str(entry.get("name", "")))
        infoY = nameBottom + _INFO_TOP_MARGIN
        self._buildInfo(entry, infoY)
        self._buildSpecials(entry, infoY + 3 * _INFO_ROW_GAP + _SPECIAL_TOP_MARGIN)
        self.setVisible(True)
        self.setActive(True)

    def close(self) -> None:
        r"""\brief Close the enemy encyclopedia window."""
        self.setVisible(False)
        self.setActive(False)
        if self._onCloseCallback is not None:
            self._onCloseCallback()

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Close on confirm or cancel.

        - \param kwargs Event data.
        """
        if Input.isActionTriggered(Input.getConfirmKeys(), handled=True):
            self._closeByInput()
            return
        if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
            self._closeByInput()

    def onMouseButtonDown(self, kwargs: Dict[str, Any]) -> bool:
        r"""\brief Close on right click."""
        if kwargs["button"] == Input.Mouse.Button.Right:
            Input.getMouseButtonPressed(Input.Mouse.Button.Right, handled=True)
            Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True)
            self._closeByInput()
            return True
        return False

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update the animated portrait.

        - \param deltaTime Elapsed time in seconds.
        """
        self._animatePortrait(deltaTime)
        super().onTick(deltaTime)

    @staticmethod
    def getDefaultSize() -> Tuple[int, int]:
        r"""\brief Get the default encyclopedia window size.

        - \return Width and height.
        """
        return (_WINDOW_WIDTH, _WINDOW_HEIGHT)

    def _buildPortrait(self, entry: Dict[str, Any]) -> None:
        if self._texture is None or self._rect is None:
            return
        self._portrait = FImage(self._texture, self._rect)
        contentSize = self.content.getSize()
        scale = entry.get("scale", Vector2f(1.0, 1.0))
        displayScale = Vector2f(max(0.01, abs(scale.x)), max(0.01, abs(scale.y)))
        portraitW = max(1.0, float(self._rect.size.x) * displayScale.x)
        portraitH = max(1.0, float(self._rect.size.y) * displayScale.y)
        fit = min(1.0, float(contentSize.x) / portraitW, float(_PORTRAIT_AREA_HEIGHT) / portraitH)
        displayScale = Vector2f(displayScale.x * fit, displayScale.y * fit)
        self._portrait.setScale(displayScale)
        portraitW = float(self._rect.size.x) * displayScale.x
        portraitH = float(self._rect.size.y) * displayScale.y
        self._portrait.setPosition(Vector2f((float(contentSize.x) - portraitW) / 2.0, 0.0))
        self.content.addChild(self._portrait)

    def _buildName(self, name: str) -> float:
        contentSize = self.content.getSize()
        fittedName = self._fitText(name, int(contentSize.x), _NAME_TEXT_SIZE)
        self._nameText = FPlainText(UI.DefaultFont, fittedName, _NAME_TEXT_SIZE)
        textWidth = self._measureText(fittedName, _NAME_TEXT_SIZE)
        y = _NAME_TOP_MARGIN
        if self._portrait is not None and self._rect is not None:
            portraitScale = self._portrait.getScale()
            y = float(self._rect.size.y) * portraitScale.y + _NAME_TOP_MARGIN
        self._nameText.setPosition(Vector2f((float(contentSize.x) - textWidth) / 2.0, y))
        self.content.addChild(self._nameText)
        return y + float(_NAME_TEXT_SIZE)

    def _buildInfo(self, entry: Dict[str, Any], y: float) -> None:
        rows = [
            [
                (LOC("STAT_HP"), entry.get("MAXHP", 0)),
                (LOC("STAT_ATK"), entry.get("ATK", 0)),
                (LOC("STAT_DEF"), entry.get("DEF", 0)),
            ],
            [
                (LOC("STAT_EXP"), entry.get("EXP", 0)),
                (LOC("STAT_GOLD"), entry.get("GOLD", 0)),
                (LOC("STAT_DMG"), entry.get("damage", "???")),
            ],
        ]
        for rowIndex, row in enumerate(rows):
            for colIndex, (label, value) in enumerate(row):
                self._addInfoPair(label, str(value), colIndex, y + rowIndex * _INFO_ROW_GAP)
        criticalText = self._formatCriticalText(entry.get("critical", -2))
        hitCount = self._formatHitCount(entry.get("hitCount", None))
        criticalColIndex = 0
        if hitCount:
            self._addInfoPair(LOC("STAT_HIT"), hitCount, 0, y + 2 * _INFO_ROW_GAP)
            criticalColIndex = 1
        if criticalText:
            self._addInfoPair(LOC("STAT_CRIT"), criticalText, criticalColIndex, y + 2 * _INFO_ROW_GAP)

    def _addInfoPair(self, label: str, value: str, colIndex: int, y: float) -> None:
        x = float(colIndex * _INFO_COLUMN_GAP)
        labelText = self._addInfoText(label, _INFO_LABEL_WIDTH, x, y)
        labelText.setLineAlignment(Text.LineAlignment.Left)
        valueText = self._addInfoText(value, _INFO_VALUE_WIDTH, x + _INFO_VALUE_OFFSET, y)
        valueText.setLineAlignment(Text.LineAlignment.Right)
        self._infoTexts.extend([labelText, valueText])

    def _addInfoText(self, text: str, maxWidth: int, x: float, y: float) -> FPlainText:
        fittedText = self._fitText(text, maxWidth, _INFO_TEXT_SIZE)
        infoText = FPlainText(UI.DefaultFont, fittedText, _INFO_TEXT_SIZE)
        infoText.setPosition(Vector2f(x, y))
        self.content.addChild(infoText)
        return infoText

    def _buildSpecials(self, entry: Dict[str, Any], y: float) -> None:
        specialDetails = entry.get("specialDetails", [])
        if not isinstance(specialDetails, list):
            return
        currentY = y
        descWidth = max(1, int(self.content.getSize().x) - _SPECIAL_DESC_X)
        for special in specialDetails:
            if not isinstance(special, dict):
                continue
            nameText = self._addSpecialText(
                str(special.get("name", "")),
                _SPECIAL_NAME_WIDTH,
                float(_SPECIAL_NAME_X),
                currentY,
            )
            desc = self._wrapText(str(special.get("desc", "")), descWidth, _INFO_TEXT_SIZE)
            descText = self._addSpecialText(desc, descWidth, float(_SPECIAL_DESC_X), currentY)
            self._infoTexts.extend([nameText, descText])
            lineCount = max(1, len(desc.split("\n")))
            currentY += max(_INFO_ROW_GAP, lineCount * _INFO_ROW_GAP)

    def _addSpecialText(self, text: str, maxWidth: int, x: float, y: float) -> FPlainText:
        displayText = text if "\n" in text else self._fitText(text, maxWidth, _INFO_TEXT_SIZE)
        specialText = FPlainText(UI.DefaultFont, displayText, _INFO_TEXT_SIZE)
        specialText.setLineAlignment(Text.LineAlignment.Left)
        specialText.setPosition(Vector2f(x, y))
        self.content.addChild(specialText)
        return specialText

    def _formatCriticalText(self, criticalValue: Any) -> str:
        try:
            value = int(criticalValue)
        except (TypeError, ValueError):
            value = -2
        if value == -2:
            return ""
        if value == -1:
            return "???"
        return str(value)

    def _formatHitCount(self, hitCount: Any) -> str:
        if hitCount is None:
            return ""
        try:
            value = int(hitCount)
        except (TypeError, ValueError):
            return ""
        return str(max(1, value))

    def _fitText(self, text: str, maxWidth: int, textSize: int) -> str:
        if not text:
            return ""
        result = text
        while result and self._measureText(result, textSize) > maxWidth:
            result = result[:-1]
        if result != text and len(result) > 1:
            result = result[:-1] + "."
        return result

    def _wrapText(self, text: str, maxWidth: int, textSize: int) -> str:
        if not text:
            return ""

        def wrapPara(para: str) -> list[str]:
            lines: list[str] = []
            line = ""
            lineWidth = 0.0
            for word in para.split(" "):
                wordWidth = self._measureText(word, textSize) if word else 0.0
                sepWidth = self._measureText(" ", textSize) if line else 0.0
                if lineWidth + sepWidth + wordWidth <= maxWidth:
                    line += (" " if line else "") + word
                    lineWidth += sepWidth + wordWidth
                    continue
                if line:
                    lines.append(line)
                    line = ""
                    lineWidth = 0.0
                if wordWidth <= maxWidth:
                    line = word
                    lineWidth = wordWidth
                    continue
                for ch in word:
                    chWidth = self._measureText(ch, textSize)
                    if line and lineWidth + chWidth > maxWidth:
                        lines.append(line)
                        line = ""
                        lineWidth = 0.0
                    line += ch
                    lineWidth += chWidth
            lines.append(line)
            return lines

        return "\n".join(line for para in text.split("\n") for line in wrapPara(para))

    def _measureText(self, text: str, textSize: int) -> float:
        from Engine import Scale

        charSize = int(textSize * Scale)
        return sum(UI.DefaultFont.getGlyph(ch, charSize, False).advance for ch in text) / Scale

    def _animatePortrait(self, deltaTime: float) -> None:
        if not self._animatable or self._portrait is None or self._texture is None or self._rect is None:
            return
        self._switchTimer += deltaTime
        if self._switchTimer < self._switchInterval:
            return
        self._switchTimer = 0.0
        rect = copy.copy(self._rect)
        textureWidth = self._texture.getSize().x
        rect.position.x = (rect.position.x + rect.size.x) % textureWidth
        self._rect = rect
        self._portrait.setTextureRect(rect)

    def _clearEnemyControls(self) -> None:
        for child in [self._portrait, self._nameText, *self._infoTexts]:
            if child is not None and child.getParent() is self.content:
                self.content.removeChild(child)
        self._portrait = None
        self._nameText = None
        self._infoTexts = []

    def _closeByInput(self) -> None:
        Manager.playSE(GameSystem.getCancelSE())
        self.close()
