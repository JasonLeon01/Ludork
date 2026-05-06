# -*- encoding: utf-8 -*-
"""WindowMessage: dialogue message window with typewriter effect and selection branches."""

from __future__ import annotations
from enum import IntEnum
from typing import Dict, Any, Optional, List, Union, Tuple
import Engine
from Engine import Color, Input, UI, Text, Vector2f, Vector2u, Vector2i, IntRect
from Engine.UI import RichText, TextStyle, PlainText, ListView
from Engine.UI.FunctionalUI import FPlainText
from Engine.Utils import Math
from Global import System as GlobalSystem
from .Base import WindowSelectable
from ..System import System


class FadePhase(IntEnum):
    NOTHING = 0
    IN = 1
    OUT = 2


class ContentMode(IntEnum):
    MESSAGE = 0
    SELECTION = 1


class WindowMessage(WindowSelectable):
    _WINDOW_PADDING = 16
    _NAME_TEXT_SIZE = 28
    _NAME_MESSAGE_GAP = 8
    _OPTION_TEXT_SIZE = 22
    _OPTION_ITEM_HEIGHT = 32
    _MAX_OPTIONS = 4

    def __init__(self) -> None:
        self._inDialogue: bool = False
        self._contentMode: ContentMode = ContentMode.MESSAGE
        self._selectionListView: Optional[ListView] = None
        self._selectionResult: Optional[int] = None
        self._allowCancel: bool = True
        self._fadePhase: FadePhase = FadePhase.NOTHING
        self._fadeInSpeed = 1000.0
        self._fadeOutSpeed = 1000.0

        super().__init__(((48, 288), (544, 160)), None, None, self._OPTION_ITEM_HEIGHT)
        self.setColor(Color(255, 255, 255, 0))
        self._window.setColor(Color(255, 255, 255, 192))
        self._name = ""
        self._message = ""
        self._textStyles: Dict[str, TextStyle] = {}
        self._initTextStyles()
        self._nameText = PlainText(System.getFonts()[0], "", self._NAME_TEXT_SIZE)
        self._nameText.setVisible(False)
        self.content.addChild(self._nameText)
        self._text = RichText(System.getFonts()[0], self._message, self._textStyles)
        self.content.addChild(self._text)
        self.setVisible(False)

    def onTick(self, deltaTime: float) -> None:
        if self._fadePhase == FadePhase.IN:
            self._fadeIn(deltaTime)
        if self._fadePhase == FadePhase.OUT:
            self._fadeOut(deltaTime)
        return super().onTick(deltaTime)

    def update(self, deltaTime: float) -> None:
        if self._contentMode != ContentMode.SELECTION:
            self.index = None
            if hasattr(self, "_rect"):
                self._rect.setVisible(False)
                if self._rect.getParent() is not None:
                    self.content.removeChild(self._rect)
            return super(WindowSelectable, self).update(deltaTime)
        return super().update(deltaTime)

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        if self.getVisible():
            if self._contentMode == ContentMode.SELECTION:
                if self._allowCancel and Input.isActionTriggered(Input.getCancelKeys(), handled=True):
                    self._resolveSelection(-1)
                    return
                super().onKeyDown(kwargs)
                return
            if Input.isActionTriggered(Input.getConfirmKeys(), handled=True):
                self._resolveSelection(0)
        return super().onKeyDown(kwargs)

    def isInDialogue(self) -> bool:
        return self._inDialogue

    def getSelectionResult(self) -> Optional[int]:
        return self._selectionResult

    def setMessage(
        self,
        refPosition: Optional[Vector2f],
        name: str,
        message: Union[str, List[str], Tuple[str, ...]],
        allowCancel: bool = True,
    ) -> None:
        self.setColor(Color(255, 255, 255, 0))
        self.setVisible(True)
        self._inDialogue = True
        self._selectionResult = None
        self._allowCancel = allowCancel
        self._fadePhase = FadePhase.IN
        self._name = name
        self._nameText.setString(name)
        self._nameText.setVisible(bool(name.strip()))
        self._nameText.setColor(Color(255, 255, 255, 0))
        if isinstance(message, (list, tuple)):
            self._contentMode = ContentMode.SELECTION
            self._message = ""
            self._text.setVisible(False)
            self._setupSelectionList([str(item) for item in message])
            if self._selectionListView is not None:
                for child in self._selectionListView.getChildren():
                    child.setColor(Color(255, 255, 255, 0))
            self._updateLayoutBySelectionSize()
        else:
            self._contentMode = ContentMode.MESSAGE
            self._message = message
            self.index = None
            self.setListView(None)
            self._text.setVisible(True)
            self._text.setColor(Color(255, 255, 255, 0))
            self._text.setString(message)
            self._updateLayoutByTextSize()
        self._updateWindowPosition(refPosition)

    def _resolveSelection(self, selectionResult: int) -> None:
        if self._selectionResult is not None:
            return
        self._selectionResult = selectionResult
        self._inDialogue = False
        self._fadePhase = FadePhase.OUT

    def _setupSelectionList(self, options: List[str]) -> None:
        options = options[: self._MAX_OPTIONS]
        if self._selectionListView is None:
            self._selectionListView = ListView(IntRect(Vector2i(0, 0), Vector2i(1, 1)), self._OPTION_ITEM_HEIGHT, True, 1)
        self._selectionListView.clearChildren()

        for optionIndex, optionText in enumerate(options):
            item = FPlainText(System.getFonts()[0], optionText, self._OPTION_TEXT_SIZE)

            def onConfirm(_itemSelf, _kwargs, optionIndex=optionIndex) -> None:
                self._resolveSelection(optionIndex)

            item.addConfirmCallback(onConfirm)
            self._applyItem(item)
            self._selectionListView.addChild(item)

        self.setListView(self._selectionListView)
        self.index = 0 if len(options) > 0 else None

    def _updateWindowPosition(self, refPosition: Optional[Vector2f]) -> None:
        gameSize = GlobalSystem.getGameSize()
        gameWidth = float(gameSize.x)
        gameHeight = float(gameSize.y)
        windowSize = self.getSize()
        windowWidth = float(windowSize.x)
        windowHeight = float(windowSize.y)
        if refPosition is None:
            posX = (gameWidth - windowWidth) / 2.0
            posY = (gameHeight - windowHeight) / 2.0
            self.setPosition(Vector2f(posX, posY))
        else:
            cellSize = float(Engine.CellSize)
            anchorX = refPosition.x + cellSize * 0.5
            halfScreenY = gameHeight * 0.5
            if refPosition.y < halfScreenY:
                anchorY = refPosition.y + cellSize
                posY = anchorY
            else:
                anchorY = refPosition.y
                posY = anchorY - windowHeight
            posX = anchorX - windowWidth * 0.5
            maxX = max(0.0, gameWidth - windowWidth)
            maxY = max(0.0, gameHeight - windowHeight)
            posX = Math.Clamp(posX, 0.0, maxX)
            posY = Math.Clamp(posY, 0.0, maxY)
            self.setPosition(Vector2f(posX, posY))

    def _initTextStyles(self) -> None:
        self._textStyles["default"] = TextStyle(22, Text.Style.Regular, Color.White, Color.Transparent, 0.0)
        self._textStyles["RosyBrown"] = TextStyle(fillColor=UI.GetRosyBrown())
        self._textStyles["Copper"] = TextStyle(fillColor=UI.GetCopper())
        self._textStyles["Sage"] = TextStyle(fillColor=UI.GetSage())
        self._textStyles["Teal"] = TextStyle(fillColor=UI.GetTeal())
        self._textStyles["MutedPurple"] = TextStyle(fillColor=UI.GetMutedPurple())
        self._textStyles["Taupe"] = TextStyle(fillColor=UI.GetTaupe())
        self._textStyles["TerraCotta"] = TextStyle(fillColor=UI.GetTerraCotta())
        self._textStyles["Ochre"] = TextStyle(fillColor=UI.GetOchre())
        self._textStyles["FernGreen"] = TextStyle(fillColor=UI.GetFernGreen())
        self._textStyles["SteelBlue"] = TextStyle(fillColor=UI.GetSteelBlue())
        self._textStyles["DimGray"] = TextStyle(fillColor=UI.GetDimGray())
        self._textStyles["Charcoal"] = TextStyle(fillColor=UI.GetCharcoal())
        self._textStyles["Black"] = TextStyle(fillColor=Color.Black)
        self._textStyles["Blue"] = TextStyle(fillColor=Color.Blue)
        self._textStyles["Cyan"] = TextStyle(fillColor=Color.Cyan)
        self._textStyles["Green"] = TextStyle(fillColor=Color.Green)
        self._textStyles["Magenta"] = TextStyle(fillColor=Color.Magenta)
        self._textStyles["Red"] = TextStyle(fillColor=Color.Red)
        self._textStyles["White"] = TextStyle(fillColor=Color.White)
        self._textStyles["Yellow"] = TextStyle(fillColor=Color.Yellow)

    def _fadeIn(self, deltaTime: float) -> None:
        fadeTargets = [self, self._nameText]
        if self._contentMode == ContentMode.MESSAGE:
            fadeTargets.append(self._text)
        elif self._contentMode == ContentMode.SELECTION and self._selectionListView is not None:
            fadeTargets.extend(self._selectionListView.getChildren())
        for comp in fadeTargets:
            a = comp.getColor().a
            if a == 255:
                continue
            deltaAlpha = self._fadeInSpeed * deltaTime
            a = int(min(a + deltaAlpha, 255))
            comp.setColor(Color(255, 255, 255, a))

    def _fadeOut(self, deltaTime: float) -> None:
        a = self.getColor().a
        if a == 0:
            self._fadePhase = FadePhase.NOTHING
            self._inDialogue = False
            self.setVisible(False)
            return
        deltaAlpha = self._fadeOutSpeed * deltaTime
        a = int(max(a - deltaAlpha, 0))
        self.setColor(Color(255, 255, 255, a))
        if a == 0:
            self._fadePhase = FadePhase.NOTHING
            self._inDialogue = False
            self.setVisible(False)

    def _updateLayoutByTextSize(self) -> None:
        nameBounds = self._nameText.getLocalBounds()
        textBounds = self._text.getLocalBounds()
        textWidth = max(1, int(textBounds.size.x + textBounds.position.x))
        textHeight = max(1, int(textBounds.size.y + textBounds.position.y))
        hasName = self._nameText.getVisible()
        nameWidth = 0
        nameHeight = 0
        if hasName:
            nameWidth = max(1, int(nameBounds.size.x + nameBounds.position.x))
            nameHeight = max(1, int(nameBounds.size.y + nameBounds.position.y))
        contentWidth = max(textWidth, nameWidth)
        contentHeight = textHeight
        if hasName:
            contentHeight += nameHeight + self._NAME_MESSAGE_GAP
        totalWidth = contentWidth + self._WINDOW_PADDING * 2
        totalHeight = contentHeight + self._WINDOW_PADDING * 2
        self._resizeCanvas(self, totalWidth, totalHeight)
        self._resizeWindow(totalWidth, totalHeight)
        self._resizeCanvas(self.content, contentWidth, contentHeight)
        self.content.setPosition(Vector2f(self._WINDOW_PADDING, self._WINDOW_PADDING))
        textY = 0.0
        if hasName:
            nameX = (contentWidth - nameWidth) / 2.0
            self._nameText.setPosition(Vector2f(nameX, 0.0))
            textY = float(nameHeight + self._NAME_MESSAGE_GAP)
        self._text.setPosition(Vector2f(0.0, textY))

    def _updateLayoutBySelectionSize(self) -> None:
        nameBounds = self._nameText.getLocalBounds()
        hasName = self._nameText.getVisible()
        nameWidth = 0
        nameHeight = 0
        if hasName:
            nameWidth = max(1, int(nameBounds.size.x + nameBounds.position.x))
            nameHeight = max(1, int(nameBounds.size.y + nameBounds.position.y))

        optionWidth = 1
        optionCount = 0
        if self._selectionListView is not None:
            optionCount = len(self._selectionListView.getChildren())
            for child in self._selectionListView.getChildren():
                bounds = child.getLocalBounds()
                childWidth = max(1, int(bounds.size.x + bounds.position.x))
                optionWidth = max(optionWidth, childWidth)

        contentWidth = max(32, optionWidth, nameWidth)
        contentHeight = optionCount * self._OPTION_ITEM_HEIGHT
        if hasName:
            contentHeight += nameHeight + self._NAME_MESSAGE_GAP
        totalWidth = contentWidth + self._WINDOW_PADDING * 2
        totalHeight = contentHeight + self._WINDOW_PADDING * 2
        self._resizeCanvas(self, totalWidth, totalHeight)
        self._resizeWindow(totalWidth, totalHeight)
        self._resizeCanvas(self.content, contentWidth, contentHeight)
        self.content.setPosition(Vector2f(self._WINDOW_PADDING, self._WINDOW_PADDING))
        currentY = 0.0
        if hasName:
            nameX = (contentWidth - nameWidth) / 2.0
            self._nameText.setPosition(Vector2f(nameX, 0.0))
            currentY = float(nameHeight + self._NAME_MESSAGE_GAP)
        if self._selectionListView is not None:
            self._selectionListView.size = Vector2i(contentWidth, optionCount * self._OPTION_ITEM_HEIGHT)
            self._selectionListView.setPosition(Vector2f(0.0, currentY))

    def _getRectPosition(self) -> Optional[Vector2f]:
        if self.index is None:
            return None
        if self._contentMode != ContentMode.SELECTION or self._selectionListView is None:
            return super()._getRectPosition()
        columns = self._selectionListView.getColumns()
        if columns <= 0:
            return super()._getRectPosition()
        listViewX, listViewY = self._selectionListView.v_getPosition()
        colWidth = (float(self._selectionListView.size.x) - 32.0) / float(columns)
        col = self.index % columns
        row = self.index // columns
        x = float(listViewX) + 16.0 + float(col) * colWidth
        y = float(listViewY) + float(row) * float(self._rectHeight)
        return Vector2f(x, y)

    def _getRectWidth(self) -> int:
        if self._contentMode != ContentMode.SELECTION or self._selectionListView is None:
            return super()._getRectWidth()
        columns = self._selectionListView.getColumns()
        if columns <= 0:
            return super()._getRectWidth()
        return max(1, int(round((float(self._selectionListView.size.x) - 32.0) / float(columns))))

    def _resizeCanvas(self, target, width: int, height: int) -> None:
        from Engine import Scale

        target._size = Vector2u(width, height)
        target._canvas.resize(Math.ToVector2u(Vector2f(width, height) * Scale))
        target.setTexture(target._canvas.getTexture(), True)
        target.setView(target.getDefaultView())

    def _resizeWindow(self, width: int, height: int) -> None:
        from Engine import Scale

        self._window._size = Vector2u(width, height)
        self._window._canvas.resize(Math.ToVector2u(Vector2f(width, height) * Scale))
        self._window._initUI()
        self._window.setTexture(self._window._canvas.getTexture(), True)
