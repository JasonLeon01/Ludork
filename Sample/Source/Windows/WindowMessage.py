# -*- encoding: utf-8 -*-

from __future__ import annotations
from enum import IntEnum
from typing import Dict, Any, Optional
from Engine import Color, Input, UI, Text, Vector2f, Vector2u, GetCellSize
from Engine.UI import RichText, TextStyle, PlainText
from Engine.Utils import Math
from Global import System as GlobalSystem
from .Base import WindowBase
from ..System import System


class FadePhase(IntEnum):
    NOTHING = 0
    IN = 1
    OUT = 2


class WindowMessage(WindowBase):
    _WINDOW_PADDING = 16
    _NAME_TEXT_SIZE = 28
    _NAME_MESSAGE_GAP = 8

    def __init__(self) -> None:
        super().__init__(((48, 288), (544, 160)))
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

        self._inDialogue: bool = False
        self._fadePhase: FadePhase = FadePhase.NOTHING
        self._fadeInSpeed = 1000.0
        self._fadeOutSpeed = 1000.0

    def onTick(self, deltaTime: float) -> None:
        if self._fadePhase == FadePhase.IN:
            self._fadeIn(deltaTime)
        if self._fadePhase == FadePhase.OUT:
            self._fadeOut(deltaTime)
        return super().onTick(deltaTime)

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        if self.getVisible():
            if Input.isActionTriggered(Input.getConfirmKeys(), handled=True):
                self._inDialogue = False
                self._fadePhase = FadePhase.OUT
        return super().onKeyDown(kwargs)

    def isInDialogue(self) -> bool:
        return self._inDialogue

    def setMessage(self, refPosition: Optional[Vector2f], name: str, message: str) -> None:
        if name == self._name and message == self._message:
            return
        self._text.setColor(Color(255, 255, 255, 0))
        self.setVisible(True)
        self._inDialogue = True
        self._fadePhase = FadePhase.IN
        self._name = name
        self._message = message
        self._nameText.setString(name)
        self._nameText.setVisible(bool(name.strip()))
        self._text.setString(message)
        self._updateLayoutByTextSize()
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
            cellSize = float(GetCellSize())
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
        for comp in [self, self._text]:
            a = comp.getColor().a
            if a == 255:
                continue
            deltaAlpha = self._fadeInSpeed * deltaTime
            a = int(min(a + deltaAlpha, 255))
            comp.setColor(Color(255, 255, 255, a))

    def _fadeOut(self, deltaTime: float) -> None:
        a = self.getColor().a
        if a == 0:
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
