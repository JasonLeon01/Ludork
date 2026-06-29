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


def _resolveKeyCode(keyCode: Key, scancode: Scan) -> Key:
    r"""\brief Resolve a logical key from SFML key code and scancode.

      SFML may report ``Key.Unknown`` while the scancode is valid; localize it so
    ``KeyPressedMap`` lookups match ``getKeyPressed``.
    """
    if keyCode != Key.Unknown:
        return keyCode
    if scancode != Scan.Unknown:
        localizedKey = Keyboard.localize(scancode)
        if localizedKey != Key.Unknown:
            return localizedKey
    return keyCode


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
    TouchFingerPositions: Dict[int, Vector2i] = {}  # Active touch fingers -> current position
    TouchCancelMouseActive: bool = False  # Whether two-finger touch is holding virtual right mouse
    TouchCancelMousePressedThisFrame: bool = False  # Whether virtual right mouse was pressed this frame
    TouchCancelMouseReleasePending: Optional[Vector2i] = None  # Deferred virtual right mouse release position

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

    KeyRepeatStartTime: Dict[Tuple[Keyboard.Key, bool, bool, bool, bool], float] = (
        {}
    )  # (key, alt, ctrl, shift, system) -> perf_counter when first triggered
    KeyRepeatLastTime: Dict[Tuple[Keyboard.Key, bool, bool, bool, bool], float] = (
        {}
    )  # (key, alt, ctrl, shift, system) -> perf_counter of last repeat fire
    JoystickButtonRepeatStartTime: Dict[int, float] = {}  # button -> first trigger time
    JoystickButtonRepeatLastTime: Dict[int, float] = {}  # button -> last repeat fire time


_InjectedEvents = []
_UseInjectedMouseOnly: bool = False


def _resetFrameState() -> None:
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
    _EventState.TouchCancelMousePressedThisFrame = False

    _EventState.JoystickButtonPressed = False
    _EventState.JoystickButtonReleased = False
    _EventState.JoystickAxisMoved = False
    _EventState.JoystickConnected = False
    _EventState.JoystickDisconnected = False
    _EventState.JoystickButtonPressedMap.clear()
    _EventState.JoystickButtonReleasedMap.clear()
    _EventState.JoystickAxisMovedMap.clear()

    _EventState.EnteredText = ""


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


def _keyEventMaps(
    key: Keyboard.Key,
    scan: Keyboard.Scan,
    alt: bool,
    ctrl: bool,
    shift: bool,
    system: bool,
) -> Tuple[
    Tuple[Keyboard.Key, bool, bool, bool, bool],
    Tuple[Keyboard.Scan, bool, bool, bool, bool],
]:
    return (key, alt, ctrl, shift, system), (scan, alt, ctrl, shift, system)


def _setKeyPressed(
    key: Keyboard.Key,
    scan: Keyboard.Scan,
    alt: bool,
    ctrl: bool,
    shift: bool,
    system: bool,
) -> None:
    _EventState.KeyPressed = True
    keyMap, scanMap = _keyEventMaps(key, scan, alt, ctrl, shift, system)
    _EventState.KeyPressedMap[keyMap] = True
    _EventState.KeyboardScanPressedMap[scanMap] = True

    if keyMap not in _EventState.KeyTriggeredMap:
        _EventState.KeyTriggeredMap[keyMap] = (0, False)
    count, handled = _EventState.KeyTriggeredMap[keyMap]
    _EventState.KeyTriggeredMap[keyMap] = (count + 1, handled)


def _setKeyReleased(
    key: Keyboard.Key,
    scan: Keyboard.Scan,
    alt: bool,
    ctrl: bool,
    shift: bool,
    system: bool,
) -> None:
    _EventState.KeyReleased = True
    keyMap, scanMap = _keyEventMaps(key, scan, alt, ctrl, shift, system)
    _EventState.KeyReleasedMap[keyMap] = True
    _EventState.KeyboardScanReleasedMap[scanMap] = True
    _EventState.KeyTriggeredMap.pop(keyMap, None)


