# -*- encoding: utf-8 -*-

import copy
import logging
from enum import Enum
from typing import Any, Dict, Optional, Tuple, Union, List, Callable

from .Utils import Math
from .pysf import Keyboard, Mouse, Joystick, WindowBase, Vector2i


class JoystickButton(Enum):
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


Key = Keyboard.Key
Scan = Keyboard.Scan
JoystickAxis = Joystick.Axis
KeyName: Dict[Key, str] = {member: member.name for member in Key.__members__.values()}
ScanName: Dict[Scan, str] = {member: member.name for member in Scan.__members__.values()}
JoyStickButtonName: Dict[JoystickButton, str] = {member: member.name for member in JoystickButton.__members__.values()}
JoystickAxisName: Dict[JoystickAxis, str] = {member: member.name for member in JoystickAxis.__members__.values()}


class _EventState:
    Focused: bool = True
    FocusLost: bool = False
    FocusGained: bool = False

    KeyPressed: bool = False
    KeyReleased: bool = False
    KeyPressedMap: Dict[Tuple[Keyboard.Key, bool, bool, bool, bool], bool] = {}
    KeyReleasedMap: Dict[Tuple[Keyboard.Key, bool, bool, bool, bool], bool] = {}
    KeyboardScanPressedMap: Dict[Tuple[Keyboard.Scan, bool, bool, bool, bool], bool] = {}
    KeyboardScanReleasedMap: Dict[Tuple[Keyboard.Scan, bool, bool, bool, bool], bool] = {}
    KeyTriggeredMap: Dict[Tuple[Keyboard.Key, bool, bool, bool, bool], Tuple[int, bool]] = {}

    MouseWheelScrolled: bool = False
    MouseScrolledWheel: Optional[Mouse.Wheel] = None
    MouseScrolledWheelDelta: float = 0.0
    MouseScrolledWheelPosition: Optional[Vector2i] = None

    MouseButtonPressed: bool = False
    MouseButtonReleased: bool = False
    MouseButtonPressedMap: Dict[Mouse.Button, bool] = {}
    MouseButtonReleasedMap: Dict[Mouse.Button, bool] = {}
    MousePressedPosition: Optional[Vector2i] = None
    MouseReleasedPosition: Optional[Vector2i] = None
    MouseButtonTriggeredMap: Dict[Mouse.Button, Tuple[int, bool]] = {}

    MouseMoved: bool = False
    MousePosition: Vector2i = Vector2i(0, 0)
    MouseMovedDelta: Optional[Vector2i] = None

    MouseEntered: bool = False
    MouseLeft: bool = False

    JoystickButtonPressed: bool = False
    JoystickButtonReleased: bool = False
    JoystickAxisMoved: bool = False
    JoystickConnected: bool = False
    JoystickDisconnected: bool = False
    JoystickButtonPressedMap: Dict[int, Dict[int, bool]] = {}
    JoystickButtonReleasedMap: Dict[int, Dict[int, bool]] = {}
    JoystickAxisMovedMap: Dict[int, Tuple[Joystick.Axis, float]] = {}
    JoystickAxisStatus: Dict[int, Dict[Joystick.Axis, float]] = {}

    EnteredText: str = ""

    KeyboardBlocked: bool = False
    MouseBlocked: bool = False
    JoystickBlocked: bool = False

    ActionMappings: Dict[
        Tuple[str, List[Union[Key, Scan, JoystickButton, JoystickAxis]]],
        Tuple[object, Callable[[object, Optional[float]], None], bool],
    ] = {}


def update(window: WindowBase) -> None:
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

    _EventState.EnteredText = ""

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
                _EventState.JoystickAxisMovedMap[joystickMoveEvent.joystickId] = (
                    joystickMoveEvent.axis,
                    joystickMoveEvent.position,
                )
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

        if _EventState.EnteredText == "\x16":
            from . import Clipboard

            _EventState.EnteredText = Clipboard.getString()

        moveActions: List[Tuple[Joystick.Axis, float, Callable, List[Any]]] = []
        for actionType, callables in _EventState.ActionMappings.items():
            _, actionKeysTuple = actionType
            if len(callables) == 2:
                obj, objCallable = callables
                triggerOnHold = False
            else:
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
        finalMoveAction: Tuple[Callable, List[Any]] = None
        maxPosition = 0.0
        for axis, position, objCallable, params in moveActions:
            if position != 0 and abs(position) > maxPosition:
                maxPosition = abs(position)
                finalMoveAction = (objCallable, params)
        if finalMoveAction:
            callable_, params = finalMoveAction
            callable_(*params)

    except Exception as e:
        logging.error(f"Error in Input.update: {e}")


