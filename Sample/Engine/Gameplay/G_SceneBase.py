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
            self._debugUpdateTimer: float = 0.0
            self._memRSS: float = 0.0
            self._sceneMem: float = 0.0
            self._textureMem: float = 0.0
            self._audioMem: float = 0.0
            self._fontMem: float = 0.0

        self._fixedAccumulator: float = 0.0
        self._fixedStep: float = 1.0 / 60.0
        self._maxFixedSteps: int = 5

        self.onCreate()

    def onEnter(self) -> None:
        from Engine import System

        System.setTransition()

    def onQuit(self) -> None:
        pass

    def onCreate(self) -> None:
        pass

    def onTick(self, deltaTime: float) -> None:
        pass

    def onLateTick(self, deltaTime: float) -> None:
        pass

    def onFixedTick(self, fixedDelta: float) -> None:
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

    def main(self) -> None:
        from Engine import System, Input

        self.onEnter()
        while System.isActive() and System.getScene() == self:
            Input.update(System.getWindow())
            Manager.TimeManager.update()
            deltaTime = Manager.TimeManager.v_getDeltaTime()
            self._updateDebugInfo(deltaTime)
            self._fixedAccumulator += deltaTime
            steps = 0
            while self._fixedAccumulator >= self._fixedStep and steps < self._maxFixedSteps:
                self.onFixedTick(self._fixedStep)
                self._fixedLogicHandle(self._fixedStep)
                self._fixedAccumulator -= self._fixedStep
                steps += 1
            self._update(deltaTime)
        self.onQuit()

    def _logicHandle(self, deltaTime: float) -> None:
        for ui in self._UIs:
            ui.onTick(deltaTime)

    def _lateLogicHandle(self, deltaTime: float) -> None:
        for ui in self._UIs:
            ui.onLateTick(deltaTime)

    def _fixedLogicHandle(self, fixedDelta: float) -> None:
        for ui in self._UIs:
            ui.onFixedTick(fixedDelta)

    def _renderHandle(self, deltaTime: float) -> None:
        from Engine import System

        for ui in self._UIs:
            ui.update(deltaTime)
            System.draw(ui)
        if System.isDebugMode() and self._debugHUDEnabled:
            System.draw(self._debugHUD)
        System.display(deltaTime)

    def _update(self, deltaTime: float) -> None:
        from Engine import System

        self.onTick(deltaTime)
        self._logicHandle(deltaTime)
        self.onLateTick(deltaTime)
        self._lateLogicHandle(deltaTime)

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

        realDeltaTime = deltaTime / Manager.TimeManager.getSpeed()
        self._totalTime += realDeltaTime
        FPS = 1.0 / realDeltaTime
        self._totalFrames += 1
        self._averageFPS = self._totalFrames / self._totalTime
        self._debugUpdateTimer += realDeltaTime
        if self._debugUpdateTimer >= 0.5:
            import psutil
            from pympler import asizeof
            process = psutil.Process(os.getpid())
            info = process.memory_info()
            self._memRSS = info.rss * 1.0
            self._sceneMem = asizeof.asizeof(self) * 1.0
            self._textureMem = Manager.TextureManager.getMemory() * 1.0
            self._audioMem = Manager.AudioManager.getMemory() * 1.0
            self._fontMem = Manager.FontManager.getMemory() * 1.0
            self._debugUpdateTimer = 0.0
        import types
        memInfo = types.SimpleNamespace(rss=self._memRSS)
        sceneMem = self._sceneMem
        textureMem = self._textureMem
        audioMem = self._audioMem
        fontMem = self._fontMem

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
