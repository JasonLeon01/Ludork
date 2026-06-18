# -*- encoding: utf-8 -*-
r"""\brief SceneBase: abstract base class for all game scenes with lifecycle hooks."""

from __future__ import annotations
import inspect
import threading
import time
from typing import Any, Callable, List, Optional
from Engine import Input, ParticleSystem
from Engine.Utils import Event
from . import Manager
from .System import System
from .Animation import Animation
from .CustomParticles import CommonTipController
from .Manager import TimerTaskEntry
from .UIManager import UIManager


class SceneBase:
    r"""\brief Abstract base class for all game scenes.

    Provides lifecycle hooks (onCreate, onEnter, onTick, onFixedTick, onLateTick,
    onDestroy, onQuit), timer management, animation list management,
    and a threaded logic loop.
    """

    def __init__(self) -> None:
        r"""\brief Construct a new scene with default timing and UI manager."""
        self._fixedAccumulator: float = 0.0
        self._fixedStep: float = 1.0 / 60.0
        self._maxFixedSteps: int = 5
        self._logicTargetFPS: int = 60
        self._timerTasks: List[TimerTaskEntry] = []
        self._created = False
        self._animList: List[Animation] = []
        self._uiManager: UIManager = UIManager()
        self._commonTipParticleSystem: ParticleSystem = ParticleSystem()
        self._commonTipController: CommonTipController = CommonTipController(self._commonTipParticleSystem, fontSize=20)
        self._logicThread: Optional[threading.Thread] = None
        self._logicStopEvent: threading.Event = threading.Event()
        self._logicDataLock: threading.RLock = threading.RLock()

    def onEnter(self) -> None:
        r"""\brief Called when the scene becomes active.

        Override to perform scene-specific setup when entering.
        """
        System.setTransition()

    def onQuit(self) -> None:
        r"""\brief Called when the scene is about to be removed.

        Override to perform cleanup before the scene quits.
        """
        pass

    def onCreate(self) -> None:
        r"""\brief Called once when the scene is first created.

        Override to initialise scene resources and state.
        """
        pass

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Called every logic frame.

        - \param deltaTime Elapsed time in seconds since the previous logic frame.
        """
        pass

    def onLateTick(self, deltaTime: float) -> None:
        r"""\brief Called after onTick for late-update logic.

        - \param deltaTime Elapsed time in seconds since the previous frame.
        """
        pass

    def onFixedTick(self, fixedDelta: float) -> None:
        r"""\brief Called at a fixed timestep for physics-like updates.

        - \param fixedDelta Fixed timestep in seconds (default 1/60).
        """
        pass

    def onDestroy(self) -> None:
        r"""\brief Called when the scene is destroyed.

        Override to release resources.
        """
        pass

    @Latent(TimeUp=(True,))
    def addTimer(
        self, interval: float, task: Optional[Callable] = None, params: Optional[List[Any]] = None
    ) -> Callable[[], bool]:
        r"""\brief Add a timer that fires after the specified interval.

        - \param interval Time in seconds before the timer fires.
        - \param task Optional callable to invoke when the timer fires.
        - \param params Optional list of parameters passed to the task.
        - \return A callable condition function that returns True when the timer fires.
        """
        with self._logicDataLock:
            if params is None:
                params = []
            taskEntry = TimerTaskEntry(interval, task, params)
            self._timerTasks.append(taskEntry)

        def condition() -> bool:
            return taskEntry.time <= 0

        return condition

    @ExecSplit(default=(None,))
    def addAnim(self, anim: Animation) -> None:
        r"""\brief Add an animation to the scene's animation list.

        - \param anim The animation to add.
        """
        with self._logicDataLock:
            self._animList.append(anim)

    @ReturnType(anims=List[Animation])
    def getAnims(self) -> List[Animation]:
        r"""\brief Get a snapshot of the current animation list.

        - \return A copy of the animation list.
        """
        with self._logicDataLock:
            return self._animList[:]

    @ExecSplit(default=(None,))
    def removeAnim(self, anim: Animation) -> None:
        r"""\brief Remove an animation from the scene.

        - \param anim The animation to remove.
        - \raises ValueError if the animation is not found.
        """
        with self._logicDataLock:
            if anim in self._animList:
                self._animList.remove(anim)
            else:
                raise ValueError("Animation not found")

    @ExecSplit(default=(None,))
    def clearAnims(self) -> None:
        r"""\brief Remove all animations from the scene."""
        with self._logicDataLock:
            self._animList.clear()

    @ExecSplit(default=(None,))
    def addCommonTip(self, text: str) -> None:
        r"""\brief Display a floating tip text in the current scene.

        - \param text The tip message to display.
        """
        self._commonTipController.addTip(text)

    def main(self) -> None:
        r"""\brief Enter the scene's main loop.

        Calls onCreate once, then onEnter, then runs the game loop
        until the scene is no longer active, then calls onQuit.
        """
        if not self._created:
            self.onCreate()
            self._created = True
        self.onEnter()
        self._startLogicThread()
        while System.isActive() and System.getScene() == self:
            System.applyPendingSceneReplace()
            Input.update(System.getWindow())
            Manager.TimeManager.update()
            deltaTime = Manager.TimeManager.v_getDeltaTime()
            self._uiManager._updatePerformanceInfo(deltaTime)
            self._uiManager._logicHandle(deltaTime)
            self._renderHandle(deltaTime)
            System.clearCanvas()
            self.onLateTick(deltaTime)
            self._commonTipParticleSystem.onLateTick(deltaTime)
            time.sleep(0)
        self._stopLogicThread()
        self.onQuit()

    def _logicHandle(self, deltaTime: float) -> None:
        Event.flush()
        with self._logicDataLock:
            for taskEntry in self._timerTasks[:]:
                taskEntry.time = max(0, taskEntry.time - deltaTime)
                if taskEntry.time <= 0:
                    if taskEntry.task is not None and inspect.isfunction(taskEntry.task):
                        taskEntry.task(*taskEntry.params)
                    self._timerTasks.remove(taskEntry)
            if len(self._animList) > 0:
                for anim in self._animList[:]:
                    if anim.isFinished():
                        self._animList.remove(anim)
                for anim in self._animList:
                    anim.update(deltaTime)
            self._commonTipController.onTick(deltaTime)
            self._commonTipParticleSystem.onTick(deltaTime)

    def _fixedLogicHandle(self, fixedDelta: float) -> None:
        self._uiManager._fixedLogicHandle(fixedDelta)

    def _drawSceneAnims(self) -> None:
        r"""\brief Draw active scene animations to the canvas."""
        animSnapshot = self.getAnims()
        for anim in animSnapshot:
            System.draw(anim)

    def _renderHandle(self, deltaTime: float) -> None:
        self._drawSceneAnims()
        self._uiManager._renderHandle(deltaTime, self._drawCommonTipOverlay)

    def _drawCommonTipOverlay(self) -> None:
        System.setWindowDefaultView()
        System.draw(self._commonTipParticleSystem)

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
