# -*- encoding: utf-8 -*-
r"""
\brief Input polling and event system.

Provides a stateful, per-frame input abstraction over SFML's event system.
Supports keyboard, mouse, and gamepad input with action mappings,
trigger/hold detection, and input blocking.
"""

import copy
import logging
import traceback
from enum import Enum
from typing import Any, Dict, Optional, Tuple, Union, List, Callable
from . import Keyboard, Mouse, Joystick, WindowBase, Vector2i
from .Utils import Math


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

    JoystickButtonPressed: bool = False  # Whether any joystick button was pressed this frame
    JoystickButtonReleased: bool = False  # Whether any joystick button was released this frame
    JoystickAxisMoved: bool = False  # Whether any joystick axis was moved this frame
    JoystickConnected: bool = False  # Whether any joystick was connected this frame
    JoystickDisconnected: bool = False  # Whether any joystick was disconnected this frame
    JoystickButtonPressedMap: Dict[int, Dict[int, bool]] = {}  # Map of joystickId -> (button -> pressed)
    JoystickButtonReleasedMap: Dict[int, Dict[int, bool]] = {}  # Map of joystickId -> (button -> released)
    JoystickAxisMovedMap: Dict[int, Dict[Joystick.Axis, float]] = {}  # Map of joystickId -> (axis -> position)
    JoystickAxisStatus: Dict[int, Dict[Joystick.Axis, float]] = {}  # Map of joystickId -> (axis -> current_position)
    JoystickLastDominantAxis: Dict[int, Tuple[Joystick.Axis, float]] = {}  # joyId -> (Axis, Value)
    JoystickAxisJustPressed: Dict[Tuple[int, Joystick.Axis], float] = {}  # (joyId, Axis) -> Value

    EnteredText: str = ""  # Text entered this frame

    KeyboardBlocked: bool = False  # Whether keyboard input is blocked
    MouseBlocked: bool = False  # Whether mouse input is blocked
    JoystickBlocked: bool = False  # Whether joystick input is blocked

    ActionMappings: Dict[
        Tuple[str, List[Union[Key, Scan, JoystickButton, JoystickAxis]]],
        Tuple[object, Callable[[object, Optional[float]], None], bool],
    ] = {}  # Action name -> (object, callback, trigger_on_hold)


_InjectedEvents = []
_UseInjectedMouseOnly: bool = False


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

    _EventState.JoystickButtonPressed = False
    _EventState.JoystickButtonReleased = False
    _EventState.JoystickAxisMoved = False
    _EventState.JoystickConnected = False
    _EventState.JoystickDisconnected = False
    _EventState.JoystickButtonPressedMap.clear()
    _EventState.JoystickButtonReleasedMap.clear()
    _EventState.JoystickAxisMovedMap.clear()
    _EventState.JoystickAxisJustPressed.clear()

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
                    _EventState.MouseScrolledWheelPosition = mouseWheelEvent.position
                if event.isMouseButtonPressed():
                    _EventState.MouseButtonPressed = True
                    mouseButtonEvent = event.getIfMouseButtonPressed()
                    _EventState.MouseButtonPressedMap[mouseButtonEvent.button] = True
                    _EventState.MousePressedPosition = mouseButtonEvent.position
                    if not mouseButtonEvent.button in _EventState.MouseButtonTriggeredMap:
                        _EventState.MouseButtonTriggeredMap[mouseButtonEvent.button] = (0, False)
                    count, handled = _EventState.MouseButtonTriggeredMap[mouseButtonEvent.button]
                    count += 1
                    _EventState.MouseButtonTriggeredMap[mouseButtonEvent.button] = (count, handled)
                if event.isMouseButtonReleased():
                    _EventState.MouseButtonReleased = True
                    mouseButtonEvent = event.getIfMouseButtonReleased()
                    _EventState.MouseButtonReleasedMap[mouseButtonEvent.button] = True
                    _EventState.MouseReleasedPosition = mouseButtonEvent.position
                    if mouseButtonEvent.button in _EventState.MouseButtonTriggeredMap:
                        _EventState.MouseButtonTriggeredMap.pop(mouseButtonEvent.button, None)
                if event.isMouseMoved():
                    _EventState.MouseMoved = True
                    mouseMoveEvent = event.getIfMouseMoved()
                    lastPosition: Vector2i = copy.copy(_EventState.MousePosition)
                    _EventState.MousePosition = mouseMoveEvent.position
                    if mouseMoveEvent.position != lastPosition:
                        _EventState.MouseMovedDelta = mouseMoveEvent.position - lastPosition
                if event.isMouseEntered():
                    _EventState.MouseEntered = True
                if event.isMouseLeft():
                    _EventState.MouseLeft = True
            if event.isJoystickButtonPressed():
                _EventState.JoystickButtonPressed = True
                joystickButtonEvent = event.getIfJoystickButtonPressed()
                if joystickButtonEvent.joystickId not in _EventState.JoystickButtonPressedMap:
                    _EventState.JoystickButtonPressedMap[joystickButtonEvent.joystickId] = {}
                _EventState.JoystickButtonPressedMap[joystickButtonEvent.joystickId][joystickButtonEvent.button] = True
            if event.isJoystickButtonReleased():
                _EventState.JoystickButtonReleased = True
                joystickButtonEvent = event.getIfJoystickButtonReleased()
                if joystickButtonEvent.joystickId not in _EventState.JoystickButtonReleasedMap:
                    _EventState.JoystickButtonReleasedMap[joystickButtonEvent.joystickId] = {}
                _EventState.JoystickButtonReleasedMap[joystickButtonEvent.joystickId][joystickButtonEvent.button] = True
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
            polledMousePosition = Mouse.getPosition(window)
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
                if maxAxis is not None:
                    _EventState.JoystickAxisJustPressed[(joyId, maxAxis)] = axes[maxAxis]
                _EventState.JoystickLastDominantAxis[joyId] = (maxAxis, maxVal)
            elif maxAxis is not None:
                _EventState.JoystickLastDominantAxis[joyId] = (maxAxis, axes[maxAxis])

        newInputType = None
        if _EventState.MouseMoved or _EventState.MouseButtonPressed or _EventState.MouseWheelScrolled:
            newInputType = InputType.Mouse
        elif (
            _EventState.KeyPressed or _EventState.JoystickButtonPressed or len(_EventState.JoystickAxisJustPressed) > 0
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
) -> bool:
    r"""
    \brief Check if a key was triggered (first press) this frame.

    - key: The key to check.
    - alt: Whether ALT modifier is required.
    - ctrl: Whether CTRL modifier is required.
    - shift: Whether SHIFT modifier is required.
    - system: Whether SYSTEM modifier is required.
    - handled: Whether to mark the key as handled if triggered.

    \return True if the key was triggered this frame, False otherwise.
    """
    if not isKeyPressed():
        return False
    keyMap = (key, alt, ctrl, shift, system)
    if not keyMap in _EventState.KeyTriggeredMap:
        return False
    count, handled_ = _EventState.KeyTriggeredMap[keyMap]
    result = count == 1 and not handled_
    if result and handled:
        handled_ = True
        _EventState.KeyTriggeredMap[keyMap] = (count, handled_)
    return result


