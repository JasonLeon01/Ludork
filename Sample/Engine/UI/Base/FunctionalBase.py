# -*- encoding: utf-8 -*-

from __future__ import annotations
import logging
from typing import Callable, Dict, Any
from ... import FloatRect
from ...Utils import Math


class FunctionalBase:
    def __init__(self) -> None:
        self._isHovered: bool = False

    def isHovered(self) -> bool:
        return self._isHovered

    def onConfirm(self, kwargs: Dict[str, Any]) -> None:
        pass

    def onCancel(self, kwargs: Dict[str, Any]) -> None:
        pass

    def onClick(self, kwargs: Dict[str, Any]) -> None:
        pass

    def onHover(self, kwargs: Dict[str, Any]) -> None:
        pass

    def onUnHover(self, kwargs: Dict[str, Any]) -> None:
        pass

    def onMouseMoved(self, kwargs: Dict[str, Any]) -> None:
        pass

    def onMouseWheelScrolled(self, kwargs: Dict[str, Any]) -> None:
        pass

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        pass

    def onKeyUp(self, kwargs: Dict[str, Any]) -> None:
        pass

    def onTick(self, deltaTime: float) -> None:
        pass

    def onLateTick(self, deltaTime: float) -> None:
        pass

    def onFixedTick(self, fixedDelta: float) -> None:
        pass

    def addConfirmCallback(self, callback_: Callable) -> None:
        self.onConfirm = callback_.__get__(self, type(self))

    def addCancelCallback(self, callback_: Callable) -> None:
        self.onCancel = callback_.__get__(self, type(self))

    def addClickCallback(self, callback_: Callable) -> None:
        self.onClick = callback_.__get__(self, type(self))

    def addHoverCallback(self, callback_: Callable) -> None:
        self.onHover = callback_.__get__(self, type(self))

    def addUnHoverCallback(self, callback_: Callable) -> None:
        self.onUnHover = callback_.__get__(self, type(self))

    def addMouseMovedCallback(self, callback_: Callable) -> None:
        self.onMouseMoved = callback_.__get__(self, type(self))

    def addMouseWheelScrolledCallback(self, callback_: Callable) -> None:
        self.onMouseWheelScrolled = callback_.__get__(self, type(self))

    def addKeyDownCallback(self, callback_: Callable) -> None:
        self.onKeyDown = callback_.__get__(self, type(self))

    def addKeyUpCallback(self, callback_: Callable) -> None:
        self.onKeyUp = callback_.__get__(self, type(self))

    def update(self, deltaTime: float) -> None:
        from Engine import Input

        self.onTick(deltaTime)
        localMousePos = Math.ToVector2f(Input.getMousePosition())
        hovered = False
        if hasattr(self, "getAbsoluteBounds"):
            bounds: FloatRect = self.getAbsoluteBounds()
            hovered = bounds.contains(localMousePos)
        if not Input.isMouseInputMode():
            hovered = False
        if hovered:
            if not self._isHovered:
                self._isHovered = True
                self.onHover({"position": localMousePos})
            if Input.isMouseMoved():
                self.onMouseMoved({"position": localMousePos})
            if Input.isMouseButtonPressed():
                self.onClick({"position": localMousePos})
            if Input.isMouseWheelScrolled():
                self.onMouseWheelScrolled({"position": localMousePos, "delta": Input.getMouseScrolledWheelDelta()})
        if not hovered:
            if self._isHovered:
                self._isHovered = False
                self.onUnHover({"position": localMousePos})
        if Input.isKeyPressed() or Input.isJoystickButtonPressed() or Input.isJoystickAxisMoved():
            self.onKeyDown({})
        if Input.isKeyReleased() or Input.isJoystickButtonReleased():
            self.onKeyUp({})

    def lateUpdate(self, deltaTime: float) -> None:
        self.onLateTick(deltaTime)

    def fixedUpdate(self, fixedDelta: float) -> None:
        self.onFixedTick(fixedDelta)

    def __del__(self) -> None:
        super().__del__()
        logging.warning(f"FunctionalBase {self} deleted")
