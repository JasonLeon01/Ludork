# -*- encoding: utf-8 -*-

from PyQt5 import QtWidgets, QtCore, QtGui


class GamePanel(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(QtCore.Qt.WA_OpaquePaintEvent)
        self.setAttribute(QtCore.Qt.WA_NoSystemBackground)
        self.setFocusPolicy(QtCore.Qt.StrongFocus)
        self.setMouseTracking(True)
        self._engineProc = None
        self._keyMap = self._initKeyMap()

    def setEngineProcess(self, proc):
        self._engineProc = proc

    def _initKeyMap(self):
        m = {}
        Qt = QtCore.Qt
        for i in range(26):
            m[Qt.Key_A + i] = chr(ord("A") + i)
        for i in range(10):
            m[Qt.Key_0 + i] = f"Num{i}"
        for i in range(12):
            m[Qt.Key_F1 + i] = f"F{i+1}"

        m.update(
            {
                Qt.Key_Escape: "Escape",
                Qt.Key_Control: "LControl",
                Qt.Key_Shift: "LShift",
                Qt.Key_Alt: "LAlt",
                Qt.Key_Meta: "LSystem",
                Qt.Key_Return: "Enter",
                Qt.Key_Enter: "Enter",
                Qt.Key_Backspace: "Backspace",
                Qt.Key_Tab: "Tab",
                Qt.Key_Space: "Space",
                Qt.Key_Left: "Left",
                Qt.Key_Right: "Right",
                Qt.Key_Up: "Up",
                Qt.Key_Down: "Down",
                Qt.Key_PageUp: "PageUp",
                Qt.Key_PageDown: "PageDown",
                Qt.Key_Home: "Home",
                Qt.Key_End: "End",
                Qt.Key_Insert: "Insert",
                Qt.Key_Delete: "Delete",
                Qt.Key_Plus: "Add",
                Qt.Key_Minus: "Subtract",
                Qt.Key_Asterisk: "Multiply",
                Qt.Key_Slash: "Divide",
                Qt.Key_Backslash: "Backslash",
                Qt.Key_Comma: "Comma",
                Qt.Key_Period: "Period",
                Qt.Key_QuoteLeft: "Quote",
                Qt.Key_Semicolon: "Semicolon",
                Qt.Key_BracketLeft: "LBracket",
                Qt.Key_BracketRight: "RBracket",
            }
        )
        return m

    def _sendEvent(self, eventData):
        if self._engineProc and self._engineProc.poll() is None:
            try:
                msg = repr(eventData)
                cmd = f"Engine.Input.injectEvent({msg})\n"
                self._engineProc.stdin.write(cmd)
                self._engineProc.stdin.flush()
            except Exception as e:
                pass

    def keyPressEvent(self, event: QtGui.QKeyEvent):
        keyName = self._keyMap.get(event.key())
        if keyName:
            data = {
                "type": "KeyPressed",
                "key": keyName,
                "alt": bool(event.modifiers() & QtCore.Qt.AltModifier),
                "control": bool(event.modifiers() & QtCore.Qt.ControlModifier),
                "shift": bool(event.modifiers() & QtCore.Qt.ShiftModifier),
                "system": bool(event.modifiers() & QtCore.Qt.MetaModifier),
            }
            self._sendEvent(data)

    def keyReleaseEvent(self, event: QtGui.QKeyEvent):
        keyName = self._keyMap.get(event.key())
        if keyName:
            data = {
                "type": "KeyReleased",
                "key": keyName,
                "alt": bool(event.modifiers() & QtCore.Qt.AltModifier),
                "control": bool(event.modifiers() & QtCore.Qt.ControlModifier),
                "shift": bool(event.modifiers() & QtCore.Qt.ShiftModifier),
                "system": bool(event.modifiers() & QtCore.Qt.MetaModifier),
            }
            self._sendEvent(data)

    def mousePressEvent(self, event: QtGui.QMouseEvent):
        btn = "Left"
        if event.button() == QtCore.Qt.RightButton:
            btn = "Right"
        elif event.button() == QtCore.Qt.MiddleButton:
            btn = "Middle"
        elif event.button() == QtCore.Qt.XButton1:
            btn = "XButton1"
        elif event.button() == QtCore.Qt.XButton2:
            btn = "XButton2"

        data = {"type": "MouseButtonPressed", "button": btn, "x": int(event.x()), "y": int(event.y())}
        self._sendEvent(data)

    def mouseReleaseEvent(self, event: QtGui.QMouseEvent):
        btn = "Left"
        if event.button() == QtCore.Qt.RightButton:
            btn = "Right"
        elif event.button() == QtCore.Qt.MiddleButton:
            btn = "Middle"
        elif event.button() == QtCore.Qt.XButton1:
            btn = "XButton1"
        elif event.button() == QtCore.Qt.XButton2:
            btn = "XButton2"

        data = {"type": "MouseButtonReleased", "button": btn, "x": int(event.x()), "y": int(event.y())}
        self._sendEvent(data)

    def mouseMoveEvent(self, event: QtGui.QMouseEvent):
        data = {"type": "MouseMoved", "x": int(event.x()), "y": int(event.y())}
        self._sendEvent(data)

    def wheelEvent(self, event: QtGui.QWheelEvent):
        delta = event.angleDelta().y() / 120.0
        data = {"type": "MouseWheelScrolled", "delta": delta, "x": int(event.x()), "y": int(event.y())}
        self._sendEvent(data)

    def focusInEvent(self, event: QtGui.QFocusEvent):
        self._sendEvent({"type": "FocusGained"})

    def focusOutEvent(self, event: QtGui.QFocusEvent):
        self._sendEvent({"type": "FocusLost"})