def _parseInjectedKeyEvent(data: Dict[str, Any]) -> Tuple[Keyboard.Key, Keyboard.Scan, bool, bool, bool, bool]:
    keyName = data.get("key")
    key = getattr(Keyboard.Key, keyName, Keyboard.Key.Unknown)
    return (
        key,
        Keyboard.Scan.Unknown,
        data.get("alt", False),
        data.get("control", False),
        data.get("shift", False),
        data.get("system", False),
    )


def _parseNativeKeyEvent(keyEvent: Any) -> Tuple[Keyboard.Key, Keyboard.Scan, bool, bool, bool, bool]:
    return (
        _resolveKeyCode(keyEvent.code, keyEvent.scancode),
        keyEvent.scancode,
        keyEvent.alt,
        keyEvent.control,
        keyEvent.shift,
        keyEvent.system,
    )


def _setMouseButtonPressed(button: Mouse.Button, position: Vector2i) -> None:
    _EventState.MouseButtonPressed = True
    _EventState.MouseButtonPressedMap[button] = True
    _EventState.MousePressedPosition = position
    _EventState.MousePosition = position
    if button not in _EventState.MouseButtonTriggeredMap:
        _EventState.MouseButtonTriggeredMap[button] = (0, False)
    count, handled = _EventState.MouseButtonTriggeredMap[button]
    _EventState.MouseButtonTriggeredMap[button] = (count + 1, handled)


def _setMouseButtonReleased(button: Mouse.Button, position: Vector2i) -> None:
    _EventState.MouseButtonReleased = True
    _EventState.MouseButtonReleasedMap[button] = True
    _EventState.MouseReleasedPosition = position
    _EventState.MousePosition = position
    _EventState.MouseButtonTriggeredMap.pop(button, None)


def _parseInjectedMouseButtonEvent(data: Dict[str, Any]) -> Tuple[Mouse.Button, Vector2i]:
    btnName = data.get("button")
    button = getattr(Mouse.Button, btnName, Mouse.Button.Left)
    return button, Vector2i(data.get("x", 0), data.get("y", 0))


def _beginTwoFingerCancel(position: Vector2i) -> None:
    if _EventState.TouchCancelMouseActive:
        return
    _setMouseButtonPressed(Mouse.Button.Right, position)
    _EventState.TouchCancelMouseActive = True
    _EventState.TouchCancelMousePressedThisFrame = True
    _EventState.TouchBeganHandled = True
    _EventState.TouchTriggered = (0, True)


def _endTwoFingerCancel(position: Vector2i) -> None:
    if not _EventState.TouchCancelMouseActive:
        return
    _EventState.TouchCancelMouseActive = False
    if _EventState.TouchCancelMousePressedThisFrame:
        _EventState.TouchCancelMouseReleasePending = position
        return
    _setMouseButtonReleased(Mouse.Button.Right, position)


def _releasePendingTwoFingerCancel() -> None:
    position = _EventState.TouchCancelMouseReleasePending
    if position is None:
        return
    _EventState.TouchCancelMouseReleasePending = None
    _setMouseButtonReleased(Mouse.Button.Right, position)


def setUseInjectedMouseOnly(value: bool) -> None:
    r"""
    \brief Set whether to use only injected mouse events.

    - value: If True, only use injected mouse events instead of polling real mouse position.
    """
    global _UseInjectedMouseOnly
    _UseInjectedMouseOnly = value


def _canUseKeyboardInput() -> bool:
    return _EventState.Focused and not _EventState.KeyboardBlocked


def _setFocused(value: bool) -> None:
    if _EventState.Focused == value:
        return
    _EventState.Focused = value
    if value:
        _EventState.FocusGained = True
    else:
        _EventState.FocusLost = True
        _clearKeyboardState()


