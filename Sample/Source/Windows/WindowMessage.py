# -*- encoding: utf-8 -*-

from __future__ import annotations
from enum import IntEnum
from typing import Dict, Any
from Engine import Color, Input, UI, Text
from Engine.UI import RichText, TextStyle
from .Base import WindowBase
from ..System import System


class FadePhase(IntEnum):
    NOTHING = 0
    IN = 1
    OUT = 2


class WindowMessage(WindowBase):
    def __init__(self) -> None:
        super().__init__(((48, 288), (544, 160)))
        self.setColor(Color(255, 255, 255, 0))
        self._window.setColor(Color(255, 255, 255, 192))
        self._message = ""
        self._textStyles: Dict[str, TextStyle] = {}
        self._initTextStyles()
        self._text = RichText(System.getFonts()[0], self._message, self._textStyles)
        self.content.addChild(self._text)
        self.setVisible(False)

        self._inDialogue: bool = False
        self._fadePhase: FadePhase = FadePhase.NOTHING
        self._fadeInSpeed = 600.0
        self._fadeOutSpeed = 600.0

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

    def setMessage(self, message: str) -> None:
        if message == self._message:
            return
        self._text.setColor(Color(255, 255, 255, 0))
        self.setVisible(True)
        self._inDialogue = True
        self._fadePhase = FadePhase.IN
        self._message = message
        self._text.setString(message)

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