def isFocused() -> bool:
    return _EventState.Focused


def isFocusLost() -> bool:
    return _EventState.FocusLost


def isFocusGained() -> bool:
    return _EventState.FocusGained


def isKeyPressed() -> bool:
    return _EventState.KeyPressed and not _EventState.KeyboardBlocked


def isKeyReleased() -> bool:
    return _EventState.KeyReleased and not _EventState.KeyboardBlocked


def getKeyPressed(
    key: Keyboard.Key,
    handled: bool,
    alt: bool = False,
    ctrl: bool = False,
    shift: bool = False,
    system: bool = False,
) -> bool:
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
    return _EventState.MouseWheelScrolled and not _EventState.MouseBlocked


def getMouseScrolledWheel() -> Optional[Mouse.Wheel]:
    if not isMouseWheelScrolled():
        return None
    return _EventState.MouseScrolledWheel


def getMouseScrolledWheelDelta() -> float:
    if not isMouseWheelScrolled():
        return 0.0
    return _EventState.MouseScrolledWheelDelta


def getMouseScrolledWheelPosition() -> Optional[Vector2i]:
    if not isMouseWheelScrolled():
        return None
    return _EventState.MouseScrolledWheelPosition


def isMouseButtonPressed() -> bool:
    return _EventState.MouseButtonPressed and not _EventState.MouseBlocked


def isMouseButtonReleased() -> bool:
    return _EventState.MouseButtonReleased and not _EventState.MouseBlocked


def getMouseButtonPressed(button: Mouse.Button, handled: bool) -> bool:
    if not isMouseButtonPressed():
        return False
    if button in _EventState.MouseButtonPressedMap:
        result = _EventState.MouseButtonPressedMap[button]
        if result and handled:
            _EventState.MouseButtonPressedMap[button] = False
        return result
    return False


def getMouseButtonReleased(button: Mouse.Button, handled: bool) -> bool:
    if not isMouseButtonReleased():
        return False
    if button in _EventState.MouseButtonReleasedMap:
        result = _EventState.MouseButtonReleasedMap[button]
        if result and handled:
            _EventState.MouseButtonReleasedMap[button] = False
        return result
    return False


def isMouseMoved() -> bool:
    return _EventState.MouseMoved and not _EventState.MouseBlocked


def getMousePosition() -> Vector2i:
    return _EventState.MousePosition


def getMouseMovedDelta() -> Optional[Vector2i]:
    if not isMouseMoved():
        return None
    return _EventState.MouseMovedDelta


def isMouseEntered() -> bool:
    return _EventState.MouseEntered and not _EventState.MouseBlocked


def isMouseLeft() -> bool:
    return _EventState.MouseLeft and not _EventState.MouseBlocked


def isJoystickButtonPressed() -> bool:
    return _EventState.JoystickButtonPressed and not _EventState.JoystickBlocked


def isJoystickButtonReleased() -> bool:
    return _EventState.JoystickButtonReleased and not _EventState.JoystickBlocked


def getJoystickButtonPressed(joystickId: int, button: Union[int, JoystickButton], handled: bool) -> bool:
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
    return _EventState.JoystickAxisMoved and not _EventState.JoystickBlocked


def getJoystickAxisMoved(joystickId: int, handled: bool) -> Optional[Tuple[Joystick.Axis, float]]:
    if not isJoystickAxisMoved():
        return None
    if joystickId in _EventState.JoystickAxisMovedMap:
        result = _EventState.JoystickAxisMovedMap[joystickId]
        if result and handled:
            _EventState.JoystickAxisMovedMap[joystickId] = None
        return result
    return None


