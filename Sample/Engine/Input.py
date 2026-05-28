# -*- encoding: utf-8 -*-
r"""
\brief Input polling and event system.

Provides a stateful, per-frame input abstraction over SFML's event system.
Supports keyboard, mouse, and gamepad input with action mappings,
trigger/hold detection, and input blocking.
"""

import copy
import logging
import threading
import time
import traceback
from enum import Enum
from typing import Any, Dict, Optional, Tuple, Union, List, Callable
from . import Keyboard, Mouse, Joystick, Touch, WindowBase, Vector2i
from .Utils import Math

_StateLock: threading.RLock = threading.RLock()


class JoystickButton(Enum):
    r"""
    \brief Joystick Button.
    """

    A = 0
    B = 1
    X = 2
    Y = 3
    LB = 4
    RB = 5
    View = 6
    Menu = 7
    LS = 8
    RS = 9
    XBox = 10
    Share = 11


class InputType(Enum):
    r"""
    \brief Input Type.
    """

    Mouse = 0
    Gamepad = 1


Key = Keyboard.Key
Scan = Keyboard.Scan
JoystickAxis = Joystick.Axis
KeyName: Dict[Key, str] = {member: member.name for member in Key.__members__.values()}
ScanName: Dict[Scan, str] = {member: member.name for member in Scan.__members__.values()}
JoyStickButtonName: Dict[JoystickButton, str] = {member: member.name for member in JoystickButton.__members__.values()}
JoystickAxisName: Dict[JoystickAxis, str] = {member: member.name for member in JoystickAxis.__members__.values()}


class _EventState:
    r"""
    \brief Internal state container for input events.

    This class stores all input states for the current frame.
    All members are class-level variables that persist across frames.
    """

    Focused: bool = True  # Whether the window has focus
    FocusLost: bool = False  # Whether focus was lost this frame
    FocusGained: bool = False  # Whether focus was gained this frame

    CurrentInputType: InputType = InputType.Mouse  # Current active input device type

    KeyPressed: bool = False  # Whether any key was pressed this frame
    KeyReleased: bool = False  # Whether any key was released this frame
    KeyPressedMap: Dict[Tuple[Keyboard.Key, bool, bool, bool, bool], bool] = (
        {}
    )  # Map of (key, alt, ctrl, shift, system) -> pressed
    KeyReleasedMap: Dict[Tuple[Keyboard.Key, bool, bool, bool, bool], bool] = (
        {}
    )  # Map of (key, alt, ctrl, shift, system) -> released
    KeyboardScanPressedMap: Dict[Tuple[Keyboard.Scan, bool, bool, bool, bool], bool] = (
        {}
    )  # Map of (scan, alt, ctrl, shift, system) -> pressed
    KeyboardScanReleasedMap: Dict[Tuple[Keyboard.Scan, bool, bool, bool, bool], bool] = (
        {}
    )  # Map of (scan, alt, ctrl, shift, system) -> released
    KeyTriggeredMap: Dict[Tuple[Keyboard.Key, bool, bool, bool, bool], Tuple[int, bool]] = (
        {}
    )  # Map of (key, alt, ctrl, shift, system) -> (press_count, handled)

    MouseWheelScrolled: bool = False  # Whether mouse wheel was scrolled this frame
    MouseScrolledWheel: Optional[Mouse.Wheel] = None  # Which wheel was scrolled
    MouseScrolledWheelDelta: float = 0.0  # Scroll delta
    MouseScrolledWheelPosition: Optional[Vector2i] = None  # Position where wheel was scrolled

    MouseButtonPressed: bool = False  # Whether any mouse button was pressed this frame
    MouseButtonReleased: bool = False  # Whether any mouse button was released this frame
    MouseButtonPressedMap: Dict[Mouse.Button, bool] = {}  # Map of button -> pressed
    MouseButtonReleasedMap: Dict[Mouse.Button, bool] = {}  # Map of button -> released
    MousePressedPosition: Optional[Vector2i] = None  # Position where mouse was pressed
    MouseReleasedPosition: Optional[Vector2i] = None  # Position where mouse was released
    MouseButtonTriggeredMap: Dict[Mouse.Button, Tuple[int, bool]] = {}  # Map of button -> (press_count, handled)

    MouseMoved: bool = False  # Whether mouse was moved this frame
    MousePosition: Vector2i = Vector2i(0, 0)  # Current mouse position
    MouseMovedDelta: Optional[Vector2i] = None  # Mouse movement delta

    MouseEntered: bool = False  # Whether mouse entered the window this frame
    MouseLeft: bool = False  # Whether mouse left the window this frame

    TouchBegan: bool = False  # Whether a touch began this frame
    TouchEnded: bool = False  # Whether a touch ended this frame
    TouchMoved: bool = False  # Whether a touch moved this frame
    TouchActive: bool = False  # Whether a touch is currently down (persistent across frames)
    TouchPosition: Optional[Vector2i] = None  # Current touch position (last known, while active)
    TouchBeganPosition: Optional[Vector2i] = None  # Position where touch began this frame
    TouchEndedPosition: Optional[Vector2i] = None  # Position where touch ended this frame
    TouchMovedDelta: Optional[Vector2i] = None  # Touch movement delta this frame
    TouchBeganHandled: bool = False  # Whether the TouchBegan was handled this frame
    TouchTriggered: Tuple[int, bool] = (0, False)  # (press_count, handled); persists across frames while active
    TouchBlocked: bool = False  # Whether touch input is blocked

    JoystickButtonPressed: bool = False  # Whether any joystick button was pressed this frame
    JoystickButtonReleased: bool = False  # Whether any joystick button was released this frame
    JoystickAxisMoved: bool = False  # Whether any joystick axis was moved this frame
    JoystickConnected: bool = False  # Whether any joystick was connected this frame
    JoystickDisconnected: bool = False  # Whether any joystick was disconnected this frame
    JoystickButtonPressedMap: Dict[int, Dict[int, bool]] = {}  # Map of joystickId -> (button -> pressed)
    JoystickButtonReleasedMap: Dict[int, Dict[int, bool]] = {}  # Map of joystickId -> (button -> released)
    JoystickButtonTriggeredMap: Dict[int, Tuple[int, bool]] = (
        {}
    )  # Map of button -> (press_count, handled); persists until ButtonReleased
    JoystickAxisMovedMap: Dict[int, Dict[Joystick.Axis, float]] = {}  # Map of joystickId -> (axis -> position)
    JoystickAxisStatus: Dict[int, Dict[Joystick.Axis, float]] = {}  # Map of joystickId -> (axis -> current_position)
    JoystickLastDominantAxis: Dict[int, Tuple[Joystick.Axis, float]] = {}  # joyId -> (Axis, Value)
    JoystickAxisJustPressed: Dict[Tuple[int, Joystick.Axis], Tuple[float, bool]] = (
        {}
    )  # (joyId, Axis) -> (Value, handled); persists until axis releases (returns under threshold or flips sign)

    EnteredText: str = ""  # Text entered this frame

    KeyboardBlocked: bool = False  # Whether keyboard input is blocked
    MouseBlocked: bool = False  # Whether mouse input is blocked
    JoystickBlocked: bool = False  # Whether joystick input is blocked

    ActionMappings: Dict[
        Tuple[str, List[Union[Key, Scan, JoystickButton, JoystickAxis]]],
        Tuple[object, Callable[[object, Optional[float]], None], bool],
    ] = {}  # Action name -> (object, callback, trigger_on_hold)

    KeyRepeatStartTime: Dict[
        Tuple[Keyboard.Key, bool, bool, bool, bool], float
    ] = {}  # (key, alt, ctrl, shift, system) -> perf_counter when first triggered
    KeyRepeatLastTime: Dict[
        Tuple[Keyboard.Key, bool, bool, bool, bool], float
    ] = {}  # (key, alt, ctrl, shift, system) -> perf_counter of last repeat fire
    JoystickButtonRepeatStartTime: Dict[int, float] = {}  # button -> first trigger time
    JoystickButtonRepeatLastTime: Dict[int, float] = {}  # button -> last repeat fire time


