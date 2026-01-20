# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Callable, Dict, Any
from ...Utils import Math


class FunctionalBase:
    def __init__(self) -> None:
        self._isHovered: bool = False
        self._isClicked: bool = False
        self._isKeyDown: bool = False
        self._isKeyUp: bool = False

    def isHovered(self) -> bool:
        return self._isHovered

    def isClicked(self) -> bool:
        return self._isClicked

    def isKeyDown(self) -> bool:
        return self._isKeyDown

    def isKeyUp(self) -> bool:
        return self._isKeyUp

    def onConfirm(self, kwargs: Dict[str, Any]):
        pass

    def onCancel(self, kwargs: Dict[str, Any]):
        pass

    def onClick(self, kwargs: Dict[str, Any]):
        pass

    def onHover(self, kwargs: Dict[str, Any]):
        pass

    def onUnHover(self, kwargs: Dict[str, Any]):
        pass

    def onMouseMoved(self, kwargs: Dict[str, Any]):
        pass

    def onMouseWheelScrolled(self, kwargs: Dict[str, Any]):
        pass

    def onKeyDown(self, kwargs: Dict[str, Any]):
        pass

    def onKeyUp(self, kwargs: Dict[str, Any]):
        pass

    def onTick(self, deltaTime: float) -> None:
        pass

    def onLateTick(self, deltaTime: float) -> None:
        pass

    def onFixedTick(self, fixedDelta: float) -> None:
        pass

    def addConfirmCallback(self, callback_: Callable):
        self.onConfirm = callback_.__get__(self, type(self))

    def addCancelCallback(self, callback_: Callable):
        self.onCancel = callback_.__get__(self, type(self))

    def addClickCallback(self, callback_: Callable):
        self.onClick = callback_.__get__(self, type(self))

    def addHoverCallback(self, callback_: Callable):
        self.onHover = callback_.__get__(self, type(self))

    def addUnHoverCallback(self, callback_: Callable):
        self.onUnHover = callback_.__get__(self, type(self))

    def addMouseMovedCallback(self, callback_: Callable):
        self.onMouseMoved = callback_.__get__(self, type(self))

    def addMouseWheelScrolledCallback(self, callback_: Callable):
        self.onMouseWheelScrolled = callback_.__get__(self, type(self))

    def addKeyDownCallback(self, callback_: Callable):
        self.onKeyDown = callback_.__get__(self, type(self))

    def addKeyUpCallback(self, callback_: Callable):
        self.onKeyUp = callback_.__get__(self, type(self))

    def update(self, deltaTime: float):
        from Engine import Input

        self.onTick(deltaTime)
        localMousePos = Math.ToVector2f(Input.getMousePosition())
        hovered = False
        self._isClicked = False
        self._isKeyDown = False
        self._isKeyUp = False
        if hasattr(self, "getAbsoluteBounds"):
            bounds = self.getAbsoluteBounds()
            hovered = bounds.contains(localMousePos)
        if hovered:
            if not self._isHovered:
                self._isHovered = True
                self.onHover({"position": localMousePos})
            if Input.isMouseMoved():
                self.onMouseMoved({"position": localMousePos})
            if Input.isMouseButtonPressed():
                self._isClicked = True
                self.onClick({"position": localMousePos})
            if Input.isMouseWheelScrolled():
                self.onMouseWheelScrolled({"position": localMousePos, "delta": Input.getMouseScrolledWheelDelta()})
        if not hovered:
            if self._isHovered:
                self._isHovered = False
                self.onUnHover({"position": localMousePos})
        if Input.isKeyPressed():
            self._isKeyDown = True
            self.onKeyDown({})
        if Input.isKeyReleased():
            self._isKeyUp = True
            self.onKeyUp({})

    def lateUpdate(self, deltaTime: float) -> None:
        self.onLateTick(deltaTime)

    def fixedUpdate(self, fixedDelta: float) -> None:
        self.onFixedTick(fixedDelta)
