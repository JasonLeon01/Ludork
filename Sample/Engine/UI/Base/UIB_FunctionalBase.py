# -*- encoding: utf-8 -*-

from __future__ import annotations
from ... import Vector2i


class FunctionalBase:
    def __init__(self) -> None:
        self._isHovered: bool = False

    def onClick(self, position: Vector2i):
        pass

    def onHover(self, position: Vector2i):
        pass

    def onUnHover(self, position: Vector2i):
        pass

    def onMouseMove(self, position: Vector2i):
        pass

    def onMouseWheel(self, position: Vector2i, delta: float):
        pass

    def onKeyDown(self):
        pass

    def onKeyUp(self):
        pass

    def update(self, deltaTime: float):
        from Engine import Input, System

        scale = System.getScale()
        mousePos = Input.getMousePosition()
        logicalPos = Vector2i(int(mousePos.x / scale), int(mousePos.y / scale))
        if hasattr(self, "getGlobalBounds"):
            bounds = self.getGlobalBounds()
            if bounds.contains(logicalPos.x, logicalPos.y):
                if not self._isHovered:
                    self._isHovered = True
                    self.onHover(logicalPos)
                if Input.isMouseMoved():
                    self.onMouseMove(logicalPos)
                if Input.isMouseButtonPressed():
                    self.onClick(logicalPos)
                if Input.isMouseWheelScrolled():
                    self.onMouseWheel(logicalPos, Input.getMouseScrolledWheelDelta())
            else:
                if self._isHovered:
                    self._isHovered = False
                    self.onUnHover(logicalPos)
        if Input.isKeyPressed():
            self.onKeyDown()
        if Input.isKeyReleased():
            self.onKeyUp()
