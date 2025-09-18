# -*- encoding: utf-8 -*-

import copy
from typing import Dict, Optional, Tuple
from . import GetGameRunning, Keyboard, Mouse, Joystick, WindowBase, Vector2i


class Input:
    _Focused: bool = True
    _FocusLost: bool = False
    _FocusGained: bool = False

    _KeyPressed: bool = False
    _KeyReleased: bool = False
    _KeyPressedMap: Dict[Tuple[Keyboard.Key, bool, bool, bool, bool], bool] = {}
    _KeyReleasedMap: Dict[Tuple[Keyboard.Key, bool, bool, bool, bool], bool] = {}
    _KeyboardScanPressedMap: Dict[Tuple[Keyboard.Scan, bool, bool, bool, bool], bool] = {}
    _KeyboardScanReleasedMap: Dict[Tuple[Keyboard.Scan, bool, bool, bool, bool], bool] = {}
    _KeyTriggeredMap: Dict[Tuple[Keyboard.Key, bool, bool, bool, bool], Tuple[int, bool]] = {}

    _MouseWheelScrolled: bool = False
    _MouseScrolledWheel: Optional[Mouse.Wheel] = None
    _MouseScrolledWheelDelta: float = 0.0
    _MouseScrolledWheelPosition: Optional[Vector2i] = None

    _MouseButtonPressed: bool = False
    _MouseButtonReleased: bool = False
    _MouseButtonPressedMap: Dict[Mouse.Button, bool] = {}
    _MouseButtonReleasedMap: Dict[Mouse.Button, bool] = {}
    _MousePressedPosition: Optional[Vector2i] = None
    _MouseReleasedPosition: Optional[Vector2i] = None
    _MouseButtonTriggeredMap: Dict[Mouse.Button, Tuple[int, bool]] = {}

    _MouseMoved: bool = False
    _MousePosition: Vector2i = Vector2i(0, 0)
    _MouseMovedDelta: Optional[Vector2i] = None

    _MouseEntered: bool = False
    _MouseLeft: bool = False

    _JoystickButtonPressed: bool = False
    _JoystickButtonReleased: bool = False
    _JoystickAxisMoved: bool = False
    _JoystickConnected: bool = False
    _JoystickDisconnected: bool = False
    _JoystickButtonPressedMap: Dict[int, Dict[int, bool]] = {}
    _JoystickButtonReleasedMap: Dict[int, Dict[int, bool]] = {}
    _JoystickAxisMovedMap: Dict[int, Tuple[Joystick.Axis, float]] = {}

    _KeyboardBlocked: bool = False
    _MouseBlocked: bool = False
    _JoystickBlocked: bool = False

    @classmethod
    def update(cls, window: WindowBase) -> None:
        cls._FocusLost = False
        cls._FocusGained = False

        cls._KeyPressed = False
        cls._KeyReleased = False
        cls._KeyPressedMap.clear()
        cls._KeyReleasedMap.clear()
        cls._KeyboardScanPressedMap.clear()
        cls._KeyboardScanReleasedMap.clear()

        cls._MouseWheelScrolled = False
        cls._MouseScrolledWheel = None
        cls._MouseScrolledWheelDelta = 0.0
        cls._MouseScrolledWheelPosition = None

        cls._MouseButtonPressed = False
        cls._MouseButtonReleased = False
        cls._MouseButtonPressedMap.clear()
        cls._MouseButtonReleasedMap.clear()
        cls._MousePressedPosition = None
        cls._MouseReleasedPosition = None

        cls._MouseMoved = False
        cls._MouseMovedDelta = None

        cls._MouseEntered = False
        cls._MouseLeft = False

        cls._JoystickButtonPressed = False
        cls._JoystickButtonReleased = False
        cls._JoystickAxisMoved = False
        cls._JoystickConnected = False
        cls._JoystickDisconnected = False
        cls._JoystickButtonPressedMap.clear()
        cls._JoystickButtonReleasedMap.clear()
        cls._JoystickAxisMovedMap.clear()

        try:
            while GetGameRunning():
                event = window.pollEvent()
                if event is None:
                    break
                if event.isClosed():
                    window.close()
                if event.isFocusLost():
                    cls._Focused = False
                    cls._FocusLost = True
                if event.isFocusGained():
                    cls._Focused = True
                    cls._FocusGained = True
                if event.isKeyPressed():
                    cls._KeyPressed = True
                    keyEvent = event.getIfKeyPressed()
                    alt = keyEvent.alt
                    ctrl = keyEvent.control
                    shift = keyEvent.shift
                    system = keyEvent.system
                    keyMap = (keyEvent.code, alt, ctrl, shift, system)
                    scanMap = (keyEvent.scancode, alt, ctrl, shift, system)
                    cls._KeyPressedMap[keyMap] = True
                    cls._KeyboardScanPressedMap[scanMap] = True
                    if not keyMap in cls._KeyTriggeredMap:
                        cls._KeyTriggeredMap[keyMap] = (0, False)
                    count, handled = cls._KeyTriggeredMap[keyMap]
                    count += 1
                    cls._KeyTriggeredMap[keyMap] = (count, handled)
                if event.isKeyReleased():
                    cls._KeyReleased = True
                    keyEvent = event.getIfKeyReleased()
                    alt = keyEvent.alt
                    ctrl = keyEvent.control
                    shift = keyEvent.shift
                    system = keyEvent.system
                    keyMap = (keyEvent.code, alt, ctrl, shift, system)
                    scanMap = (keyEvent.scancode, alt, ctrl, shift, system)
                    cls._KeyReleasedMap[keyMap] = True
                    cls._KeyboardScanReleasedMap[scanMap] = True
                    if keyMap in cls._KeyTriggeredMap:
                        cls._KeyTriggeredMap.pop(keyMap, None)
                if event.isMouseWheelScrolled():
                    cls._MouseWheelScrolled = True
                    mouseWheelEvent = event.getIfMouseWheelScrolled()
                    cls._MouseScrolledWheel = mouseWheelEvent.wheel
                    cls._MouseScrolledWheelDelta = mouseWheelEvent.delta
                    cls._MouseScrolledWheelPosition = mouseWheelEvent.position
                if event.isMouseButtonPressed():
                    cls._MouseButtonPressed = True
                    mouseButtonEvent = event.getIfMouseButtonPressed()
                    cls._MouseButtonPressedMap[mouseButtonEvent.button] = True
                    cls._MousePressedPosition = mouseButtonEvent.position
                    if not mouseButtonEvent.button in cls._MouseButtonTriggeredMap:
                        cls._MouseButtonTriggeredMap[mouseButtonEvent.button] = (0, False)
                    count, handled = cls._MouseButtonTriggeredMap[mouseButtonEvent.button]
                    count += 1
                    cls._MouseButtonTriggeredMap[mouseButtonEvent.button] = (count, handled)
                if event.isMouseButtonReleased():
                    cls._MouseButtonReleased = True
                    mouseButtonEvent = event.getIfMouseButtonReleased()
                    cls._MouseButtonReleasedMap[mouseButtonEvent.button] = True
                    cls._MouseReleasedPosition = mouseButtonEvent.position
                    if mouseButtonEvent.button in cls._MouseButtonTriggeredMap:
                        cls._MouseButtonTriggeredMap.pop(mouseButtonEvent.button, None)
                if event.isMouseMoved():
                    cls._MouseMoved = True
                    mouseMoveEvent = event.getIfMouseMoved()
                    lastPosition: Vector2i = copy.copy(cls._MousePosition)
                    cls._MousePosition = mouseMoveEvent.position
                    if mouseMoveEvent.position != lastPosition:
                        cls._MouseMovedDelta = mouseMoveEvent.position - lastPosition
                if event.isMouseEntered():
                    cls._MouseEntered = True
                if event.isMouseLeft():
                    cls._MouseLeft = True
                if event.isJoystickButtonPressed():
                    cls._JoystickButtonPressed = True
                    joystickButtonEvent = event.getIfJoystickButtonPressed()
                    if joystickButtonEvent.joystickId not in cls._JoystickButtonPressedMap:
                        cls._JoystickButtonPressedMap[joystickButtonEvent.joystickId] = {}
                    cls._JoystickButtonPressedMap[joystickButtonEvent.joystickId][joystickButtonEvent.button] = True
                if event.isJoystickButtonReleased():
                    cls._JoystickButtonReleased = True
                    joystickButtonEvent = event.getIfJoystickButtonReleased()
                    if joystickButtonEvent.joystickId not in cls._JoystickButtonReleasedMap:
                        cls._JoystickButtonReleasedMap[joystickButtonEvent.joystickId] = {}
                    cls._JoystickButtonReleasedMap[joystickButtonEvent.joystickId][joystickButtonEvent.button] = True
                if event.isJoystickMoved():
                    cls._JoystickAxisMoved = True
                    joystickMoveEvent = event.getIfJoystickMoved()
                    cls._JoystickAxisMovedMap[joystickMoveEvent.joystickId] = (
                        joystickMoveEvent.axis,
                        joystickMoveEvent.position,
                    )
                if event.isJoystickConnected():
                    cls._JoystickConnected = True
                if event.isJoystickDisconnected():
                    cls._JoystickDisconnected = True

        except Exception as e:
            print(f"Error in Input.update: {e}")

    @classmethod
    def isFocused(cls) -> bool:
        return cls._Focused

    @classmethod
    def isFocusLost(cls) -> bool:
        return cls._FocusLost

    @classmethod
    def isFocusGained(cls) -> bool:
        return cls._FocusGained

    @classmethod
    def isKeyPressed(cls) -> bool:
        return cls._KeyPressed and not cls._KeyboardBlocked

    @classmethod
    def isKeyReleased(cls) -> bool:
        return cls._KeyReleased and not cls._KeyboardBlocked

    @classmethod
    def getKeyPressed(
        cls,
        key: Keyboard.Key,
        handled: bool,
        alt: bool = False,
        ctrl: bool = False,
        shift: bool = False,
        system: bool = False,
    ) -> bool:
        if not cls.isKeyPressed():
            return False
        mapKey = (key, alt, ctrl, shift, system)
        if mapKey in cls._KeyPressedMap:
            result = cls._KeyPressedMap[mapKey]
            if result and handled:
                cls._KeyPressedMap[mapKey] = False
            return result
        return False

    @classmethod
    def getScanPressed(
        cls,
        scan: Keyboard.Scan,
        handled: bool,
        alt: bool = False,
        ctrl: bool = False,
        shift: bool = False,
        system: bool = False,
    ) -> bool:
        if not cls.isKeyPressed():
            return False
        mapScan = (scan, alt, ctrl, shift, system)
        if mapScan in cls._KeyboardScanPressedMap:
            result = cls._KeyboardScanPressedMap[mapScan]
            if result and handled:
                cls._KeyboardScanPressedMap[mapScan] = False
            return result
        return False

    @classmethod
    def getKeyReleased(
        cls,
        key: Keyboard.Key,
        handled: bool,
        alt: bool = False,
        ctrl: bool = False,
        shift: bool = False,
        system: bool = False,
    ) -> bool:
        if not cls.isKeyReleased():
            return False
        mapKey = (key, alt, ctrl, shift, system)
        if mapKey in cls._KeyReleasedMap:
            result = cls._KeyReleasedMap[mapKey]
            if result and handled:
                cls._KeyReleasedMap[mapKey] = False
            return result
        return False

    @classmethod
    def getScanReleased(
        cls,
        scan: Keyboard.Scan,
        handled: bool,
        alt: bool = False,
        ctrl: bool = False,
        shift: bool = False,
        system: bool = False,
    ) -> bool:
        if not cls.isKeyReleased():
            return False
        mapScan = (scan, alt, ctrl, shift, system)
        if mapScan in cls._KeyboardScanReleasedMap:
            result = cls._KeyboardScanReleasedMap[mapScan]
            if result and handled:
                cls._KeyboardScanReleasedMap[mapScan] = False
            return result
        return False

    @classmethod
    def isMouseWheelScrolled(cls) -> bool:
        return cls._MouseWheelScrolled and not cls._MouseBlocked

    @classmethod
    def getMouseScrolledWheel(cls) -> Optional[Mouse.Wheel]:
        if not cls.isMouseWheelScrolled():
            return None
        return cls._MouseScrolledWheel

    @classmethod
    def getMouseScrolledWheelDelta(cls) -> float:
        if not cls.isMouseWheelScrolled():
            return 0.0
        return cls._MouseScrolledWheelDelta

    @classmethod
    def getMouseScrolledWheelPosition(cls) -> Optional[Vector2i]:
        if not cls.isMouseWheelScrolled():
            return None
        return cls._MouseScrolledWheelPosition

    @classmethod
    def isMouseButtonPressed(cls) -> bool:
        return cls._MouseButtonPressed and not cls._MouseBlocked

    @classmethod
    def isMouseButtonReleased(cls) -> bool:
        return cls._MouseButtonReleased and not cls._MouseBlocked

    @classmethod
    def getMouseButtonPressed(cls, button: Mouse.Button, handled: bool) -> bool:
        if not cls.isMouseButtonPressed():
            return False
        if button in cls._MouseButtonPressedMap:
            result = cls._MouseButtonPressedMap[button]
            if result and handled:
                cls._MouseButtonPressedMap[button] = False
            return result
        return False

    @classmethod
    def getMouseButtonReleased(cls, button: Mouse.Button, handled: bool) -> bool:
        if not cls.isMouseButtonReleased():
            return False
        if button in cls._MouseButtonReleasedMap:
            result = cls._MouseButtonReleasedMap[button]
            if result and handled:
                cls._MouseButtonReleasedMap[button] = False
            return result
        return False

    @classmethod
    def isMouseMoved(cls) -> bool:
        return cls._MouseMoved and not cls._MouseBlocked

    @classmethod
    def getMousePosition(cls) -> Vector2i:
        return cls._MousePosition

    @classmethod
    def getMouseMovedDelta(cls) -> Optional[Vector2i]:
        if not cls.isMouseMoved():
            return None
        return cls._MouseMovedDelta

    @classmethod
    def isMouseEntered(cls) -> bool:
        return cls._MouseEntered and not cls._MouseBlocked

    @classmethod
    def isMouseLeft(cls) -> bool:
        return cls._MouseLeft and not cls._MouseBlocked

    @classmethod
    def isJoystickButtonPressed(cls) -> bool:
        return cls._JoystickButtonPressed and not cls._JoystickBlocked

    @classmethod
    def isJoystickButtonReleased(cls) -> bool:
        return cls._JoystickButtonReleased and not cls._JoystickBlocked

    @classmethod
    def getJoystickButtonPressed(cls, joystickId: int, button: int, handled: bool) -> bool:
        if not cls.isJoystickButtonPressed():
            return False
        if joystickId in cls._JoystickButtonPressedMap:
            if button in cls._JoystickButtonPressedMap[joystickId]:
                result = cls._JoystickButtonPressedMap[joystickId][button]
                if result and handled:
                    cls._JoystickButtonPressedMap[joystickId][button] = False
                return result
        return False

    @classmethod
    def getJoystickButtonReleased(cls, joystickId: int, button: int, handled: bool) -> bool:
        if not cls.isJoystickButtonReleased():
            return False
        if joystickId in cls._JoystickButtonReleasedMap:
            if button in cls._JoystickButtonReleasedMap[joystickId]:
                result = cls._JoystickButtonReleasedMap[joystickId][button]
                if result and handled:
                    cls._JoystickButtonReleasedMap[joystickId][button] = False
                return result
        return False

    @classmethod
    def isJoystickAxisMoved(cls) -> bool:
        return cls._JoystickAxisMoved and not cls._JoystickBlocked

    @classmethod
    def getJoystickAxisMoved(cls, joystickId: int, handled: bool) -> Optional[Tuple[int, float]]:
        if not cls.isJoystickAxisMoved():
            return None
        if joystickId in cls._JoystickAxisMovedMap:
            result = cls._JoystickAxisMovedMap[joystickId]
            if result and handled:
                cls._JoystickAxisMovedMap[joystickId] = None
            return result
        return None

    @classmethod
    def isJoystickConnected(cls) -> bool:
        return cls._JoystickConnected and not cls._JoystickBlocked

    @classmethod
    def isJoystickDisconnected(cls) -> bool:
        return cls._JoystickDisconnected and not cls._JoystickBlocked

    @classmethod
    def isKeyTriggered(
        cls,
        key: Keyboard.Key,
        alt: bool = False,
        ctrl: bool = False,
        shift: bool = False,
        system: bool = False,
        handled: bool = False,
    ) -> bool:
        if not cls.isKeyPressed():
            return False
        keyMap = (key, alt, ctrl, shift, system)
        if not keyMap in cls._KeyTriggeredMap:
            return False
        count, handled_ = cls._KeyTriggeredMap[keyMap]
        result = count == 1 and not handled_
        if result and handled:
            handled_ = True
            cls._KeyTriggeredMap[keyMap] = (count, handled_)
        return result

    @classmethod
    def isMouseButtonTriggered(
        cls,
        button: Mouse.Button,
        handled: bool = False,
    ) -> bool:
        if not cls.isMouseButtonPressed():
            return False
        if not button in cls._MouseButtonTriggeredMap:
            return False
        count, handled_ = cls._MouseButtonTriggeredMap[button]
        result = count == 1 and not handled_
        if result and handled:
            handled_ = True
            cls._MouseButtonTriggeredMap[button] = (count, handled_)
        return result

    @classmethod
    def isKeyboardBlocked(cls) -> bool:
        return cls._KeyboardBlocked

    @classmethod
    def isMouseBlocked(cls) -> bool:
        return cls._MouseBlocked

    @classmethod
    def isJoystickBlocked(cls) -> bool:
        return cls._JoystickBlocked

    @classmethod
    def blockKeyboard(cls) -> None:
        cls._KeyboardBlocked = True

    @classmethod
    def blockMouse(cls) -> None:
        cls._MouseBlocked = True

    @classmethod
    def blockJoystick(cls) -> None:
        cls._JoystickBlocked = True

    @classmethod
    def unblockKeyboard(cls) -> None:
        cls._KeyboardBlocked = False

    @classmethod
    def unblockMouse(cls) -> None:
        cls._MouseBlocked = False

    @classmethod
    def unblockJoystick(cls) -> None:
        cls._JoystickBlocked = False

    @classmethod
    def blockInput(cls) -> None:
        cls._KeyboardBlocked = True
        cls._MouseBlocked = True
        cls._JoystickBlocked = True

    @classmethod
    def unblockInput(cls) -> None:
        cls._KeyboardBlocked = False
        cls._MouseBlocked = False
        cls._JoystickBlocked = False
