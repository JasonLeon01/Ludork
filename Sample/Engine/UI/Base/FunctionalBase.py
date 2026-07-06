# -*- encoding: utf-8 -*-

from __future__ import annotations
import logging
from typing import Callable, Dict, Any, Optional
from ... import FloatRect
from ...Utils import Math
from .FocusableMixin import Direction, FocusableMixin


class FunctionalBase(FocusableMixin):
    """Mixin providing interactive event callbacks for UI controls.

    Handles hover detection and dispatches confirm, cancel, click, hover,
    mouse, keyboard, and tick events to registered callbacks.
    """

    _keyboardFocusResolver: Optional[Callable[["FunctionalBase"], bool]] = None
    _directionalFocusRequester: Optional[Callable[["FunctionalBase", Direction], bool]] = None
    _keyboardFocusSetter: Optional[Callable[["FunctionalBase"], bool]] = None
    _keyboardCursorResolver: Optional[Callable[["FunctionalBase"], bool]] = None

    def __init__(self) -> None:
        r"""\brief Construct a FunctionalBase mixin.

        Initialises the hovered state to False.
        """
        FocusableMixin.__init__(self)
        self._isHovered: bool = False
        self._active: bool = True

    @classmethod
    def setKeyboardFocusResolver(
        cls,
        resolver: Optional[Callable[["FunctionalBase"], bool]],
    ) -> None:
        r"""\brief Set the global keyboard focus resolver.

        - \param resolver Callback returning whether a control receives keyboard events.
        """
        cls._keyboardFocusResolver = resolver

    @classmethod
    def setDirectionalFocusRequester(
        cls,
        requester: Optional[Callable[["FunctionalBase", Direction], bool]],
    ) -> None:
        r"""\brief Set the global directional focus move requester.

        - \param requester Callback used by controls when focus should leave them.
        """
        cls._directionalFocusRequester = requester

    @classmethod
    def setKeyboardFocusSetter(
        cls,
        setter: Optional[Callable[["FunctionalBase"], bool]],
    ) -> None:
        r"""\brief Set the global keyboard focus setter.

        - \param setter Callback used by controls to request keyboard focus.
        """
        cls._keyboardFocusSetter = setter

    @classmethod
    def setKeyboardCursorResolver(
        cls,
        resolver: Optional[Callable[["FunctionalBase"], bool]],
    ) -> None:
        r"""\brief Set the global keyboard cursor owner resolver.

        - \param resolver Callback returning whether a control owns the selection cursor.
        """
        cls._keyboardCursorResolver = resolver

    def canReceiveFocus(self) -> bool:
        r"""\brief Return whether this control can currently receive focus.

        - \return True if the control is active, visible when applicable, and focusable.
        """
        from .ControlBase import ControlBase

        if not self.getCanReceiveFocus() or not self.getActive():
            return False
        if isinstance(self, ControlBase) and not self.getVisible():
            return False
        return True

    def shouldDispatchKeyboardInput(self) -> bool:
        r"""\brief Return whether keyboard callbacks should be dispatched.

        - \return True if no focus resolver blocks this control.
        """
        resolver = type(self)._keyboardFocusResolver
        if resolver is None:
            return True
        return resolver(self)

    def requestDirectionalFocusMove(self, direction: Direction) -> bool:
        r"""\brief Request a focus move in a direction.

        - \param direction Navigation direction.
        - \return True if focus moved.
        """
        requester = type(self)._directionalFocusRequester
        if requester is None:
            return False
        return requester(self, direction)

    def requestKeyboardFocus(self) -> bool:
        r"""\brief Request keyboard focus for this control.

        - \return True if focus moved to this control.
        """
        setter = type(self)._keyboardFocusSetter
        if setter is None:
            return False
        return setter(self)

    def ownsKeyboardCursorFocus(self) -> bool:
        r"""\brief Return whether this control owns the selection cursor.

        - \return True if the current focus manager assigns cursor ownership to this control.
        """
        resolver = type(self)._keyboardCursorResolver
        if resolver is None:
            return self.getFocused()
        return resolver(self)

    def isHovered(self) -> bool:
        r"""\brief Check whether the mouse is hovering over this control.

        - \return  True if hovered, False otherwise
        """
        return self._isHovered

    def getActive(self) -> bool:
        r"""\brief Check whether this control is active.

        - \return  True if active, False otherwise
        """
        return self._active

    def setActive(self, active: bool) -> None:
        r"""\brief Set the activity state of this control.

        - \param active  True to activate, False to deactivate
        """
        self._active = active

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

    def onMouseButtonDown(self, kwargs: Dict[str, Any]) -> bool:
        r"""\brief Called when any mouse button is pressed, regardless of hover state.

        - \param kwargs  Event arguments (includes position and button)

        - \return True if the event was handled and should be consumed.
        """
        return False

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

    def addMouseButtonDownCallback(self, callback_: Callable) -> None:
        r"""\brief Register a callback for the mouse-button-down action.

        - \param callback_  Callback to invoke on mouse button down
        """
        self.onMouseButtonDown = callback_.__get__(self, type(self))

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
        from Engine.UI.Base import ControlBase

        self.onTick(deltaTime)
        if isinstance(self, ControlBase) and not self.getVisible():
            if self._isHovered:
                self._isHovered = False
                self.onUnHover({"position": Math.ToVector2f(Input.getMousePosition())})
            return
        localMousePos = Math.ToVector2f(Input.getMousePosition())
        active = self.getActive()
        if active and Input.isMouseButtonPressed():
            for btn in [Input.Mouse.Button.Left, Input.Mouse.Button.Right, Input.Mouse.Button.Middle]:
                if not Input.getMouseButtonPressed(btn, handled=False):
                    continue
                if self.onMouseButtonDown({"position": localMousePos, "button": btn}):
                    Input.getMouseButtonPressed(btn, handled=True)
                    Input.isMouseButtonTriggered(btn, handled=True)
        hovered = False
        if isinstance(self, ControlBase):
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
            if active and Input.isMouseButtonPressed():
                self.onClick({"position": localMousePos})
            if active and Input.isMouseWheelScrolled():
                self.onMouseWheelScrolled({"position": localMousePos, "delta": Input.getMouseScrolledWheelDelta()})
        if not hovered:
            if self._isHovered:
                self._isHovered = False
                self.onUnHover({"position": localMousePos})
        if active and isinstance(self, ControlBase):
            bounds = self.getAbsoluteBounds()
            if Input.isTouchBegan():
                beganPos = Input.getTouchBeganPosition()
                if beganPos is not None:
                    touchLocal = Math.ToVector2f(beganPos)
                    if bounds.contains(touchLocal):
                        self.onClick({"position": touchLocal})
            if Input.isTouchMoved():
                touchPos = Input.getTouchPosition()
                if touchPos is not None:
                    touchLocal = Math.ToVector2f(touchPos)
                    if bounds.contains(touchLocal):
                        self.onMouseMoved({"position": touchLocal})
        if self.shouldDispatchKeyboardInput():
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

        Logs at debug level when the object is collected.
        """
        try:
            super().__del__()
        except AttributeError:
            pass
        logging.debug(f"FunctionalBase {self} deleted")
