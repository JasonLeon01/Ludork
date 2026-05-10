# -*- encoding: utf-8 -*-
r"""\brief UIManager: manages active UI canvases, event dispatch, and rendering order."""

import os
from typing import List
from Engine import Input
from Engine.Utils import Math
from Engine.UI import Canvas
from . import Manager
from .System import System


class UIManager:
    r"""\brief Manages active UI canvases, event dispatch, and rendering order.

    Handles loading, removal, and sorted update/render of UI canvases.
    Includes optional debug HUD with FPS and memory information.
    """

    def __init__(self) -> None:
        r"""\brief Construct a UI manager with debug HUD if debug mode is enabled."""
        self._UIs: List[Canvas] = []
        self._debugHUDEnabled: bool = False
        if System.isDebugMode():
            from Engine.UI import DefaultFont, PlainText

            self._debugHUDEnabled: bool = True
            self._debugHUD: PlainText = PlainText(DefaultFont, "", 12)
            self._totalTime: float = 0.0
            self._totalFrames: int = 0
            self._averageFPS: float = 0.0
            self._debugUpdateTimer: float = 0.0
            self._memRSS: float = 0.0
            self._sceneMem: float = 0.0
            self._textureMem: float = 0.0
            self._audioMem: float = 0.0
            self._fontMem: float = 0.0

    @ExecSplit(default=(None,))
    def loadUI(self, ui: Canvas) -> None:
        r"""\brief Load a UI canvas into the manager.

        - \param ui The canvas to load.
        """
        self._UIs.append(ui)

    @ReturnType(uis=List[Canvas])
    def getUIs(self) -> List[Canvas]:
        r"""\brief Get the list of all loaded UI canvases.

        - \return A list of Canvas objects.
        """
        return self._UIs

    @ExecSplit(default=(None,))
    def removeUI(self, ui: Canvas) -> None:
        r"""\brief Remove a UI canvas from the manager.

        - \param ui The canvas to remove.
        - \raises ValueError if the UI is not found.
        """
        if ui in self._UIs:
            self._UIs.remove(ui)
        else:
            raise ValueError("UI not found")

    def _fixedLogicHandle(self, fixedDelta: float) -> None:
        sortedUIs = sorted(
            self._UIs, key=lambda item: item.getZOrder() if hasattr(item, "getZOrder") else 0, reverse=True
        )
        for ui in sortedUIs:
            if ui.getActive() and ui.getVisible():
                if hasattr(ui, "fixedUpdate"):
                    ui.fixedUpdate(fixedDelta)

    def _logicHandle(self, deltaTime: float) -> None:
        sortedUIs = sorted(
            self._UIs, key=lambda item: item.getZOrder() if hasattr(item, "getZOrder") else 0, reverse=True
        )
        for ui in sortedUIs:
            if ui.getActive() and ui.getVisible():
                if hasattr(ui, "update"):
                    ui.update(deltaTime)

    def _renderHandle(self, deltaTime: float) -> None:
        sortedUIs = sorted(self._UIs, key=lambda item: item.getZOrder() if hasattr(item, "getZOrder") else 0)
        for ui in sortedUIs:
            if ui.getVisible():
                if hasattr(ui, "render"):
                    ui.render()
                System.draw(ui)
        if System.isDebugMode() and self._debugHUDEnabled:
            System.draw(self._debugHUD)
        System.display(deltaTime)
        for ui in sortedUIs:
            if ui.getActive() and ui.getVisible():
                if hasattr(ui, "lateUpdate"):
                    ui.lateUpdate(deltaTime)

    def _updateDebugInfo(self, deltaTime: float) -> None:
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
        System.recordFPS(FPS)
        self._totalFrames += 1
        self._averageFPS = self._totalFrames / self._totalTime
        self._debugUpdateTimer += realDeltaTime
        if self._debugUpdateTimer >= 0.5:
            import psutil  # type: ignore
            from pympler import asizeof  # type: ignore

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