_InjectedEvents = []
_UseInjectedMouseOnly: bool = False


def _pixelToWorld(window: WindowBase, pixel: Vector2i) -> Vector2i:
    r"""
    \brief Convert a window-pixel position to world (view) coordinates.

    Returns the same coordinates when the window uses a default view that spans
    the full pixel surface. On targets that apply a custom view (e.g. iOS
    letterbox), this maps the raw event pixel position into the UI's logical
    coordinate space so it aligns with `getAbsoluteBounds()` outputs.

    - window: The window whose current view defines the mapping.
    - pixel: Input position in window pixel coordinates.

    \return The mapped position as a Vector2i in world coordinates.
    """
    try:
        world = window.mapPixelToCoords(pixel)
        return Vector2i(int(world.x), int(world.y))
    except Exception:
        return pixel


def setUseInjectedMouseOnly(value: bool) -> None:
    r"""
    \brief Set whether to use only injected mouse events.

    - value: If True, only use injected mouse events instead of polling real mouse position.
    """
    global _UseInjectedMouseOnly
    _UseInjectedMouseOnly = value


def injectEvent(data: Dict[str, Any]) -> None:
    r"""
    \brief Inject an artificial input event.

    - data: Dictionary containing event data with 'type' key and type-specific fields.
    """
    with _StateLock:
        _InjectedEvents.append(data)


def _processInjectedEvents() -> None:
    while _InjectedEvents:
        data = _InjectedEvents.pop(0)
        t = data.get("type")
        if t == "KeyPressed":
            keyName = data.get("key")
            key = getattr(Keyboard.Key, keyName, Keyboard.Key.Unknown)
            scan = Keyboard.Scan.Unknown
            alt = data.get("alt", False)
            ctrl = data.get("control", False)
            shift = data.get("shift", False)
            system = data.get("system", False)

            _EventState.KeyPressed = True
            keyMap = (key, alt, ctrl, shift, system)
            scanMap = (scan, alt, ctrl, shift, system)
            _EventState.KeyPressedMap[keyMap] = True
            _EventState.KeyboardScanPressedMap[scanMap] = True

            if not keyMap in _EventState.KeyTriggeredMap:
                _EventState.KeyTriggeredMap[keyMap] = (0, False)
            count, handled = _EventState.KeyTriggeredMap[keyMap]
            count += 1
            _EventState.KeyTriggeredMap[keyMap] = (count, handled)

        elif t == "KeyReleased":
            keyName = data.get("key")
            key = getattr(Keyboard.Key, keyName, Keyboard.Key.Unknown)
            scan = Keyboard.Scan.Unknown
            alt = data.get("alt", False)
            ctrl = data.get("control", False)
            shift = data.get("shift", False)
            system = data.get("system", False)

            _EventState.KeyReleased = True
            keyMap = (key, alt, ctrl, shift, system)
            scanMap = (scan, alt, ctrl, shift, system)
            _EventState.KeyReleasedMap[keyMap] = True
            _EventState.KeyboardScanReleasedMap[scanMap] = True
            if keyMap in _EventState.KeyTriggeredMap:
                _EventState.KeyTriggeredMap.pop(keyMap, None)

        elif t == "MouseMoved":
            x = data.get("x", 0)
            y = data.get("y", 0)
            _EventState.MouseMoved = True
            _EventState.MousePosition = Vector2i(x, y)

        elif t == "MouseButtonPressed":
            btnName = data.get("button")
            btn = getattr(Mouse.Button, btnName, Mouse.Button.Left)
            x = data.get("x", 0)
            y = data.get("y", 0)
            pos = Vector2i(x, y)

            _EventState.MouseButtonPressed = True
            _EventState.MouseButtonPressedMap[btn] = True
            _EventState.MousePressedPosition = pos

            if not btn in _EventState.MouseButtonTriggeredMap:
                _EventState.MouseButtonTriggeredMap[btn] = (0, False)
            count, handled = _EventState.MouseButtonTriggeredMap[btn]
            count += 1
            _EventState.MouseButtonTriggeredMap[btn] = (count, handled)

        elif t == "MouseButtonReleased":
            btnName = data.get("button")
            btn = getattr(Mouse.Button, btnName, Mouse.Button.Left)
            x = data.get("x", 0)
            y = data.get("y", 0)
            pos = Vector2i(x, y)

            _EventState.MouseButtonReleased = True
            _EventState.MouseButtonReleasedMap[btn] = True
            _EventState.MouseReleasedPosition = pos

            if btn in _EventState.MouseButtonTriggeredMap:
                _EventState.MouseButtonTriggeredMap.pop(btn, None)

        elif t == "MouseWheelScrolled":
            delta = data.get("delta", 0.0)
            x = data.get("x", 0)
            y = data.get("y", 0)

            _EventState.MouseWheelScrolled = True
            _EventState.MouseScrolledWheel = Mouse.Wheel.Vertical
            _EventState.MouseScrolledWheelDelta = delta
            _EventState.MouseScrolledWheelPosition = Vector2i(x, y)

        elif t == "FocusGained":
            _EventState.Focused = True
            _EventState.FocusGained = True

        elif t == "FocusLost":
            _EventState.Focused = False
            _EventState.FocusLost = True