def isAnyJoystickButtonTriggered(button: Union[int, JoystickButton], handled: bool = False) -> bool:
    r"""
    \brief Check if any joystick button was triggered (first press) this frame.

    - button: The button to check.
    - handled: Whether to mark the button as handled if triggered.

    \return True if the button was triggered this frame, False otherwise.
    """
    if not isJoystickButtonPressed():
        return False
    if isinstance(button, JoystickButton):
        button = button.value
    triggered = False
    for joyId, buttons in _EventState.JoystickButtonPressedMap.items():
        if button in buttons and buttons[button]:
            triggered = True
            if handled:
                buttons[button] = False
    return triggered


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
) -> bool:
    r"""
    \brief Check whether any key in an action key set was triggered this frame.

    - actionKeys: List of keys/buttons/axes to check.
    - handled: Whether to mark the action as handled if triggered.

    \return True if any action key was triggered this frame, False otherwise.
    """
    triggered = False
    for key in actionKeys:
        if isinstance(key, (Key, Scan)):
            if isKeyTriggered(key, handled=handled):
                triggered = True
        elif isinstance(key, JoystickButton):
            if isAnyJoystickButtonTriggered(key, handled=handled):
                triggered = True
        elif isinstance(key, tuple):
            axis, threshold, condition = key
            triggered_smart = False
            for (joyId, pressedAxis), pos in _EventState.JoystickAxisJustPressed.items():
                if pressedAxis == axis and condition(pos, threshold):
                    triggered_smart = True
                    break

            if triggered_smart:
                triggered = True
            elif isJoystickAxisMoved():
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
    \brief Check if a mouse button was triggered (first press) this frame.

    - button: The button to check.
    - handled: Whether to mark the button as handled if triggered.

    \return True if the button was triggered this frame, False otherwise.
    """
    if not isMouseButtonPressed():
        return False
    if not button in _EventState.MouseButtonTriggeredMap:
        return False
    count, handled_ = _EventState.MouseButtonTriggeredMap[button]
    result = count == 1 and not handled_
    if result and handled:
        handled_ = True
        _EventState.MouseButtonTriggeredMap[button] = (count, handled_)
    return result


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
    \brief Block all input (keyboard, mouse, joystick).
    """
    _EventState.KeyboardBlocked = True
    _EventState.MouseBlocked = True
    _EventState.JoystickBlocked = True


def unblockInput() -> None:
    r"""
    \brief Unblock all input (keyboard, mouse, joystick).
    """
    _EventState.KeyboardBlocked = False
    _EventState.MouseBlocked = False
    _EventState.JoystickBlocked = False


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
