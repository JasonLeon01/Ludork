# -*- encoding: utf-8 -*-

from __future__ import annotations
import logging
from typing import Callable, Dict, Any
from ... import FloatRect
from ...Utils import Math


class FunctionalBase:
    """Mixin providing interactive event callbacks for UI controls.

    Handles hover detection and dispatches confirm, cancel, click, hover,
    mouse, keyboard, and tick events to registered callbacks.
    """
    def __init__(self) -> None:
        r"""\brief Construct a FunctionalBase mixin.

        Initialises the hovered state to False.
        """
        self._isHovered: bool = False

    def isHovered(self) -> bool:
        r"""\brief Check whether the mouse is hovering over this control.

        - \return  True if hovered, False otherwise
        """
        return self._isHovered

    def onConfirm(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Called when the control is confirmed (e.g. mouse click).

        - \param kwargs  Event arguments (may include position)
        """
        pass

    def onCancel(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Called when the control is cancelled.

        - \param kwargs  Event arguments
        """
        pass

    def onClick(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Called when the control is clicked.

        - \param kwargs  Event arguments (may include position)
        """
        pass

    def onHover(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Called when the mouse starts hovering over the control.

        - \param kwargs  Event arguments (may include position)
        """
        pass

    def onUnHover(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Called when the mouse stops hovering over the control.

        - \param kwargs  Event arguments (may include position)
        """
        pass

    def onMouseMoved(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Called when the mouse moves while hovering.

        - \param kwargs  Event arguments (may include position)
        """
        pass

    def onMouseWheelScrolled(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Called when the mouse wheel is scrolled while hovering.

        - \param kwargs  Event arguments (may include position and delta)
        """
        pass

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Called when a key or button is pressed while hovering.

        - \param kwargs  Event arguments (empty dict)
        """
        pass

    def onKeyUp(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Called when a key or button is released while hovering.

        - \param kwargs  Event arguments (empty dict)
        """
        pass

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Called every frame with the frame time.

        - \param deltaTime  Time elapsed since last frame, in seconds
        """
        pass

    def onLateTick(self, deltaTime: float) -> None:
        r"""\brief Called after onTick every frame.

        - \param deltaTime  Time elapsed since last frame, in seconds
        """
        pass

    def onFixedTick(self, fixedDelta: float) -> None:
        r"""\brief Called at a fixed timestep.

        - \param fixedDelta  Fixed timestep duration, in seconds
        """
        pass

    def addConfirmCallback(self, callback_: Callable) -> None:
        r"""\brief Register a callback for the confirm action.

        - \param callback_  Callback to invoke on confirm
        """
        self.onConfirm = callback_.__get__(self, type(self))

    def addCancelCallback(self, callback_: Callable) -> None:
        r"""\brief Register a callback for the cancel action.

        - \param callback_  Callback to invoke on cancel
        """
        self.onCancel = callback_.__get__(self, type(self))

    def addClickCallback(self, callback_: Callable) -> None:
        r"""\brief Register a callback for the click action.

        - \param callback_  Callback to invoke on click
        """
        self.onClick = callback_.__get__(self, type(self))

    def addHoverCallback(self, callback_: Callable) -> None:
        r"""\brief Register a callback for the hover action.

        - \param callback_  Callback to invoke on hover
        """
        self.onHover = callback_.__get__(self, type(self))

    def addUnHoverCallback(self, callback_: Callable) -> None:
        r"""\brief Register a callback for the un-hover action.

        - \param callback_  Callback to invoke on un-hover
        """
        self.onUnHover = callback_.__get__(self, type(self))

    def addMouseMovedCallback(self, callback_: Callable) -> None:
        r"""\brief Register a callback for the mouse-moved action.

        - \param callback_  Callback to invoke on mouse move
        """
        self.onMouseMoved = callback_.__get__(self, type(self))

    def addMouseWheelScrolledCallback(self, callback_: Callable) -> None:
        r"""\brief Register a callback for the mouse-wheel-scrolled action.

        - \param callback_  Callback to invoke on mouse wheel scroll
        """
        self.onMouseWheelScrolled = callback_.__get__(self, type(self))

    def addKeyDownCallback(self, callback_: Callable) -> None:
        r"""\brief Register a callback for the key-down action.

        - \param callback_  Callback to invoke on key down
        """
        self.onKeyDown = callback_.__get__(self, type(self))

    def addKeyUpCallback(self, callback_: Callable) -> None:
        r"""\brief Register a callback for the key-up action.

        - \param callback_  Callback to invoke on key up
        """
        self.onKeyUp = callback_.__get__(self, type(self))

    def update(self, deltaTime: float) -> None:
        r"""\brief Process input and fire tick callbacks.

        - \param deltaTime  Time elapsed since last frame, in seconds
        """
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
        r"""\brief Run late-update tick callback.

        - \param deltaTime  Time elapsed since last frame, in seconds
        """
        self.onLateTick(deltaTime)

    def fixedUpdate(self, fixedDelta: float) -> None:
        r"""\brief Run fixed-timestep tick callback.

        - \param fixedDelta  Fixed timestep duration, in seconds
        """
        self.onFixedTick(fixedDelta)

    def __del__(self) -> None:
        r"""\brief Destructor for FunctionalBase.

        Logs a warning when the object is deleted.
        """
        super().__del__()
        logging.warning(f"FunctionalBase {self} deleted")