def isJoystickConnected() -> bool:
    return _EventState.JoystickConnected and not _EventState.JoystickBlocked


def isJoystickDisconnected() -> bool:
    return _EventState.JoystickDisconnected and not _EventState.JoystickBlocked


def isKeyTriggered(
    key: Keyboard.Key,
    alt: bool = False,
    ctrl: bool = False,
    shift: bool = False,
    system: bool = False,
    handled: bool = False,
) -> bool:
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


def isMouseButtonTriggered(
    button: Mouse.Button,
    handled: bool = False,
) -> bool:
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


def isTextEntered() -> bool:
    return _EventState.EnteredText != ""


def getEnteredText() -> str:
    return _EventState.EnteredText


def isTextEntered() -> bool:
    return (len(_EventState.EnteredText) > 0) and not _EventState.KeyboardBlocked


def isKeyboardBlocked() -> bool:
    return _EventState.KeyboardBlocked


def isMouseBlocked() -> bool:
    return _EventState.MouseBlocked


def isJoystickBlocked() -> bool:
    return _EventState.JoystickBlocked


def blockKeyboard() -> None:
    _EventState.KeyboardBlocked = True


def blockMouse() -> None:
    _EventState.MouseBlocked = True


def blockJoystick() -> None:
    _EventState.JoystickBlocked = True


def unblockKeyboard() -> None:
    _EventState.KeyboardBlocked = False


def unblockMouse() -> None:
    _EventState.MouseBlocked = False


def unblockJoystick() -> None:
    _EventState.JoystickBlocked = False


def blockInput() -> None:
    _EventState.KeyboardBlocked = True
    _EventState.MouseBlocked = True
    _EventState.JoystickBlocked = True


def unblockInput() -> None:
    _EventState.KeyboardBlocked = False
    _EventState.MouseBlocked = False
    _EventState.JoystickBlocked = False


def getConfirmKeys() -> (
    List[Union[Key, Scan, JoystickButton, Tuple[JoystickAxis, float, Callable[[float, float], None]]]]
):
    return (Key.Enter, Key.Space, Scan.Enter, Scan.Space, JoystickButton.A)


def getCancelKeys() -> (
    List[Union[Key, Scan, JoystickButton, Tuple[JoystickAxis, float, Callable[[float, float], None]]]]
):
    return (Key.Escape, Scan.Escape, JoystickButton.B)


def getUpKeys() -> List[Union[Key, Scan, JoystickButton, Tuple[JoystickAxis, float, Callable[[float, float], None]]]]:
    return (Key.Up, Scan.Up, (Joystick.Axis.Y, -10.0, float.__lt__))


def getDownKeys() -> List[Union[Key, Scan, JoystickButton, Tuple[JoystickAxis, float, Callable[[float, float], None]]]]:
    return (Key.Down, Scan.Down, (Joystick.Axis.Y, 10.0, float.__gt__))


def getLeftKeys() -> List[Union[Key, Scan, JoystickButton, Tuple[JoystickAxis, float, Callable[[float, float], None]]]]:
    return (Key.Left, Scan.Left, (Joystick.Axis.X, -10.0, float.__lt__))


def getRightKeys() -> (
    List[Union[Key, Scan, JoystickButton, Tuple[JoystickAxis, float, Callable[[float, float], None]]]]
):
    return (Key.Right, Scan.Right, (Joystick.Axis.X, 10.0, float.__gt__))


def registerActionMapping(
    obj: object,
    actionName: str,
    actionKeys: Tuple[Union[Key, Scan, JoystickButton, Tuple[JoystickAxis, float, Callable[[float, float], None]]]],
    callable_: Callable[[object, Optional[float]], None],
    triggerOnHold: bool = False,
) -> None:
    _EventState.ActionMappings[(actionName, actionKeys)] = (obj, callable_, triggerOnHold)


def unregisterActionMapping(obj: object, actionName: str) -> None:
    toRemove = [k for k, v in _EventState.ActionMappings.items() if k[0] == actionName and v[0] == obj]
    for k in toRemove:
        _EventState.ActionMappings.pop(k, None)