def _clearKeyboardState() -> None:
    _EventState.KeyPressed = False
    _EventState.KeyReleased = False
    _EventState.KeyPressedMap.clear()
    _EventState.KeyReleasedMap.clear()
    _EventState.KeyboardScanPressedMap.clear()
    _EventState.KeyboardScanReleasedMap.clear()
    _EventState.KeyTriggeredMap.clear()
    _EventState.KeyRepeatStartTime.clear()
    _EventState.KeyRepeatLastTime.clear()


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
            _setFocused(True)
            _setKeyPressed(*_parseInjectedKeyEvent(data))

        elif t == "KeyReleased":
            _setFocused(True)
            _setKeyReleased(*_parseInjectedKeyEvent(data))

        elif t == "MouseMoved":
            x = data.get("x", 0)
            y = data.get("y", 0)
            _EventState.MouseMoved = True
            _EventState.MousePosition = Vector2i(x, y)

        elif t == "MouseButtonPressed":
            button, position = _parseInjectedMouseButtonEvent(data)
            _setMouseButtonPressed(button, position)

        elif t == "MouseButtonReleased":
            button, position = _parseInjectedMouseButtonEvent(data)
            _setMouseButtonReleased(button, position)

        elif t == "MouseWheelScrolled":
            delta = data.get("delta", 0.0)
            x = data.get("x", 0)
            y = data.get("y", 0)

            _EventState.MouseWheelScrolled = True
            _EventState.MouseScrolledWheel = Mouse.Wheel.Vertical
            _EventState.MouseScrolledWheelDelta = delta
            _EventState.MouseScrolledWheelPosition = Vector2i(x, y)

        elif t == "FocusGained":
            _setFocused(True)

        elif t == "FocusLost":
            _setFocused(False)


def _syncNativeWindowFocus(window: WindowBase) -> None:
    if _UseInjectedMouseOnly:
        return
    _setFocused(window.hasFocus())


def _usesInjectedWindowInput() -> bool:
    return _UseInjectedMouseOnly


def _updateNativeKeyState(event: Any) -> None:
    if event.isKeyPressed():
        _setKeyPressed(*_parseNativeKeyEvent(event.getIfKeyPressed()))
    if event.isKeyReleased():
        _setKeyReleased(*_parseNativeKeyEvent(event.getIfKeyReleased()))


def _updateJoystickDominantAxes() -> None:
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


def _updateInputType(window: WindowBase) -> None:
    newInputType = None
    if _EventState.MouseMoved or _EventState.MouseButtonPressed or _EventState.MouseWheelScrolled:
        newInputType = InputType.Mouse
    elif _EventState.KeyPressed or _EventState.JoystickButtonPressed or len(_EventState.JoystickAxisJustPressed) > 0:
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


def _dispatchKeyboardAction(
    key: Union[Key, Scan],
    obj: object,
    objCallable: Callable[[object, Optional[float]], None],
    triggerOnHold: bool,
) -> None:
    if not _canUseKeyboardInput():
        return
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


def _dispatchJoystickButtonAction(
    key: JoystickButton,
    obj: object,
    objCallable: Callable[[object, Optional[float]], None],
    triggerOnHold: bool,
) -> None:
    if _EventState.JoystickBlocked:
        return
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


def _collectJoystickAxisAction(
    key: Tuple[JoystickAxis, float, Callable[[float, float], bool]],
    obj: object,
    objCallable: Callable[[object, Optional[float]], None],
    moveActions: List[Tuple[Joystick.Axis, float, Callable, List[Any]]],
) -> None:
    if _EventState.JoystickBlocked:
        return
    axis, threshold, callable_ = key
    if isinstance(axis, JoystickAxis):
        for _, axisMap in _EventState.JoystickAxisStatus.items():
            if axis in axisMap:
                position = axisMap[axis]
                if not Math.IsNearZero(position):
                    if callable_(position, threshold):
                        moveActions.append((axis, position, objCallable, [obj, position]))


def _dispatchActionMappings() -> None:
    moveActions: List[Tuple[Joystick.Axis, float, Callable, List[Any]]] = []
    for actionType, callables in _EventState.ActionMappings.items():
        _, actionKeysTuple = actionType
        obj, objCallable, triggerOnHold = callables

        actionKeys = list(actionKeysTuple)
        for key in actionKeys:
            if isinstance(key, (Key, Scan)):
                _dispatchKeyboardAction(key, obj, objCallable, triggerOnHold)
            if isinstance(key, JoystickButton):
                _dispatchJoystickButtonAction(key, obj, objCallable, triggerOnHold)
            if isinstance(key, tuple):
                _collectJoystickAxisAction(key, obj, objCallable, moveActions)

    finalMoveAction: Optional[Tuple[Callable, List[Any]]] = None
    maxPosition = 0.0
    for axis, position, objCallable, params in moveActions:
        if position != 0 and abs(position) > maxPosition:
            maxPosition = abs(position)
            finalMoveAction = (objCallable, params)
    if finalMoveAction:
        callable_, params = finalMoveAction
        callable_(*params)


