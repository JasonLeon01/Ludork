# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
import inspect
from typing import Any, Callable, List, Dict, Optional
from .. import Latent, Manager, ExecSplit
from ..UI import Canvas
from ..Utils import Event, Math
from ..Manager import TimerTaskEntry


class SceneBase:
    def __init__(self) -> None:
        from .. import System

        self._UIs: List[Canvas] = []
        if System.isDebugMode():
            from ..UI import UI_Text as Text

            PlainText = Text.PlainText
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
        self._timerTasks: Dict[str, TimerTaskEntry] = {}
        self._created = False

    @ExecSplit(default=(None,))
    def onEnter(self) -> None:
        from .. import System

        System.setTransition()

    @ExecSplit(default=(None,))
    def onQuit(self) -> None:
        pass

    @ExecSplit(default=(None,))
    def onCreate(self) -> None:
        pass

    @ExecSplit(default=(None,))
    def onTick(self, deltaTime: float) -> None:
        pass

    @ExecSplit(default=(None,))
    def onLateTick(self, deltaTime: float) -> None:
        pass

    @ExecSplit(default=(None,))
    def onFixedTick(self, fixedDelta: float) -> None:
        pass

    @ExecSplit(default=(None,))
    def onDestroy(self) -> None:
        pass

    @ExecSplit(default=(None,))
    def addUI(self, ui: Canvas) -> None:
        self._UIs.append(ui)

    @ExecSplit(default=(None,))
    def getUIs(self) -> List[Canvas]:
        return self._UIs

    @ExecSplit(default=(None,))
    def removeUI(self, ui: Canvas) -> None:
        if ui in self._UIs:
            self._UIs.remove(ui)
        else:
            raise ValueError("UI not found")

    @Latent(TimeUp=(True,))
    def addTimer(self, key: str, interval: float, task: Optional[Callable] = None, params: Optional[List[Any]] = None):
        if key in self._timerTasks:
            raise ValueError("Timer key already exists")
        if params is None:
            params = []
        taskEntry = TimerTaskEntry(interval, task, params)
        self._timerTasks[key] = taskEntry

        def condition() -> bool:
            return taskEntry.time <= 0

        return condition

    def main(self) -> None:
        from .. import System, Input

        if not self._created:
            self.onCreate()
            self._created = True
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

    def _fixedLogicHandle(self, fixedDelta: float) -> None:
        for ui in self._UIs:
            if ui.getActive() and ui.getVisible():
                if hasattr(ui, "fixedUpdate"):
                    ui.fixedUpdate(fixedDelta)

    def _renderHandle(self, deltaTime: float) -> None:
        from .. import System

        for ui in self._UIs:
            if ui.getActive() and ui.getVisible():
                if hasattr(ui, "update"):
                    ui.update(deltaTime)
            if ui.getVisible():
                System.draw(ui)
        for ui in self._UIs:
            if ui.getActive() and ui.getVisible():
                if hasattr(ui, "lateUpdate"):
                    ui.lateUpdate(deltaTime)
        if System.isDebugMode() and self._debugHUDEnabled:
            System.draw(self._debugHUD)
        System.display(deltaTime)

    def _update(self, deltaTime: float) -> None:
        from .. import System

        self.onTick(deltaTime)
        self.onLateTick(deltaTime)
        Event.flush()
        for key, taskEntry in self._timerTasks.copy().items():
            taskEntry.time = max(0, taskEntry.time - deltaTime)
            if taskEntry.time <= 0:
                if not taskEntry.task is None and inspect.isfunction(taskEntry.task):
                    taskEntry.task(*taskEntry.params)
                self._timerTasks.pop(key)
        System.clearCanvas()
        self._renderHandle(deltaTime)

    def _updateDebugInfo(self, deltaTime: float) -> None:
        from .. import System, Input, Manager

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
