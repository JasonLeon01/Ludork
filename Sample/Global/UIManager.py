# -*- encoding: utf-8 -*-
r"""\brief UIManager: manages active UI canvases, event dispatch, and rendering order."""

from typing import Callable, List, Optional
from Engine.Utils import Math
from Engine.UI.Base import ControlBase, FunctionalBase
from . import Manager
from .System import System


class UIManager:
    r"""\brief Manages active UI canvases, event dispatch, and rendering order.

    Handles loading, removal, and sorted update/render of UI canvases.
    """

    def __init__(self) -> None:
        r"""\brief Construct a UI manager."""
        self._UIs: List[ControlBase] = []

    @ExecSplit(default=(None,))
    def loadUI(self, ui: ControlBase) -> None:
        r"""\brief Load a UI canvas into the manager.

        - \param ui The canvas to load.
        """
        self._UIs.append(ui)

    @ReturnType(uis=List[ControlBase])
    def getUIs(self) -> List[ControlBase]:
        r"""\brief Get the list of all loaded UI canvases.

        - \return A list of ControlBase objects.
        """
        return self._UIs

    @ExecSplit(default=(None,))
    def removeUI(self, ui: ControlBase) -> None:
        r"""\brief Remove a UI canvas from the manager.

        - \param ui The canvas to remove.
        - \raises ValueError if the UI is not found.
        """
        if ui in self._UIs:
            self._UIs.remove(ui)
        else:
            raise ValueError("UI not found")

    def _fixedLogicHandle(self, fixedDelta: float) -> None:
        from Engine.UI import Canvas

        sortedUIs = sorted(
            self._UIs, key=lambda item: item.getZOrder() if isinstance(item, Canvas) else 0, reverse=True
        )
        for ui in sortedUIs:
            if ui.getVisible():
                if isinstance(ui, FunctionalBase):
                    ui.fixedUpdate(fixedDelta)

    def _logicHandle(self, deltaTime: float) -> None:
        from Engine.UI import Canvas

        sortedUIs = sorted(
            self._UIs, key=lambda item: item.getZOrder() if isinstance(item, Canvas) else 0, reverse=True
        )
        for ui in sortedUIs:
            if ui.getVisible():
                if isinstance(ui, FunctionalBase):
                    ui.update(deltaTime)

    def _renderHandle(self, deltaTime: float, overlayRenderer: Optional[Callable[[], None]] = None) -> None:
        from Engine.UI import Canvas

        sortedUIs = sorted(self._UIs, key=lambda item: item.getZOrder() if isinstance(item, Canvas) else 0)
        for ui in sortedUIs:
            if ui.getVisible():
                if isinstance(ui, Canvas):
                    ui.render()
                System.draw(ui)
        if overlayRenderer is not None:
            overlayRenderer()
        System.display(deltaTime)
        for ui in sortedUIs:
            if ui.getVisible():
                if isinstance(ui, FunctionalBase):
                    ui.lateUpdate(deltaTime)

    def _updatePerformanceInfo(self, deltaTime: float) -> None:
        if not System.isDebugMode():
            return
        if Math.IsNearZero(Manager.TimeManager.getSpeed()):
            return

        realDeltaTime = deltaTime / Manager.TimeManager.getSpeed()
        FPS = 1.0 / realDeltaTime
        System.recordPerformance(FPS)