def update(window: WindowBase) -> None:
    r"""
    \brief Update input state for the current frame.

    This function should be called once per frame. It resets all input states,
    processes SFML events, and updates action mappings.

    - window: The window to poll events from.
    """
    with _StateLock:
        _EventState.FocusLost = False
        _EventState.FocusGained = False

        _EventState.KeyPressed = False
        _EventState.KeyReleased = False
        _EventState.KeyPressedMap.clear()
        _EventState.KeyReleasedMap.clear()
        _EventState.KeyboardScanPressedMap.clear()
        _EventState.KeyboardScanReleasedMap.clear()

        _EventState.MouseWheelScrolled = False
        _EventState.MouseScrolledWheel = None
        _EventState.MouseScrolledWheelDelta = 0.0
        _EventState.MouseScrolledWheelPosition = None

        _EventState.MouseButtonPressed = False
        _EventState.MouseButtonReleased = False
        _EventState.MouseButtonPressedMap.clear()
        _EventState.MouseButtonReleasedMap.clear()
        _EventState.MousePressedPosition = None
        _EventState.MouseReleasedPosition = None

        _EventState.MouseMoved = False
        _EventState.MouseMovedDelta = None

        _EventState.MouseEntered = False
        _EventState.MouseLeft = False

        _EventState.TouchBegan = False
        _EventState.TouchEnded = False
        _EventState.TouchMoved = False
        _EventState.TouchBeganPosition = None
        _EventState.TouchEndedPosition = None
        _EventState.TouchMovedDelta = None
        _EventState.TouchBeganHandled = False

        _EventState.JoystickButtonPressed = False
        _EventState.JoystickButtonReleased = False
        _EventState.JoystickAxisMoved = False
        _EventState.JoystickConnected = False
        _EventState.JoystickDisconnected = False
        _EventState.JoystickButtonPressedMap.clear()
        _EventState.JoystickButtonReleasedMap.clear()
        _EventState.JoystickAxisMovedMap.clear()

        _EventState.EnteredText = ""

        _processInjectedEvents()

        try:
            while True:
                event = window.pollEvent()
                if event is None:
                    break
                if event.isClosed():
                    window.close()
                if event.isFocusLost():
                    _EventState.Focused = False
                    _EventState.FocusLost = True
                if event.isFocusGained():
                    _EventState.Focused = True
                    _EventState.FocusGained = True
                if not window.hasFocus():
                    break
                if event.isKeyPressed():
                    _EventState.KeyPressed = True
                    keyEvent = event.getIfKeyPressed()
                    alt = keyEvent.alt
                    ctrl = keyEvent.control
                    shift = keyEvent.shift
                    system = keyEvent.system
                    keyMap = (keyEvent.code, alt, ctrl, shift, system)
                    scanMap = (keyEvent.scancode, alt, ctrl, shift, system)
                    _EventState.KeyPressedMap[keyMap] = True
                    _EventState.KeyboardScanPressedMap[scanMap] = True
                    if not keyMap in _EventState.KeyTriggeredMap:
                        _EventState.KeyTriggeredMap[keyMap] = (0, False)
                    count, handled = _EventState.KeyTriggeredMap[keyMap]
                    count += 1
                    _EventState.KeyTriggeredMap[keyMap] = (count, handled)
                if event.isKeyReleased():
                    _EventState.KeyReleased = True
                    keyEvent = event.getIfKeyReleased()
                    alt = keyEvent.alt
                    ctrl = keyEvent.control
                    shift = keyEvent.shift
                    system = keyEvent.system
                    keyMap = (keyEvent.code, alt, ctrl, shift, system)
                    scanMap = (keyEvent.scancode, alt, ctrl, shift, system)
                    _EventState.KeyReleasedMap[keyMap] = True
                    _EventState.KeyboardScanReleasedMap[scanMap] = True
                    if keyMap in _EventState.KeyTriggeredMap:
                        _EventState.KeyTriggeredMap.pop(keyMap, None)
                if not _UseInjectedMouseOnly:
                    if event.isMouseWheelScrolled():
                        _EventState.MouseWheelScrolled = True
                        mouseWheelEvent = event.getIfMouseWheelScrolled()
                        _EventState.MouseScrolledWheel = mouseWheelEvent.wheel
                        _EventState.MouseScrolledWheelDelta = mouseWheelEvent.delta
                        _EventState.MouseScrolledWheelPosition = _pixelToWorld(window, mouseWheelEvent.position)
                    if event.isMouseButtonPressed():
                        _EventState.MouseButtonPressed = True
                        mouseButtonEvent = event.getIfMouseButtonPressed()
                        _EventState.MouseButtonPressedMap[mouseButtonEvent.button] = True
                        _EventState.MousePressedPosition = _pixelToWorld(window, mouseButtonEvent.position)
                        if not mouseButtonEvent.button in _EventState.MouseButtonTriggeredMap:
                            _EventState.MouseButtonTriggeredMap[mouseButtonEvent.button] = (0, False)
                        count, handled = _EventState.MouseButtonTriggeredMap[mouseButtonEvent.button]
                        count += 1
                        _EventState.MouseButtonTriggeredMap[mouseButtonEvent.button] = (count, handled)
                    if event.isMouseButtonReleased():
                        _EventState.MouseButtonReleased = True
                        mouseButtonEvent = event.getIfMouseButtonReleased()
                        _EventState.MouseButtonReleasedMap[mouseButtonEvent.button] = True
                        _EventState.MouseReleasedPosition = _pixelToWorld(window, mouseButtonEvent.position)
                        if mouseButtonEvent.button in _EventState.MouseButtonTriggeredMap:
                            _EventState.MouseButtonTriggeredMap.pop(mouseButtonEvent.button, None)
                    if event.isMouseMoved():
                        _EventState.MouseMoved = True
                        mouseMoveEvent = event.getIfMouseMoved()
                        lastPosition: Vector2i = copy.copy(_EventState.MousePosition)
                        _EventState.MousePosition = _pixelToWorld(window, mouseMoveEvent.position)
                        if _EventState.MousePosition != lastPosition:
                            _EventState.MouseMovedDelta = _EventState.MousePosition - lastPosition
                    if event.isMouseEntered():
                        _EventState.MouseEntered = True
                    if event.isMouseLeft():
                        _EventState.MouseLeft = True
                if not _EventState.TouchBlocked:
                    if event.isTouchBegan():
                        touchEvent = event.getIfTouchBegan()
                        if touchEvent.finger == 0:
                            _EventState.TouchBegan = True
                            _EventState.TouchActive = True
                            worldPos = _pixelToWorld(window, touchEvent.position)
                            _EventState.TouchBeganPosition = worldPos
                            _EventState.TouchPosition = worldPos
                            count, _ = _EventState.TouchTriggered
                            _EventState.TouchTriggered = (count + 1, False)
                    if event.isTouchMoved():
                        touchEvent = event.getIfTouchMoved()
                        if touchEvent.finger == 0:
                            _EventState.TouchMoved = True
                            lastPosition = Cast(Vector2i, _EventState.TouchPosition)
                            worldPos = _pixelToWorld(window, touchEvent.position)
                            _EventState.TouchPosition = worldPos
                            if lastPosition is not None and worldPos != lastPosition:
                                _EventState.TouchMovedDelta = worldPos - lastPosition
                    if event.isTouchEnded():
                        touchEvent = event.getIfTouchEnded()
                        if touchEvent.finger == 0:
                            _EventState.TouchEnded = True
                            _EventState.TouchActive = False
                            worldPos = _pixelToWorld(window, touchEvent.position)
                            _EventState.TouchEndedPosition = worldPos
                            _EventState.TouchPosition = worldPos
                            _EventState.TouchTriggered = (0, False)
                if event.isJoystickButtonPressed():
                    _EventState.JoystickButtonPressed = True
                    joystickButtonEvent = event.getIfJoystickButtonPressed()
                    if joystickButtonEvent.joystickId not in _EventState.JoystickButtonPressedMap:
                        _EventState.JoystickButtonPressedMap[joystickButtonEvent.joystickId] = {}
                    _EventState.JoystickButtonPressedMap[joystickButtonEvent.joystickId][
                        joystickButtonEvent.button
                    ] = True
                    if not joystickButtonEvent.button in _EventState.JoystickButtonTriggeredMap:
                        _EventState.JoystickButtonTriggeredMap[joystickButtonEvent.button] = (0, False)
                    count, handled = _EventState.JoystickButtonTriggeredMap[joystickButtonEvent.button]
                    count += 1
                    _EventState.JoystickButtonTriggeredMap[joystickButtonEvent.button] = (count, handled)
                if event.isJoystickButtonReleased():
                    _EventState.JoystickButtonReleased = True
                    joystickButtonEvent = event.getIfJoystickButtonReleased()
                    if joystickButtonEvent.joystickId not in _EventState.JoystickButtonReleasedMap:
                        _EventState.JoystickButtonReleasedMap[joystickButtonEvent.joystickId] = {}
                    _EventState.JoystickButtonReleasedMap[joystickButtonEvent.joystickId][
                        joystickButtonEvent.button
                    ] = True
                    if joystickButtonEvent.button in _EventState.JoystickButtonTriggeredMap:
                        _EventState.JoystickButtonTriggeredMap.pop(joystickButtonEvent.button, None)
                if event.isJoystickMoved():
                    _EventState.JoystickAxisMoved = True
                    joystickMoveEvent = event.getIfJoystickMoved()
                    if joystickMoveEvent.joystickId not in _EventState.JoystickAxisMovedMap:
                        _EventState.JoystickAxisMovedMap[joystickMoveEvent.joystickId] = {}
                    _EventState.JoystickAxisMovedMap[joystickMoveEvent.joystickId][
                        joystickMoveEvent.axis
                    ] = joystickMoveEvent.position

                    if joystickMoveEvent.joystickId not in _EventState.JoystickAxisStatus:
                        _EventState.JoystickAxisStatus[joystickMoveEvent.joystickId] = {}
                    _EventState.JoystickAxisStatus[joystickMoveEvent.joystickId][
                        joystickMoveEvent.axis
                    ] = joystickMoveEvent.position
                if event.isJoystickConnected():
                    _EventState.JoystickConnected = True
                if event.isJoystickDisconnected():
                    _EventState.JoystickDisconnected = True
                    joystickDisconnectEvent = event.getIfJoystickDisconnected()
                    if joystickDisconnectEvent.joystickId in _EventState.JoystickAxisStatus:
                        del _EventState.JoystickAxisStatus[joystickDisconnectEvent.joystickId]
                    if joystickDisconnectEvent.joystickId in _EventState.JoystickButtonPressedMap:
                        del _EventState.JoystickButtonPressedMap[joystickDisconnectEvent.joystickId]
                    if joystickDisconnectEvent.joystickId in _EventState.JoystickButtonReleasedMap:
                        del _EventState.JoystickButtonReleasedMap[joystickDisconnectEvent.joystickId]
                if event.isTextEntered():
                    _EventState.EnteredText += event.getIfTextEntered().unicode

            if window.hasFocus() and not _UseInjectedMouseOnly:
                polledMousePosition = _pixelToWorld(window, Mouse.getPosition(window))
                if polledMousePosition != _EventState.MousePosition:
                    lastPosition: Vector2i = copy.copy(_EventState.MousePosition)
                    _EventState.MousePosition = polledMousePosition
                    _EventState.MouseMoved = True
                    _EventState.MouseMovedDelta = polledMousePosition - lastPosition

            if _EventState.EnteredText == "\x16":
                from . import Clipboard

                _EventState.EnteredText = Clipboard.getString()

            for joyId, axes in _EventState.JoystickAxisStatus.items():
                maxAxis = None
                maxVal = 0.0
                for axis, val in axes.items():
                    if abs(val) > maxVal:
                        maxVal = abs(val)
                        maxAxis = axis

                lastAxis, lastVal = _EventState.JoystickLastDominantAxis.get(joyId, (None, 0.0))

                if maxVal < 10.0:
                    maxAxis = None
                    maxVal = 0.0

                if maxAxis != lastAxis:
                    if lastAxis is not None:
                        _EventState.JoystickAxisJustPressed.pop((joyId, lastAxis), None)
                    if maxAxis is not None:
                        _EventState.JoystickAxisJustPressed[(joyId, maxAxis)] = (axes[maxAxis], False)
                    _EventState.JoystickLastDominantAxis[joyId] = (maxAxis, maxVal)
                elif maxAxis is not None:
                    _EventState.JoystickLastDominantAxis[joyId] = (maxAxis, axes[maxAxis])

            newInputType = None
            if _EventState.MouseMoved or _EventState.MouseButtonPressed or _EventState.MouseWheelScrolled:
                newInputType = InputType.Mouse
            elif (
                _EventState.KeyPressed
                or _EventState.JoystickButtonPressed
                or len(_EventState.JoystickAxisJustPressed) > 0
            ):
                newInputType = InputType.Gamepad

            if newInputType is None and _EventState.JoystickAxisMoved:
                for joyId, axes in _EventState.JoystickAxisStatus.items():
                    for axis, val in axes.items():
                        if abs(val) > 10.0:
                            newInputType = InputType.Gamepad
                            break
                    if newInputType is not None:
                        break

            if newInputType is not None and newInputType != _EventState.CurrentInputType:
                _EventState.CurrentInputType = newInputType
                try:
                    window.setMouseCursorVisible(newInputType == InputType.Mouse)
                except AttributeError:
                    pass

            moveActions: List[Tuple[Joystick.Axis, float, Callable, List[Any]]] = []
            for actionType, callables in _EventState.ActionMappings.items():
                _, actionKeysTuple = actionType
                obj, objCallable, triggerOnHold = callables

                actionKeys = list(actionKeysTuple)
                for key in actionKeys:
                    if not _EventState.KeyboardBlocked:
                        if isinstance(key, Key):
                            triggered = False
                            if (key, False, False, False, False) in _EventState.KeyPressedMap:
                                triggered = True
                            elif triggerOnHold and Keyboard.isKeyPressed(key):
                                triggered = True

                            if triggered:
                                objCallable(obj, None)
                        if isinstance(key, Scan):
                            if (key, False, False, False, False) in _EventState.KeyboardScanPressedMap:
                                objCallable(obj, None)
                    if not _EventState.JoystickBlocked:
                        if isinstance(key, JoystickButton):
                            triggered = False
                            for joyId, joystickDict in _EventState.JoystickButtonPressedMap.items():
                                if joystickDict.get(key.value, False):
                                    triggered = True
                                    break

                            if not triggered and triggerOnHold:
                                for jId in range(Joystick.Count):
                                    if Joystick.isConnected(jId) and Joystick.isButtonPressed(jId, key.value):
                                        triggered = True
                                        break

                            if triggered:
                                objCallable(obj, None)

                        if isinstance(key, tuple):
                            axis, threshold, callable_ = key
                            if isinstance(axis, JoystickAxis):
                                for _, axisMap in _EventState.JoystickAxisStatus.items():
                                    if axis in axisMap:
                                        position = axisMap[axis]
                                        if not Math.IsNearZero(position):
                                            if callable_(position, threshold):
                                                moveActions.append((axis, position, objCallable, [obj, position]))
            finalMoveAction: Optional[Tuple[Callable, List[Any]]] = None
            maxPosition = 0.0
            for axis, position, objCallable, params in moveActions:
                if position != 0 and abs(position) > maxPosition:
                    maxPosition = abs(position)
                    finalMoveAction = (objCallable, params)
            if finalMoveAction:
                callable_, params = finalMoveAction
                callable_(*params)

        except Exception as e:
            logging.error(f"Error in Input.update: {e}\n {traceback.format_exc()}")

    from .NodeGraph import latentManager

    latentManager.update()


