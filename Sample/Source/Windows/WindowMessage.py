# -*- encoding: utf-8 -*-

from __future__ import annotations
from enum import IntEnum
from typing import Dict, Any, Optional, List, Union, Tuple, Callable
import Engine
from Engine import Color, Input, UI, Text, Vector2f, Vector2u, Vector2i, IntRect
from Engine.UI import RichText, TextStyle, PlainText, ListView
from Engine.UI.FunctionalUI import FPlainText
from Engine.UI.Base import FunctionalBase
from Engine.Utils import Math
from Global import Manager, System as GlobalSystem
from .Base import WindowSelectable, WindowBase
from ..System import System


class FadePhase(IntEnum):
    NOTHING = 0
    IN = 1
    OUT = 2


class ContentMode(IntEnum):
    MESSAGE = 0
    SELECTION = 1


class WindowMessage(WindowSelectable):
    r"""\brief Dialogue message window with typewriter effect and selection branches.

    Supports fade-in/out, speaker name display, multi-option selection,
    and automatic positioning relative to reference actors.
    """

    _WINDOW_PADDING = 16
    _SCREEN_EDGE_MARGIN = 64
    _NAME_TEXT_SIZE = 28
    _NAME_MESSAGE_GAP = 8
    _MESSAGE_TEXT_SIZE = 22
    _OPTION_TEXT_SIZE = 22
    _OPTION_ITEM_HEIGHT = 32
    _SELECTION_LIST_HORIZONTAL_INSET = 32
    _MAX_OPTIONS = 4

    def __init__(self) -> None:
        r"""\brief Construct a message window with default fade and layout settings."""
        self._inDialogue: bool = False
        self._contentMode: ContentMode = ContentMode.MESSAGE
        self._selectionListView: Optional[ListView] = None
        self._messageListView: Optional[ListView] = None
        self._messageAdvancer: Optional[FPlainText] = None
        self._selectionResult: Optional[int] = None
        self._allowCancel: bool = True
        self._onFinished: Optional[Callable[[], None]] = None
        self._fadePhase: FadePhase = FadePhase.NOTHING
        self._fadeInSpeed = 1000.0
        self._fadeOutSpeed = 1000.0
        self._pendingLayout: bool = False
        self._pendingRefPosition: Optional[Vector2f] = None

        super().__init__(((48, 288), (544, 160)), None, None, self._OPTION_ITEM_HEIGHT)
        self.setColour(Color(255, 255, 255, 0))
        self._window.setColour(Color(255, 255, 255, 192))
        self._name = ""
        self._message = ""
        self._textStyles: Dict[str, TextStyle] = {}
        self._initTextStyles()
        self._nameText = PlainText(System.getFonts()[0], "", self._NAME_TEXT_SIZE)
        self._nameText.setVisible(False)
        self.content.addChild(self._nameText)
        self._text = RichText(System.getFonts()[0], self._message, self._textStyles)
        self.content.addChild(self._text)
        self._setupMessageAdvancer()
        self.setVisible(False)

    def _setupMessageAdvancer(self) -> None:
        self._messageAdvancer = FPlainText(System.getFonts()[0], "", self._MESSAGE_TEXT_SIZE)
        self._messageAdvancer.setVisible(False)

        def onConfirm(_itemSelf, _kwargs) -> None:
            self._resolveSelection(0)

        self._messageAdvancer.addConfirmCallback(onConfirm)
        self._messageListView = ListView(
            IntRect(Vector2i(0, 0), Vector2i(1, 1)), self._OPTION_ITEM_HEIGHT, True, 1
        )
        self._messageListView.addChild(self._messageAdvancer)

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update fade animations, layout, and selection cursor visibility.

        - \param deltaTime Elapsed time in seconds.
        """
        if self._fadePhase == FadePhase.IN:
            self._fadeIn(deltaTime)
        if self._fadePhase == FadePhase.OUT:
            self._fadeOut(deltaTime)
        if self._pendingLayout:
            self._pendingLayout = False
            if self._contentMode == ContentMode.SELECTION:
                self._updateLayoutBySelectionSize()
            else:
                self._updateLayoutByTextSize()
            self._updateWindowPosition(self._pendingRefPosition)
        if self._contentMode != ContentMode.SELECTION:
            if hasattr(self, "_rect"):
                self._rect.setVisible(False)
                if self._rect.getParent() is not None:
                    self.content.removeChild(self._rect)
            return super(WindowSelectable, self).onTick(deltaTime)
        super().onTick(deltaTime)

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Handle keyboard input for selection cancel and option navigation.

        - \param kwargs Event data.
        """
        if (
            self._contentMode == ContentMode.SELECTION
            and self._allowCancel
            and self._selectionListView is not None
            and self.index is not None
            and Input.isActionTriggered(Input.getCancelKeys(), handled=True)
        ):
            children = self._selectionListView.getChildren()
            if 0 <= self.index < len(children):
                child = children[self.index]
                if isinstance(child, FunctionalBase):
                    child.onCancel({})
            return
        return super().onKeyDown(kwargs)

    def onClick(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Advance a plain message dialogue on left mouse click anywhere within the window.

        - \param kwargs Event data with cursor position.
        """
        if not self.getVisible():
            return super().onClick(kwargs)
        if self._contentMode != ContentMode.MESSAGE:
            return super().onClick(kwargs)
        if self._fadePhase == FadePhase.OUT:
            return super().onClick(kwargs)
        if self._messageAdvancer is not None:
            self._messageAdvancer.onConfirm({})
        return super().onClick(kwargs)

    def isInDialogue(self) -> bool:
        r"""\brief Check if the window is currently showing a dialogue.

        - \return True if in dialogue mode.
        """
        return self._inDialogue

    def getSelectionResult(self) -> Optional[int]:
        r"""\brief Get the result of a selection dialogue.

        - \return The selected option index, or None if no selection has been made.
        """
        return self._selectionResult

    def setMessage(
        self,
        refPosition: Optional[Vector2f],
        name: str,
        message: Union[str, List[str], Tuple[str, ...]],
        allowCancel: bool = True,
        onFinished: Optional[Callable[[], None]] = None,
    ) -> None:
        r"""\brief Show a message or selection dialogue.

        - \param refPosition Optional reference position for window placement.
        - \param name Speaker name to display.
        - \param message Message text (str) or selection options (list).
        - \param allowCancel Whether the selection can be cancelled.
        - \param onFinished Optional callback invoked when the dialogue is confirmed/cancelled.
        """
        self.hidePauseMark()
        self.setColour(Color(255, 255, 255, 0))
        self.setVisible(True)
        self._inDialogue = True
        self._selectionResult = None
        self._allowCancel = allowCancel
        self._onFinished = onFinished
        self._fadePhase = FadePhase.IN
        self._name = name
        self._nameText.setString(name)
        self._nameText.setVisible(bool(name.strip()))
        self._nameText.setColour(Color(255, 255, 255, 0))
        if isinstance(message, (list, tuple)):
            self._contentMode = ContentMode.SELECTION
            self._message = ""
            self._text.setVisible(False)
            self._setupSelectionList([str(item) for item in message])
            if self._selectionListView is not None:
                for child in self._selectionListView.getChildren():
                    child.setColour(Color(255, 255, 255, 0))
        else:
            self._contentMode = ContentMode.MESSAGE
            self._message = message
            self.setListView(self._messageListView)
            self.index = 0
            self._text.setVisible(True)
            self._text.setColour(Color(255, 255, 255, 0))
            self._text.setString(message)
        self._pendingLayout = True
        self._pendingRefPosition = refPosition

    def _resolveSelection(self, selectionResult: int) -> None:
        if self._selectionResult is not None:
            return
        self.hidePauseMark()
        self._selectionResult = selectionResult
        self._inDialogue = False
        self._fadePhase = FadePhase.OUT
        if self._onFinished is not None:
            onFinished = self._onFinished
            self._onFinished = None
            onFinished()

    def _setupSelectionList(self, options: List[str]) -> None:
        options = options[: self._MAX_OPTIONS]
        if self._selectionListView is None:
            self._selectionListView = ListView(
                IntRect(Vector2i(0, 0), Vector2i(1, 1)), self._OPTION_ITEM_HEIGHT, True, 1
            )
        self._selectionListView.clearChildren()

        for optionIndex, optionText in enumerate(options):
            item = FPlainText(System.getFonts()[0], optionText, self._OPTION_TEXT_SIZE)

            def onConfirm(_itemSelf, _kwargs, optionIndex=optionIndex) -> None:
                Manager.playSE("decision1.ogg")
                self._resolveSelection(optionIndex)

            def onCancel(_itemSelf, _kwargs) -> None:
                Manager.playSE("cancel.ogg")
                self._resolveSelection(-1)

            item.addConfirmCallback(onConfirm)
            item.addCancelCallback(onCancel)
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
        self._textStyles["DimGrey"] = TextStyle(fillColor=UI.GetDimGrey())
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
            a = comp.getColour().a
            if a == 255:
                continue
            deltaAlpha = self._fadeInSpeed * deltaTime
            a = int(min(a + deltaAlpha, 255))
            comp.setColour(Color(255, 255, 255, a))
        if all(comp.getColour().a == 255 for comp in fadeTargets):
            self._fadePhase = FadePhase.NOTHING
            self._onFadeInComplete()

    def _onFadeInComplete(self) -> None:
        if self._contentMode == ContentMode.MESSAGE:
            self.refreshPauseMarkLayout()
            self.showPauseMark()

    def _fadeOut(self, deltaTime: float) -> None:
        a = self.getColour().a
        if a == 0:
            self._fadePhase = FadePhase.NOTHING
            self._inDialogue = False
            self.setVisible(False)
            return
        deltaAlpha = self._fadeOutSpeed * deltaTime
        a = int(max(a - deltaAlpha, 0))
        self.setColour(Color(255, 255, 255, a))
        if a == 0:
            self._fadePhase = FadePhase.NOTHING
            self._inDialogue = False
            self.setVisible(False)

    def _shouldAutoFitWidth(self) -> bool:
        return not self._nameText.getVisible()

    def _getMaxWindowWidth(self) -> int:
        gameWidth = int(GlobalSystem.getGameSize().x)
        return max(1, gameWidth - self._SCREEN_EDGE_MARGIN)

    def _measurePlainTextWidth(self, text: str, characterSize: int) -> int:
        from Engine import Scale

        font = System.getFonts()[0]
        charSize = int(characterSize * Scale)
        if not text:
            return 1
        pixelWidth = sum(font.getGlyph(ch, charSize, False).advance for ch in text)
        return max(1, int(round(pixelWidth / Scale)))

    def _wrapMessage(self, text: str, maxWidth: float) -> str:
        from Engine import Scale

        font = System.getFonts()[0]
        charSize = int(self._MESSAGE_TEXT_SIZE * Scale)
        maxW = maxWidth * Scale

        def adv(ch: str) -> float:
            return font.getGlyph(ch, charSize, False).advance

        def wrap_para(para: str) -> str:
            lines: List[str] = []
            line = ""
            line_w = 0.0
            for word in para.split(" "):
                word_w = sum(adv(ch) for ch in word) if word else 0.0
                sep_w = adv(" ") if line else 0.0
                if line_w + sep_w + word_w <= maxW:
                    line += (" " if line else "") + word
                    line_w += sep_w + word_w
                else:
                    if line:
                        lines.append(line)
                        line = ""
                        line_w = 0.0
                    if word_w <= maxW:
                        line = word
                        line_w = word_w
                    else:
                        for ch in word:
                            ch_w = adv(ch)
                            if line and line_w + ch_w > maxW:
                                lines.append(line)
                                line = ""
                                line_w = 0.0
                            line += ch
                            line_w += ch_w
            lines.append(line)
            return "\n".join(lines)

        return "\n".join(wrap_para(p) for p in text.split("\n"))

    def _updateLayoutByTextSize(self) -> None:
        nameBounds = self._nameText.getLocalBounds()
        hasName = self._nameText.getVisible()
        nameWidth = 0
        nameHeight = 0
        if hasName:
            nameWidth = max(1, int(nameBounds.size.x + nameBounds.position.x))
            nameHeight = max(1, int(nameBounds.size.y + nameBounds.position.y))

        displayMessage = self._message
        maxContentWidth: Optional[int] = None
        if self._shouldAutoFitWidth():
            maxContentWidth = max(32, self._getMaxWindowWidth() - self._WINDOW_PADDING * 2)

        self._text.setString(displayMessage)
        textBounds = self._text.getLocalBounds()
        textWidth = max(1, int(textBounds.size.x + textBounds.position.x))
        if maxContentWidth is not None and textWidth > maxContentWidth:
            displayMessage = self._wrapMessage(displayMessage, float(maxContentWidth))
            self._text.setString(displayMessage)
            textBounds = self._text.getLocalBounds()
            textWidth = max(1, int(textBounds.size.x + textBounds.position.x))

        textHeight = max(1, int(textBounds.size.y + textBounds.position.y))
        contentWidth = max(textWidth, nameWidth, WindowBase._PAUSE_MARK_SIZE)
        if maxContentWidth is not None:
            contentWidth = min(contentWidth, maxContentWidth)
        contentHeight = textHeight + WindowBase._PAUSE_MARK_SIZE
        if hasName:
            contentHeight += nameHeight + self._NAME_MESSAGE_GAP
        totalWidth = contentWidth + self._WINDOW_PADDING * 2
        if maxContentWidth is not None:
            totalWidth = min(totalWidth, self._getMaxWindowWidth())
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
        self.refreshPauseMarkLayout()

    def _updateLayoutBySelectionSize(self) -> None:
        nameBounds = self._nameText.getLocalBounds()
        hasName = self._nameText.getVisible()
        nameWidth = 0
        nameHeight = 0
        if hasName:
            nameWidth = max(1, int(nameBounds.size.x + nameBounds.position.x))
            nameHeight = max(1, int(nameBounds.size.y + nameBounds.position.y))

        maxOptionTextWidth = 1
        optionCount = 0
        if self._selectionListView is not None:
            optionCount = len(self._selectionListView.getChildren())
            for child in self._selectionListView.getChildren():
                optionText = child.getString() if isinstance(child, (PlainText, RichText)) else ""
                maxOptionTextWidth = max(
                    maxOptionTextWidth,
                    self._measurePlainTextWidth(optionText, self._OPTION_TEXT_SIZE),
                )

        contentWidth = max(
            32,
            nameWidth,
            maxOptionTextWidth + self._SELECTION_LIST_HORIZONTAL_INSET,
        )
        if self._shouldAutoFitWidth():
            maxContentWidth = max(32, self._getMaxWindowWidth() - self._WINDOW_PADDING * 2)
            contentWidth = min(contentWidth, maxContentWidth)
        contentHeight = optionCount * self._OPTION_ITEM_HEIGHT
        if hasName:
            contentHeight += nameHeight + self._NAME_MESSAGE_GAP
        totalWidth = contentWidth + self._WINDOW_PADDING * 2
        if self._shouldAutoFitWidth():
            totalWidth = min(totalWidth, self._getMaxWindowWidth())
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
        self.refreshPauseMarkLayout()

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
        target._size = Vector2u(width, height)
        target._canvas.resize(Math.ToVector2u(Vector2f(width, height) * Engine.Scale))
        target.setTexture(target._canvas.getTexture(), True)
        target.setView(target.getDefaultView())

    def _resizeWindow(self, width: int, height: int) -> None:
        self._window._size = Vector2u(width, height)
        self._window._canvas.resize(Math.ToVector2u(Vector2f(width, height) * Engine.Scale))
        self._window._initUI()
        self._window.setTexture(self._window._canvas.getTexture(), True)