def update(window: WindowBase) -> None:
    r"""
    \brief Update input state for the current frame.

    This function should be called once per frame. It resets all input states,
    processes SFML events, and updates action mappings.

    - window: The window to poll events from.
    """
    with _StateLock:
        _resetFrameState()
        _releasePendingTwoFingerCancel()
        _processInjectedEvents()
        _syncNativeWindowFocus(window)

        try:
            while True:
                event = window.pollEvent()
                if event is None:
                    break
                if event.isClosed():
                    window.close()
                if event.isFocusLost():
                    if not _usesInjectedWindowInput():
                        _setFocused(False)
                if event.isFocusGained():
                    if not _usesInjectedWindowInput():
                        _setFocused(True)
                if not _usesInjectedWindowInput() and not window.hasFocus():
                    break
                if not _usesInjectedWindowInput():
                    _updateNativeKeyState(event)
                if not _UseInjectedMouseOnly:
                    if event.isMouseWheelScrolled():
                        _EventState.MouseWheelScrolled = True
                        mouseWheelEvent = event.getIfMouseWheelScrolled()
                        _EventState.MouseScrolledWheel = mouseWheelEvent.wheel
                        _EventState.MouseScrolledWheelDelta = mouseWheelEvent.delta
                        _EventState.MouseScrolledWheelPosition = _pixelToWorld(window, mouseWheelEvent.position)
                    if event.isMouseButtonPressed():
                        mouseButtonEvent = event.getIfMouseButtonPressed()
                        _setMouseButtonPressed(
                            mouseButtonEvent.button, _pixelToWorld(window, mouseButtonEvent.position)
                        )
                    if event.isMouseButtonReleased():
                        mouseButtonEvent = event.getIfMouseButtonReleased()
                        _setMouseButtonReleased(
                            mouseButtonEvent.button, _pixelToWorld(window, mouseButtonEvent.position)
                        )
                    if event.isMouseMoved():
                        _EventState.MouseMoved = True
                        mouseMoveEvent = event.getIfMouseMoved()
                        lastPosition = copy.copy(_EventState.MousePosition)
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
                        worldPos = _pixelToWorld(window, touchEvent.position)
                        _EventState.TouchFingerPositions[touchEvent.finger] = worldPos
                        if touchEvent.finger == 0:
                            _EventState.TouchBegan = True
                            _EventState.TouchActive = True
                            _EventState.TouchBeganPosition = worldPos
                            _EventState.TouchPosition = worldPos
                            count, _ = _EventState.TouchTriggered
                            _EventState.TouchTriggered = (count + 1, False)
                        if len(_EventState.TouchFingerPositions) >= 2:
                            _beginTwoFingerCancel(worldPos)
                    if event.isTouchMoved():
                        touchEvent = event.getIfTouchMoved()
                        worldPos = _pixelToWorld(window, touchEvent.position)
                        if touchEvent.finger in _EventState.TouchFingerPositions:
                            _EventState.TouchFingerPositions[touchEvent.finger] = worldPos
                        if touchEvent.finger == 0:
                            _EventState.TouchMoved = True
                            lastPosition = Cast(Vector2i, _EventState.TouchPosition)
                            _EventState.TouchPosition = worldPos
                            if lastPosition is not None and worldPos != lastPosition:
                                _EventState.TouchMovedDelta = worldPos - lastPosition
                    if event.isTouchEnded():
                        touchEvent = event.getIfTouchEnded()
                        worldPos = _pixelToWorld(window, touchEvent.position)
                        _EventState.TouchFingerPositions.pop(touchEvent.finger, None)
                        if touchEvent.finger == 0:
                            _EventState.TouchEnded = True
                            _EventState.TouchActive = False
                            _EventState.TouchEndedPosition = worldPos
                            _EventState.TouchPosition = worldPos
                            _EventState.TouchTriggered = (0, False)
                        if _EventState.TouchCancelMouseActive and len(_EventState.TouchFingerPositions) < 2:
                            _endTwoFingerCancel(worldPos)
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
                if not _usesInjectedWindowInput() and event.isTextEntered():
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

            _updateJoystickDominantAxes()
            _updateInputType(window)
            _dispatchActionMappings()

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
    return _EventState.KeyPressed and _canUseKeyboardInput()