def isFocused() -> bool:
    r"""
    \brief Check if the window is currently focused.

    \return True if the window has focus, False otherwise.
    """
    return _EventState.Focused


def isFocusLost() -> bool:
    r"""
    \brief Check if the window lost focus this frame.

    \return True if focus was lost this frame, False otherwise.
    """
    return _EventState.FocusLost


def isFocusGained() -> bool:
    r"""
    \brief Check if the window gained focus this frame.

    \return True if focus was gained this frame, False otherwise.
    """
    return _EventState.FocusGained


def isKeyPressed() -> bool:
    r"""
    \brief Check if any key was pressed this frame.

    \return True if any key was pressed and keyboard is not blocked, False otherwise.
    """
    return _EventState.KeyPressed and not _EventState.KeyboardBlocked


def isKeyReleased() -> bool:
    r"""
    \brief Check if any key was released this frame.

    \return True if any key was released and keyboard is not blocked, False otherwise.
    """
    return _EventState.KeyReleased and not _EventState.KeyboardBlocked


def getKeyPressed(
    key: Keyboard.Key,
    handled: bool,
    alt: bool = False,
    ctrl: bool = False,
    shift: bool = False,
    system: bool = False,
) -> bool:
    r"""
    \brief Check if a specific key was pressed this frame.

    - key: The key to check.
    - handled: Whether to mark the key as handled if pressed.
    - alt: Whether ALT modifier is required.
    - ctrl: Whether CTRL modifier is required.
    - shift: Whether SHIFT modifier is required.
    - system: Whether SYSTEM modifier is required.

    \return True if the key was pressed this frame, False otherwise.
    """
    with _StateLock:
        if not isKeyPressed():
            return False
        mapKey = (key, alt, ctrl, shift, system)
        if mapKey in _EventState.KeyPressedMap:
            result = _EventState.KeyPressedMap[mapKey]
            if result and handled:
                _EventState.KeyPressedMap[mapKey] = False
            return result
        return False


