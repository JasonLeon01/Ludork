# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
from typing import List, TYPE_CHECKING
from . import Manager
from ..Utils import Event, Math

if TYPE_CHECKING:
    from Engine.UI import Canvas


class SceneBase:
    def __init__(self) -> None:
        from Engine import System

        self._UIs: List[Canvas] = []
        if System.isDebugMode():
            from Engine.UI import UI_Text

            PlainText = UI_Text.PlainText
            self._debugHUDEnabled: bool = True
            self._debugHUD: PlainText = PlainText(list(System.getFonts())[0], "", 12)
            self._totalTime: float = 0.0
            self._totalFrames: int = 0
            self._averageFPS: float = 0.0

        self.onCreate()

    def onCreate(self) -> None:
        pass

    def onTick(self, deltaTime: float) -> None:
        pass

    def onLateTick(self, deltaTime: float) -> None:
        pass

    def onDestroy(self) -> None:
        pass

    def addUI(self, ui: Canvas) -> None:
        self._UIs.append(ui)

    def getUIs(self) -> List[Canvas]:
        return self._UIs

    def removeUI(self, ui: Canvas) -> None:
        if ui in self._UIs:
            self._UIs.remove(ui)
        else:
            raise ValueError("UI not found")

    def _logicHandle(self, deltaTime: float) -> None:
        for ui in self._UIs:
            ui.onTick(deltaTime)
        for ui in self._UIs:
            ui.onLateTick(deltaTime)

    def _renderHandle(self, deltaTime: float) -> None:
        from Engine import System

        for ui in self._UIs:
            ui.update(deltaTime)
            System.draw(ui)
        if System.isDebugMode() and self._debugHUDEnabled:
            System.draw(self._debugHUD)
        System.display()

    def main(self) -> None:
        from Engine import System, Input

        while System.isActive() and System.getScene() == self:
            Input.update(System.getWindow())
            Manager.TimeManager.update()
            deltaTime = Manager.TimeManager.v_getDeltaTime()
            self._updateDebugInfo(deltaTime)
            self._update(deltaTime)

    def _update(self, deltaTime: float) -> None:
        from Engine import System

        self.onTick(deltaTime)
        self._logicHandle(deltaTime)
        self.onLateTick(deltaTime)

        Event.flush()

        System.clearCanvas()
        self._renderHandle(deltaTime)

    def _updateDebugInfo(self, deltaTime: float) -> None:
        from Engine import System, Input, Manager

        if not System.isDebugMode():
            return
        if Input.isKeyTriggered(Input.Key.F3, handled=False):
            self._debugHUDEnabled = not self._debugHUDEnabled

        if not self._debugHUDEnabled:
            return
        if Math.IsNearZero(Manager.TimeManager.getSpeed()):
            return

        import psutil
        from pympler import asizeof

        realDeltaTime = deltaTime / Manager.TimeManager.getSpeed()
        self._totalTime += realDeltaTime
        FPS = 1.0 / realDeltaTime
        self._totalFrames += 1
        self._averageFPS = self._totalFrames / self._totalTime
        process = psutil.Process(os.getpid())
        memInfo = process.memory_info()
        sceneMem = asizeof.asizeof(self)
        textureMem = Manager.TextureManager.getMemory()
        audioMem = Manager.AudioManager.getMemory()
        fontMem = Manager.FontManager.getMemory()

        debugString = ""
        debugString += f"Total Time: {self._totalTime:.2f}s\n"
        debugString += f"FPS: {FPS:.2f}\n"
        debugString += f"Average FPS: {self._averageFPS:.2f}\n"
        debugString += f"Memory: {memInfo.rss / 1024 / 1024:.2f} MB\n"
        debugString += f"Scene Memory: {sceneMem / 1024 / 1024:.2f} MB\n"
        debugString += f"Texture Memory: {textureMem / 1024 / 1024:.2f} MB\n"
        debugString += f"Audio Memory: {audioMem / 1024 / 1024:.2f} MB\n"
        debugString += f"Font Memory: {fontMem / 1024 / 1024:.2f} MB\n"

        self._debugHUD.setString(debugString)