def isKeyReleased() -> bool:
    r"""
    \brief Check if any key was released this frame.

    \return True if any key was released and keyboard is not blocked, False otherwise.
    """
    return _EventState.KeyReleased and _canUseKeyboardInput()


def _consumeScanEvent(
    eventMap: Dict[Tuple[Keyboard.Scan, bool, bool, bool, bool], bool],
    scan: Keyboard.Scan,
    handled: bool,
    alt: bool,
    ctrl: bool,
    shift: bool,
    system: bool,
) -> bool:
    mapScan = (scan, alt, ctrl, shift, system)
    if mapScan in eventMap:
        result = eventMap[mapScan]
        if result and handled:
            eventMap[mapScan] = False
        return result
    return False


def _consumeKeyEvent(
    keyEventMap: Dict[Tuple[Keyboard.Key, bool, bool, bool, bool], bool],
    scanEventMap: Dict[Tuple[Keyboard.Scan, bool, bool, bool, bool], bool],
    key: Keyboard.Key,
    handled: bool,
    alt: bool,
    ctrl: bool,
    shift: bool,
    system: bool,
) -> bool:
    mapKey = (key, alt, ctrl, shift, system)
    if mapKey in keyEventMap:
        result = keyEventMap[mapKey]
        if result and handled:
            keyEventMap[mapKey] = False
        return result

    scan = Keyboard.delocalize(key)
    if scan == Scan.Unknown:
        return False
    result = _consumeScanEvent(scanEventMap, scan, handled, alt, ctrl, shift, system)
    if result and handled:
        unknownKey = (Key.Unknown, alt, ctrl, shift, system)
        if unknownKey in keyEventMap:
            keyEventMap[unknownKey] = False
    return result


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
        return _consumeKeyEvent(
            _EventState.KeyPressedMap,
            _EventState.KeyboardScanPressedMap,
            key,
            handled,
            alt,
            ctrl,
            shift,
            system,
        )


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
        return _consumeScanEvent(_EventState.KeyboardScanPressedMap, scan, handled, alt, ctrl, shift, system)


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
        return _consumeKeyEvent(
            _EventState.KeyReleasedMap,
            _EventState.KeyboardScanReleasedMap,
            key,
            handled,
            alt,
            ctrl,
            shift,
            system,
        )


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
        return _consumeScanEvent(_EventState.KeyboardScanReleasedMap, scan, handled, alt, ctrl, shift, system)


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
        if not _canUseKeyboardInput():
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
                if isAnyJoystickButtonTriggered(
                    key, handled=handled, repeatDelay=repeatDelay, repeatInterval=repeatInterval
                ):
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


def isActionHeld(
    actionKeys: List[
        Union[
            Key,
            Scan,
            JoystickButton,
            Tuple[JoystickAxis, float, Callable[[float, float], bool]],
        ]
    ],
) -> bool:
    r"""
    \brief Check whether any key in an action key set is currently held.

    - actionKeys: List of keys/buttons/axes to check.

    \return True if any action key is held, False otherwise.
    """
    with _StateLock:
        for key in actionKeys:
            if isinstance(key, Key):
                if _canUseKeyboardInput() and Keyboard.isKeyPressed(key):
                    return True
            if isinstance(key, Scan):
                if _canUseKeyboardInput() and Keyboard.isKeyPressed(key):
                    return True
            if isinstance(key, JoystickButton):
                if not _EventState.JoystickBlocked and _isAnyJoystickButtonDown(key.value):
                    return True
            if isinstance(key, tuple):
                axis, threshold, condition = key
                if _EventState.JoystickBlocked:
                    continue
                if isinstance(axis, JoystickAxis):
                    for _, axisMap in _EventState.JoystickAxisStatus.items():
                        if axis in axisMap:
                            position = axisMap[axis]
                            if not Math.IsNearZero(position) and condition(position, threshold):
                                return True
        return False


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