def getScanPressed(
    scan: Keyboard.Scan,
    handled: bool,
    alt: bool = False,
    ctrl: bool = False,
    shift: bool = False,
    system: bool = False,
) -> bool:
    r"""
    \brief Check if a specific scan code was pressed this frame.

    - scan: The scan code to check.
    - handled: Whether to mark the scan as handled if pressed.
    - alt: Whether ALT modifier is required.
    - ctrl: Whether CTRL modifier is required.
    - shift: Whether SHIFT modifier is required.
    - system: Whether SYSTEM modifier is required.

    \return True if the scan was pressed this frame, False otherwise.
    """
    with _StateLock:
        if not isKeyPressed():
            return False
        mapScan = (scan, alt, ctrl, shift, system)
        if mapScan in _EventState.KeyboardScanPressedMap:
            result = _EventState.KeyboardScanPressedMap[mapScan]
            if result and handled:
                _EventState.KeyboardScanPressedMap[mapScan] = False
            return result
        return False


def getKeyReleased(
    key: Keyboard.Key,
    handled: bool,
    alt: bool = False,
    ctrl: bool = False,
    shift: bool = False,
    system: bool = False,
) -> bool:
    r"""
    \brief Check if a specific key was released this frame.

    - key: The key to check.
    - handled: Whether to mark the key as handled if released.
    - alt: Whether ALT modifier is required.
    - ctrl: Whether CTRL modifier is required.
    - shift: Whether SHIFT modifier is required.
    - system: Whether SYSTEM modifier is required.

    \return True if the key was released this frame, False otherwise.
    """
    with _StateLock:
        if not isKeyReleased():
            return False
        mapKey = (key, alt, ctrl, shift, system)
        if mapKey in _EventState.KeyReleasedMap:
            result = _EventState.KeyReleasedMap[mapKey]
            if result and handled:
                _EventState.KeyReleasedMap[mapKey] = False
            return result
        return False


def getScanReleased(
    scan: Keyboard.Scan,
    handled: bool,
    alt: bool = False,
    ctrl: bool = False,
    shift: bool = False,
    system: bool = False,
) -> bool:
    r"""
    \brief Check if a specific scan code was released this frame.

    - scan: The scan code to check.
    - handled: Whether to mark the scan as handled if released.
    - alt: Whether ALT modifier is required.
    - ctrl: Whether CTRL modifier is required.
    - shift: Whether SHIFT modifier is required.
    - system: Whether SYSTEM modifier is required.

    \return True if the scan was released this frame, False otherwise.
    """
    with _StateLock:
        if not isKeyReleased():
            return False
        mapScan = (scan, alt, ctrl, shift, system)
        if mapScan in _EventState.KeyboardScanReleasedMap:
            result = _EventState.KeyboardScanReleasedMap[mapScan]
            if result and handled:
                _EventState.KeyboardScanReleasedMap[mapScan] = False
            return result
        return False


def isMouseWheelScrolled() -> bool:
    r"""
    \brief Check if the mouse wheel was scrolled this frame.

    \return True if the mouse wheel was scrolled and mouse is not blocked, False otherwise.
    """
    return _EventState.MouseWheelScrolled and not _EventState.MouseBlocked


def getMouseScrolledWheel() -> Optional[Mouse.Wheel]:
    r"""
    \brief Get which mouse wheel was scrolled this frame.

    \return The wheel that was scrolled, or None if not scrolled.
    """
    if not isMouseWheelScrolled():
        return None
    return _EventState.MouseScrolledWheel


def getMouseScrolledWheelDelta() -> float:
    r"""
    \brief Get the mouse wheel scroll delta.

    \return The scroll delta, or 0.0 if not scrolled.
    """
    if not isMouseWheelScrolled():
        return 0.0
    return _EventState.MouseScrolledWheelDelta


def getMouseScrolledWheelPosition() -> Optional[Vector2i]:
    r"""
    \brief Get the position where the mouse wheel was scrolled.

    \return The position where the wheel was scrolled, or None if not scrolled.
    """
    if not isMouseWheelScrolled():
        return None
    return _EventState.MouseScrolledWheelPosition


def isMouseButtonPressed() -> bool:
    r"""
    \brief Check if any mouse button was pressed this frame.

    \return True if any mouse button was pressed and mouse is not blocked, False otherwise.
    """
    return _EventState.MouseButtonPressed and not _EventState.MouseBlocked


def isMouseButtonReleased() -> bool:
    r"""
    \brief Check if any mouse button was released this frame.

    \return True if any mouse button was released and mouse is not blocked, False otherwise.
    """
    return _EventState.MouseButtonReleased and not _EventState.MouseBlocked


def getMouseButtonPressed(button: Mouse.Button, handled: bool) -> bool:
    r"""
    \brief Check if a specific mouse button was pressed this frame.

    - button: The button to check.
    - handled: Whether to mark the button as handled if pressed.

    \return True if the button was pressed this frame, False otherwise.
    """
    with _StateLock:
        if not isMouseButtonPressed():
            return False
        if button in _EventState.MouseButtonPressedMap:
            result = _EventState.MouseButtonPressedMap[button]
            if result and handled:
                _EventState.MouseButtonPressedMap[button] = False
            return result
        return False


def getMouseButtonReleased(button: Mouse.Button, handled: bool) -> bool:
    r"""
    \brief Check if a specific mouse button was released this frame.

    - button: The button to check.
    - handled: Whether to mark the button as handled if released.

    \return True if the button was released this frame, False otherwise.
    """
    with _StateLock:
        if not isMouseButtonReleased():
            return False
        if button in _EventState.MouseButtonReleasedMap:
            result = _EventState.MouseButtonReleasedMap[button]
            if result and handled:
                _EventState.MouseButtonReleasedMap[button] = False
            return result
        return False


def isMouseMoved() -> bool:
    r"""
    \brief Check if the mouse was moved this frame.

    \return True if the mouse was moved and mouse is not blocked, False otherwise.
    """
    return _EventState.MouseMoved and not _EventState.MouseBlocked


def getMousePosition() -> Vector2i:
    r"""
    \brief Get the current mouse position.

    \return The current mouse position.
    """
    return _EventState.MousePosition


def getMouseMovedDelta() -> Optional[Vector2i]:
    r"""
    \brief Get the mouse movement delta this frame.

    \return The mouse movement delta, or None if mouse was not moved.
    """
    if not isMouseMoved():
        return None
    return _EventState.MouseMovedDelta


def setMousePosition(position: Vector2i, window: WindowBase) -> None:
    r"""
    \brief Set the mouse position relative to a window.

    - position: The new mouse position.
    - window: The window to set position relative to.
    """
    Mouse.setPosition(position, window)


