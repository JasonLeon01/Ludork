# -*- encoding: utf-8 -*-

from __future__ import annotations
import inspect
import threading
import time
from typing import Any, Callable, List, Dict, Optional
from Engine import Input
from Engine.Utils import Event
from . import Manager
from .System import System
from .Animation import Animation
from .Manager import TimerTaskEntry
from .UIManager import UIManager


class SceneBase:
    def __init__(self) -> None:
        self._fixedAccumulator: float = 0.0
        self._fixedStep: float = 1.0 / 60.0
        self._maxFixedSteps: int = 5
        self._logicTargetFPS: int = 60
        self._timerTasks: Dict[str, TimerTaskEntry] = {}
        self._created = False
        self._animList: List[Animation] = []
        self._uiManager: UIManager = UIManager()
        self._logicThread: Optional[threading.Thread] = None
        self._logicStopEvent: threading.Event = threading.Event()
        self._logicDataLock: threading.RLock = threading.RLock()

    def onEnter(self) -> None:
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

    @Latent(TimeUp=(True,))
    def addTimer(self, key: str, interval: float, task: Optional[Callable] = None, params: Optional[List[Any]] = None):
        with self._logicDataLock:
            if key in self._timerTasks:
                raise ValueError("Timer key already exists")
            if params is None:
                params = []
            taskEntry = TimerTaskEntry(interval, task, params)
            self._timerTasks[key] = taskEntry

        def condition() -> bool:
            return taskEntry.time <= 0

        return condition

    @ExecSplit(default=(None,))
    def addAnim(self, anim: Animation) -> None:
        with self._logicDataLock:
            self._animList.append(anim)

    @ReturnType(anims=List[Animation])
    def getAnims(self) -> List[Animation]:
        with self._logicDataLock:
            return self._animList[:]

    @ExecSplit(default=(None,))
    def removeAnim(self, anim: Animation) -> None:
        with self._logicDataLock:
            if anim in self._animList:
                self._animList.remove(anim)
            else:
                raise ValueError("Animation not found")

    @ExecSplit(default=(None,))
    def clearAnims(self) -> None:
        with self._logicDataLock:
            self._animList.clear()

    def main(self) -> None:
        if not self._created:
            self.onCreate()
            self._created = True
        self.onEnter()
        self._startLogicThread()
        while System.isActive() and System.getScene() == self:
            Input.update(System.getWindow())
            Manager.TimeManager.update()
            deltaTime = Manager.TimeManager.v_getDeltaTime()
            self._uiManager._updateDebugInfo(deltaTime)
            self._uiManager._logicHandle(deltaTime)
            self._renderHandle(deltaTime)
            System.clearCanvas()
            self.onLateTick(deltaTime)
        self._stopLogicThread()
        self.onQuit()

    def _logicHandle(self, deltaTime: float) -> None:
        Event.flush()
        with self._logicDataLock:
            for key, taskEntry in self._timerTasks.copy().items():
                taskEntry.time = max(0, taskEntry.time - deltaTime)
                if taskEntry.time <= 0:
                    if not taskEntry.task is None and inspect.isfunction(taskEntry.task):
                        taskEntry.task(*taskEntry.params)
                    self._timerTasks.pop(key)
            if len(self._animList) > 0:
                for anim in self._animList[:]:
                    if anim.isFinished():
                        self._animList.remove(anim)
                for anim in self._animList:
                    anim.update(deltaTime)

    def _fixedLogicHandle(self, fixedDelta: float) -> None:
        self._uiManager._fixedLogicHandle(fixedDelta)

    def _renderHandle(self, deltaTime: float) -> None:
        animSnapshot = self.getAnims()
        if len(animSnapshot) > 0:
            for anim in animSnapshot:
                System.draw(anim)
        self._uiManager._renderHandle(deltaTime)

    def _update(self, deltaTime: float) -> None:
        self._renderHandle(deltaTime)
        System.clearCanvas()
        self.onLateTick(deltaTime)

    def _startLogicThread(self) -> None:
        if self._logicThread and self._logicThread.is_alive():
            return
        self._logicStopEvent.clear()
        self._logicThread = threading.Thread(
            target=self._logicLoop,
            name=f"SceneLogicThread-{id(self)}",
            daemon=True,
        )
        self._logicThread.start()

    def _stopLogicThread(self) -> None:
        self._logicStopEvent.set()
        if self._logicThread and self._logicThread.is_alive():
            self._logicThread.join()
        self._logicThread = None

    def _logicLoop(self) -> None:
        logicFrameTime = 1.0 / max(1, self._logicTargetFPS)
        self._fixedAccumulator = 0.0
        lastTime = time.perf_counter()
        while not self._logicStopEvent.is_set() and System.isActive() and System.getScene() == self:
            frameStart = time.perf_counter()
            deltaTime = max(0.0, frameStart - lastTime)
            lastTime = frameStart
            self.onTick(deltaTime)
            self._logicHandle(deltaTime)
            self._fixedAccumulator += deltaTime
            steps = 0
            while self._fixedAccumulator >= self._fixedStep and steps < self._maxFixedSteps:
                self.onFixedTick(self._fixedStep)
                self._fixedLogicHandle(self._fixedStep)
                self._fixedAccumulator -= self._fixedStep
                steps += 1
            elapsed = time.perf_counter() - frameStart
            remain = logicFrameTime - elapsed
            if remain > 0:
                time.sleep(remain)