def isMouseEntered() -> bool:
    r"""
    \brief Check if the mouse entered the window this frame.

    \return True if mouse entered and mouse is not blocked, False otherwise.
    """
    return _EventState.MouseEntered and not _EventState.MouseBlocked


def isMouseLeft() -> bool:
    r"""
    \brief Check if the mouse left the window this frame.

    \return True if mouse left and mouse is not blocked, False otherwise.
    """
    return _EventState.MouseLeft and not _EventState.MouseBlocked


def isTouchBegan(handled: bool = False) -> bool:
    r"""
    \brief Check if a touch began this frame (primary finger).

    - handled: Whether to mark the TouchBegan as handled (consumed) if true.

    \return True if a touch began this frame and touch is not blocked, False otherwise.
    """
    with _StateLock:
        if _EventState.TouchBlocked:
            return False
        if not _EventState.TouchBegan or _EventState.TouchBeganHandled:
            return False
        if handled:
            _EventState.TouchBeganHandled = True
        return True


def isTouchEnded() -> bool:
    r"""
    \brief Check if a touch ended this frame (primary finger).

    \return True if a touch ended this frame and touch is not blocked, False otherwise.
    """
    return _EventState.TouchEnded and not _EventState.TouchBlocked


def isTouchMoved() -> bool:
    r"""
    \brief Check if the primary touch moved this frame.

    \return True if the primary touch moved this frame and touch is not blocked, False otherwise.
    """
    return _EventState.TouchMoved and not _EventState.TouchBlocked


def isTouchActive() -> bool:
    r"""
    \brief Check whether the primary finger is currently touching the screen.

    The state persists across frames until a TouchEnded event.

    \return True if the primary finger is currently down and touch is not blocked, False otherwise.
    """
    return _EventState.TouchActive and not _EventState.TouchBlocked


def getTouchPosition() -> Optional[Vector2i]:
    r"""
    \brief Get the most recently reported primary touch position.

    \return The last known touch position, or None if no touch has occurred.
    """
    return _EventState.TouchPosition


def getTouchBeganPosition() -> Optional[Vector2i]:
    r"""
    \brief Get the position where the primary touch began this frame.

    \return The touch-began position, or None if no TouchBegan occurred this frame.
    """
    return _EventState.TouchBeganPosition


def getTouchEndedPosition() -> Optional[Vector2i]:
    r"""
    \brief Get the position where the primary touch ended this frame.

    \return The touch-ended position, or None if no TouchEnded occurred this frame.
    """
    return _EventState.TouchEndedPosition


def getTouchMovedDelta() -> Optional[Vector2i]:
    r"""
    \brief Get the primary touch movement delta this frame.

    \return The touch movement delta, or None if touch was not moved this frame.
    """
    if not isTouchMoved():
        return None
    return _EventState.TouchMovedDelta


def isTouchTriggered(handled: bool = False) -> bool:
    r"""
    \brief Check if the primary touch is currently triggered (held since TouchBegan, not yet consumed).

    Persists across frames until the touch ends or is consumed via handled=True.

    - handled: Whether to mark the touch as handled (consumed) if triggered.

    \return True if the touch is currently triggered and not yet handled, False otherwise.
    """
    with _StateLock:
        if _EventState.TouchBlocked:
            return False
        count, handled_ = _EventState.TouchTriggered
        if handled_ or count < 1:
            return False
        if handled:
            _EventState.TouchTriggered = (count, True)
        return True


def isTouchBlocked() -> bool:
    r"""
    \brief Check if touch input is blocked.

    \return True if touch is blocked, False otherwise.
    """
    return _EventState.TouchBlocked


def blockTouch() -> None:
    r"""
    \brief Block touch input.
    """
    _EventState.TouchBlocked = True


def unblockTouch() -> None:
    r"""
    \brief Unblock touch input.
    """
    _EventState.TouchBlocked = False


def isJoystickButtonPressed() -> bool:
    r"""
    \brief Check if any joystick button was pressed this frame.

    \return True if any joystick button was pressed and joystick is not blocked, False otherwise.
    """
    return _EventState.JoystickButtonPressed and not _EventState.JoystickBlocked


def isJoystickButtonReleased() -> bool:
    r"""
    \brief Check if any joystick button was released this frame.

    \return True if any joystick button was released and joystick is not blocked, False otherwise.
    """
    return _EventState.JoystickButtonReleased and not _EventState.JoystickBlocked


def isMouseInputMode() -> bool:
    r"""
    \brief Check if the current input mode is mouse.

    \return True if the current input type is Mouse, False otherwise.
    """
    return _EventState.CurrentInputType == InputType.Mouse


def getJoystickButtonPressed(joystickId: int, button: Union[int, JoystickButton], handled: bool) -> bool:
    r"""
    \brief Check if a specific joystick button was pressed this frame.

    - joystickId: The ID of the joystick.
    - button: The button to check.
    - handled: Whether to mark the button as handled if pressed.

    \return True if the button was pressed this frame, False otherwise.
    """
    with _StateLock:
        if not isJoystickButtonPressed():
            return False
        if isinstance(button, JoystickButton):
            button = button.value
        if joystickId in _EventState.JoystickButtonPressedMap:
            if button in _EventState.JoystickButtonPressedMap[joystickId]:
                result = _EventState.JoystickButtonPressedMap[joystickId][button]
                if result and handled:
                    _EventState.JoystickButtonPressedMap[joystickId][button] = False
                return result
        return False


def getJoystickButtonReleased(joystickId: int, button: int, handled: bool) -> bool:
    r"""
    \brief Check if a specific joystick button was released this frame.

    - joystickId: The ID of the joystick.
    - button: The button to check.
    - handled: Whether to mark the button as handled if released.

    \return True if the button was released this frame, False otherwise.
    """
    with _StateLock:
        if not isJoystickButtonReleased():
            return False
        if joystickId in _EventState.JoystickButtonReleasedMap:
            if button in _EventState.JoystickButtonReleasedMap[joystickId]:
                result = _EventState.JoystickButtonReleasedMap[joystickId][button]
                if result and handled:
                    _EventState.JoystickButtonReleasedMap[joystickId][button] = False
                return result
        return False


def isJoystickAxisMoved() -> bool:
    r"""
    \brief Check if any joystick axis was moved this frame.

    \return True if any joystick axis was moved and joystick is not blocked, False otherwise.
    """
    return _EventState.JoystickAxisMoved and not _EventState.JoystickBlocked


def getJoystickAxisMoved(joystickId: int, handled: bool) -> Optional[Tuple[Joystick.Axis, float]]:
    r"""
    \brief Get the joystick axis that was moved this frame.

    - joystickId: The ID of the joystick.
    - handled: Whether to mark the axis movement as handled.

    \return Tuple of (axis, position) if an axis was moved, None otherwise.
    """
    with _StateLock:
        if not isJoystickAxisMoved():
            return None
        if joystickId in _EventState.JoystickAxisMovedMap:
            axisMap = _EventState.JoystickAxisMovedMap[joystickId]
            if not axisMap:
                return None
            axis, pos = next(iter(axisMap.items()))
            if handled:
                del axisMap[axis]
                if not axisMap:
                    del _EventState.JoystickAxisMovedMap[joystickId]
            return (axis, pos)
        return None


def isJoystickConnected() -> bool:
    r"""
    \brief Check if any joystick was connected this frame.

    \return True if any joystick was connected and joystick is not blocked, False otherwise.
    """
    return _EventState.JoystickConnected and not _EventState.JoystickBlocked


def isJoystickDisconnected() -> bool:
    r"""
    \brief Check if any joystick was disconnected this frame.

    \return True if any joystick was disconnected and joystick is not blocked, False otherwise.
    """
    return _EventState.JoystickDisconnected and not _EventState.JoystickBlocked


def isKeyTriggered(
    key: Keyboard.Key,
    alt: bool = False,
    ctrl: bool = False,
    shift: bool = False,
    system: bool = False,
    handled: bool = False,
    repeatDelay: float = 0.0,
    repeatInterval: float = 0.0,
) -> bool:
    r"""
    \brief Check if a key is currently triggered (held since first press, not yet consumed).

    A key becomes triggered on KeyPressed and stays triggered across frames
    until either it is released (KeyReleased) or consumed via handled=True.
    This is thread-safe and decoupled from the per-frame KeyPressed flag,
    so concurrent threads observing input will agree on the trigger state.

    When `repeatInterval` > 0.0 the function also supports repeat fires:
    the first press returns True immediately (and is consumed via `handled`),
    then after `repeatDelay` seconds of holding the same key it returns True
    again at `repeatInterval`-second intervals until the key is released.

    - key: The key to check.
    - alt: Whether ALT modifier is required.
    - ctrl: Whether CTRL modifier is required.
    - shift: Whether SHIFT modifier is required.
    - system: Whether SYSTEM modifier is required.
    - handled: Whether to mark the key as handled (consumed) if triggered.
    - repeatDelay: Seconds to wait after first press before repeat starts (0 = no repeat).
    - repeatInterval: Seconds between repeat fires after the initial delay.

    \return True if the key is currently triggered and not yet handled, False otherwise.
    """
    with _StateLock:
        if _EventState.KeyboardBlocked:
            return False
        keyMap = (key, alt, ctrl, shift, system)
        entry = _EventState.KeyTriggeredMap.get(keyMap)
        if entry is None:
            _EventState.KeyRepeatStartTime.pop(keyMap, None)
            _EventState.KeyRepeatLastTime.pop(keyMap, None)
            return False
        count, handled_ = entry

        if repeatInterval > 0.0:
            if not handled_:
                now = time.perf_counter()
                _EventState.KeyRepeatStartTime[keyMap] = now
                _EventState.KeyRepeatLastTime[keyMap] = now
                if handled:
                    _EventState.KeyTriggeredMap[keyMap] = (count, True)
                return True
            if Keyboard.isKeyPressed(key):
                now = time.perf_counter()
                startTime = _EventState.KeyRepeatStartTime.get(keyMap, 0.0)
                lastTime = _EventState.KeyRepeatLastTime.get(keyMap, 0.0)
                if now - startTime >= repeatDelay and now - lastTime >= repeatInterval:
                    _EventState.KeyRepeatLastTime[keyMap] = now
                    return True
            return False

        if handled_ or count < 1:
            return False
        if handled:
            _EventState.KeyTriggeredMap[keyMap] = (count, True)
        return True


def _isAnyJoystickButtonDown(button: int) -> bool:
    r"""\brief Check whether any connected joystick has `button` held right now.

    - \param button  Integer button index.

    - \return True when at least one connected joystick reports the button pressed.
    """
    for jId in range(Joystick.Count):
        if Joystick.isConnected(jId) and Joystick.isButtonPressed(jId, button):
            return True
    return False


def isAnyJoystickButtonTriggered(
    button: Union[int, JoystickButton],
    handled: bool = False,
    repeatDelay: float = 0.0,
    repeatInterval: float = 0.0,
) -> bool:
    r"""
    \brief Check if a joystick button is currently triggered (held since first press, not yet consumed).

    Persists across frames until the button is released or consumed via handled=True.

    When `repeatInterval` > 0.0 the function also supports repeat fires
    with the same timing semantics as `isKeyTriggered`.

    - button: The button to check.
    - handled: Whether to mark the button as handled (consumed) if triggered.
    - repeatDelay: Seconds to wait after first press before repeat starts (0 = no repeat).
    - repeatInterval: Seconds between repeat fires after the initial delay.

    \return True if the button is currently triggered and not yet handled, False otherwise.
    """
    with _StateLock:
        if _EventState.JoystickBlocked:
            return False
        if isinstance(button, JoystickButton):
            button = button.value
        entry = _EventState.JoystickButtonTriggeredMap.get(button)
        if entry is None:
            _EventState.JoystickButtonRepeatStartTime.pop(button, None)
            _EventState.JoystickButtonRepeatLastTime.pop(button, None)
            return False
        count, handled_ = entry

        if repeatInterval > 0.0:
            if not handled_:
                now = time.perf_counter()
                _EventState.JoystickButtonRepeatStartTime[button] = now
                _EventState.JoystickButtonRepeatLastTime[button] = now
                if handled:
                    _EventState.JoystickButtonTriggeredMap[button] = (count, True)
                return True
            if _isAnyJoystickButtonDown(button):
                now = time.perf_counter()
                startTime = _EventState.JoystickButtonRepeatStartTime.get(button, 0.0)
                lastTime = _EventState.JoystickButtonRepeatLastTime.get(button, 0.0)
                if now - startTime >= repeatDelay and now - lastTime >= repeatInterval:
                    _EventState.JoystickButtonRepeatLastTime[button] = now
                    return True
            return False

        if handled_ or count < 1:
            return False
        if handled:
            _EventState.JoystickButtonTriggeredMap[button] = (count, True)
        return True


def isActionTriggered(
    actionKeys: List[
        Union[
            Key,
            Scan,
            JoystickButton,
            Tuple[JoystickAxis, float, Callable[[float, float], bool]],
        ]
    ],
    handled: bool = False,
    repeatDelay: float = 0.0,
    repeatInterval: float = 0.0,
) -> bool:
    r"""
    \brief Check whether any key in an action key set is currently triggered.

    Trigger state persists across frames until release or consumption (handled=True),
    and is consistent across threads via an internal lock.

    When `repeatInterval` > 0.0, keyboard keys and joystick buttons in the
    action set gain repeat-fire behaviour (see `isKeyTriggered` for timing).

    - actionKeys: List of keys/buttons/axes to check.
    - handled: Whether to mark the matching action keys as handled (consumed) if triggered.
    - repeatDelay: Seconds to wait after first press before repeat starts (0 = no repeat).
    - repeatInterval: Seconds between repeat fires after the initial delay.

    \return True if any action key is currently triggered and not yet handled, False otherwise.
    """
    with _StateLock:
        triggered = False
        for key in actionKeys:
            if isinstance(key, (Key, Scan)):
                if isKeyTriggered(key, handled=handled, repeatDelay=repeatDelay, repeatInterval=repeatInterval):
                    triggered = True
            elif isinstance(key, JoystickButton):
                if isAnyJoystickButtonTriggered(key, handled=handled, repeatDelay=repeatDelay, repeatInterval=repeatInterval):
                    triggered = True
            elif isinstance(key, tuple):
                axis, threshold, condition = key
                if _EventState.JoystickBlocked:
                    continue
                matched = False
                for (joyId, pressedAxis), entry in list(_EventState.JoystickAxisJustPressed.items()):
                    pos, handled_ = entry
                    if pressedAxis == axis and not handled_ and condition(pos, threshold):
                        matched = True
                        if handled:
                            _EventState.JoystickAxisJustPressed[(joyId, pressedAxis)] = (pos, True)
                        break
                if matched:
                    triggered = True
                elif _EventState.JoystickAxisMoved:
                    is_nav_key = abs(threshold) in [10.0, 50.0]
                    if not is_nav_key:
                        for joyId, axisMap in _EventState.JoystickAxisMovedMap.items():
                            if axis in axisMap:
                                pos = axisMap[axis]
                                if condition(pos, threshold):
                                    triggered = True
        return triggered


def isMouseButtonTriggered(
    button: Mouse.Button,
    handled: bool = False,
) -> bool:
    r"""
    \brief Check if a mouse button is currently triggered (held since first press, not yet consumed).

    Persists across frames until the button is released or consumed via handled=True.

    - button: The button to check.
    - handled: Whether to mark the button as handled (consumed) if triggered.

    \return True if the button is currently triggered and not yet handled, False otherwise.
    """
    with _StateLock:
        if _EventState.MouseBlocked:
            return False
        entry = _EventState.MouseButtonTriggeredMap.get(button)
        if entry is None:
            return False
        count, handled_ = entry
        if handled_ or count < 1:
            return False
        if handled:
            _EventState.MouseButtonTriggeredMap[button] = (count, True)
        return True


def isMouseButtonDown(button: Mouse.Button) -> bool:
    r"""
    \brief Check if a mouse button is currently held down.

    This ignores whether the matching press trigger was already handled, which
    makes it suitable for drag interactions after another widget selects focus.

    - button: The button to check.

    \return True if the button is held, False otherwise.
    """
    with _StateLock:
        if _EventState.MouseBlocked:
            return False
        entry = _EventState.MouseButtonTriggeredMap.get(button)
        if entry is not None and entry[0] >= 1:
            return True
        if _UseInjectedMouseOnly:
            return False
    return Mouse.isButtonPressed(button)


def getEnteredText() -> str:
    r"""
    \brief Get the text entered this frame.

    \return The entered text string.
    """
    return _EventState.EnteredText


def isTextEntered() -> bool:
    r"""
    \brief Check if any text was entered this frame.

    \return True if text was entered and keyboard is not blocked, False otherwise.
    """
    return (len(_EventState.EnteredText) > 0) and not _EventState.KeyboardBlocked


def isKeyboardBlocked() -> bool:
    r"""
    \brief Check if keyboard input is blocked.

    \return True if keyboard is blocked, False otherwise.
    """
    return _EventState.KeyboardBlocked


def isMouseBlocked() -> bool:
    r"""
    \brief Check if mouse input is blocked.

    \return True if mouse is blocked, False otherwise.
    """
    return _EventState.MouseBlocked


def isJoystickBlocked() -> bool:
    r"""
    \brief Check if joystick input is blocked.

    \return True if joystick is blocked, False otherwise.
    """
    return _EventState.JoystickBlocked


def blockKeyboard() -> None:
    r"""
    \brief Block keyboard input.
    """
    _EventState.KeyboardBlocked = True


def blockMouse() -> None:
    r"""
    \brief Block mouse input.
    """
    _EventState.MouseBlocked = True


def blockJoystick() -> None:
    r"""
    \brief Block joystick input.
    """
    _EventState.JoystickBlocked = True


def unblockKeyboard() -> None:
    r"""
    \brief Unblock keyboard input.
    """
    _EventState.KeyboardBlocked = False


def unblockMouse() -> None:
    r"""
    \brief Unblock mouse input.
    """
    _EventState.MouseBlocked = False


def unblockJoystick() -> None:
    r"""
    \brief Unblock joystick input.
    """
    _EventState.JoystickBlocked = False


def blockInput() -> None:
    r"""
    \brief Block all input (keyboard, mouse, joystick, touch).
    """
    _EventState.KeyboardBlocked = True
    _EventState.MouseBlocked = True
    _EventState.JoystickBlocked = True
    _EventState.TouchBlocked = True


def unblockInput() -> None:
    r"""
    \brief Unblock all input (keyboard, mouse, joystick, touch).
    """
    _EventState.KeyboardBlocked = False
    _EventState.MouseBlocked = False
    _EventState.JoystickBlocked = False
    _EventState.TouchBlocked = False


def getConfirmKeys() -> (
    List[Union[Key, Scan, JoystickButton, Tuple[JoystickAxis, float, Callable[[float, float], bool]]]]
):
    r"""
    \brief Get the default confirmation action keys.

    \return List of keys/buttons that trigger confirmation.
    """
    return [Key.Enter, Key.Space, Scan.Enter, Scan.Space, JoystickButton.A]


def getCancelKeys() -> (
    List[Union[Key, Scan, JoystickButton, Tuple[JoystickAxis, float, Callable[[float, float], bool]]]]
):
    r"""
    \brief Get the default cancellation action keys.

    \return List of keys/buttons that trigger cancellation.
    """
    return [Key.Escape, Scan.Escape, JoystickButton.B]


def getUpKeys() -> List[Union[Key, Scan, JoystickButton, Tuple[JoystickAxis, float, Callable[[float, float], bool]]]]:
    r"""
    \brief Get the default up navigation keys.

    \return List of keys/buttons that trigger up navigation.
    """
    return [
        Key.Up,
        Scan.Up,
        (Joystick.Axis.Y, -10.0, float.__lt__),
        (Joystick.Axis.PovY, 50.0, float.__gt__),
    ]


def getDownKeys() -> List[Union[Key, Scan, JoystickButton, Tuple[JoystickAxis, float, Callable[[float, float], bool]]]]:
    r"""
    \brief Get the default down navigation keys.

    \return List of keys/buttons that trigger down navigation.
    """
    return [
        Key.Down,
        Scan.Down,
        (Joystick.Axis.Y, 10.0, float.__gt__),
        (Joystick.Axis.PovY, -50.0, float.__lt__),
    ]


def getLeftKeys() -> List[Union[Key, Scan, JoystickButton, Tuple[JoystickAxis, float, Callable[[float, float], bool]]]]:
    r"""
    \brief Get the default left navigation keys.

    \return List of keys/buttons that trigger left navigation.
    """
    return [
        Key.Left,
        Scan.Left,
        (Joystick.Axis.X, -10.0, float.__lt__),
        (Joystick.Axis.PovX, -50.0, float.__lt__),
    ]


def getRightKeys() -> (
    List[Union[Key, Scan, JoystickButton, Tuple[JoystickAxis, float, Callable[[float, float], bool]]]]
):
    r"""
    \brief Get the default right navigation keys.

    \return List of keys/buttons that trigger right navigation.
    """
    return [
        Key.Right,
        Scan.Right,
        (Joystick.Axis.X, 10.0, float.__gt__),
        (Joystick.Axis.PovX, 50.0, float.__gt__),
    ]


def registerActionMapping(
    obj: object,
    actionName: str,
    actionKeys: List[Union[Key, Scan, JoystickButton, Tuple[JoystickAxis, float, Callable[[float, float], bool]]]],
    callable_: Callable[[object, Optional[float]], None],
    triggerOnHold: bool = False,
) -> None:
    r"""
    \brief Register an action mapping.

    - obj: The object that owns the callback.
    - actionName: Name of the action.
    - actionKeys: List of keys/buttons that trigger this action.
    - callable_: Callback function to invoke when action is triggered.
    - triggerOnHold: Whether to trigger on key/button hold.
    """
    _EventState.ActionMappings[(actionName, tuple(actionKeys))] = (obj, callable_, triggerOnHold)


def unregisterActionMapping(obj: object, actionName: str) -> None:
    r"""
    \brief Unregister an action mapping.

    - obj: The object that owns the callback.
    - actionName: Name of the action to unregister.
    """
    toRemove = [k for k, v in _EventState.ActionMappings.items() if k[0] == actionName and v[0] == obj]
    for k in toRemove:
        _EventState.ActionMappings.pop(k, None)
