# -*- encoding: utf-8 -*-

from __future__ import annotations

from typing import Any, Optional

from PyQt5 import QtWidgets, QtCore, QtGui


class GamePanel(QtWidgets.QWidget):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setAttribute(QtCore.Qt.WA_OpaquePaintEvent)
        self.setAttribute(QtCore.Qt.WA_NoSystemBackground)
        self.setFocusPolicy(QtCore.Qt.StrongFocus)
        self.setMouseTracking(True)
        self._engineProc = None
        self._engineCommandClient = None
        self._keyMap = self._initKeyMap()
        self._appEventFilter: Optional[QtWidgets.QApplication] = None
        self._keyboardCaptureActive = False

    def _dpr(self) -> float:
        return self.devicePixelRatioF()

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        dpr = self._dpr()
        print(f"[GamePanel] resized: logical={self.width()}x{self.height()}, dpr={dpr}, physical={int(self.width()*dpr)}x{int(self.height()*dpr)}")

    def setEngineProcess(self, proc: Any) -> None:
        self._engineProc = proc
        self._syncAppEventFilter()
        if proc is None:
            self._keyboardCaptureActive = False
            return
        self._restoreKeyboardFocus()

    def setEngineCommandClient(self, client: Any) -> None:
        self._engineCommandClient = client

    def _initKeyMap(self) -> dict[int, str]:
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

    def _hasRunningEngine(self) -> bool:
        return self._engineProc is not None and self._engineProc.poll() is None

    def _syncAppEventFilter(self) -> None:
        app = QtWidgets.QApplication.instance()
        if app is None:
            return
        if self._engineProc is None:
            if self._appEventFilter is not None:
                self._appEventFilter.removeEventFilter(self)
                self._appEventFilter = None
            return
        if self._appEventFilter is None:
            app.installEventFilter(self)
            self._appEventFilter = app

    def _sendEvent(self, eventData: dict[str, Any]) -> None:
        if self._hasRunningEngine():
            try:
                msg = repr(eventData)
                cmd = f"Engine.Input.injectEvent({msg})\n"
                if self._engineCommandClient is not None:
                    self._engineCommandClient.sendLine(cmd)
            except Exception:
                pass

    def _canForwardInputEvent(self) -> bool:
        app = QtWidgets.QApplication.instance()
        if app is None:
            return True
        if app.applicationState() != QtCore.Qt.ApplicationActive:
            return False
        activeWindow = app.activeWindow()
        if activeWindow is None:
            return False
        return activeWindow is self.window()

    def _restoreKeyboardFocus(self) -> None:
        if not self._canForwardInputEvent():
            return
        self._keyboardCaptureActive = True
        window = self.window()
        if isinstance(window, QtWidgets.QWidget):
            window.activateWindow()
        self.setFocus(QtCore.Qt.OtherFocusReason)

    def _shouldUseFallbackKeyForwarding(self, receiver: QtCore.QObject) -> bool:
        if receiver is self or not self._hasRunningEngine() or not self._canForwardInputEvent():
            return False
        if not self.isVisible():
            return False
        return self._keyboardCaptureActive or self.underMouse()

    def _shouldAcceptShortcutOverride(self, receiver: QtCore.QObject) -> bool:
        if not self._hasRunningEngine() or not self._canForwardInputEvent() or not self.isVisible():
            return False
        return receiver is self or self._shouldUseFallbackKeyForwarding(receiver)

    def _keyEventData(self, event: QtGui.QKeyEvent, eventType: str) -> Optional[dict[str, Any]]:
        keyName = self._keyMap.get(event.key())
        if not keyName:
            return None
        return {
            "type": eventType,
            "key": keyName,
            "alt": bool(event.modifiers() & QtCore.Qt.AltModifier),
            "control": bool(event.modifiers() & QtCore.Qt.ControlModifier),
            "shift": bool(event.modifiers() & QtCore.Qt.ShiftModifier),
            "system": bool(event.modifiers() & QtCore.Qt.MetaModifier),
        }

    def _forwardKeyEvent(self, event: QtGui.QKeyEvent, eventType: str) -> bool:
        data = self._keyEventData(event, eventType)
        if data is None:
            return False
        self._sendEvent(data)
        return True

    def _mouseButtonName(self, button: QtCore.Qt.MouseButton) -> str:
        if button == QtCore.Qt.RightButton:
            return "Right"
        if button == QtCore.Qt.MiddleButton:
            return "Middle"
        if button == QtCore.Qt.XButton1:
            return "XButton1"
        if button == QtCore.Qt.XButton2:
            return "XButton2"
        return "Left"

    def _eventPosition(self, event: Any) -> tuple[int, int]:
        dpr = self._dpr()
        return int(event.x() * dpr), int(event.y() * dpr)

    def _sendMouseButtonEvent(self, event: QtGui.QMouseEvent, eventType: str) -> None:
        x, y = self._eventPosition(event)
        data = {"type": eventType, "button": self._mouseButtonName(event.button()), "x": x, "y": y}
        self._sendEvent(data)

    def eventFilter(self, receiver: QtCore.QObject, event: QtCore.QEvent) -> bool:
        eventType = event.type()
        if eventType == QtCore.QEvent.ApplicationStateChange:
            if self._keyboardCaptureActive:
                QtCore.QTimer.singleShot(0, self._restoreKeyboardFocus)
            return super().eventFilter(receiver, event)
        if eventType == QtCore.QEvent.ShortcutOverride:
            if self._shouldAcceptShortcutOverride(receiver):
                event.accept()
                return True
            return super().eventFilter(receiver, event)
        if not self._shouldUseFallbackKeyForwarding(receiver):
            return super().eventFilter(receiver, event)
        if isinstance(event, QtGui.QKeyEvent):
            if eventType == QtCore.QEvent.KeyPress and self._forwardKeyEvent(event, "KeyPressed"):
                event.accept()
                return True
            if eventType == QtCore.QEvent.KeyRelease and self._forwardKeyEvent(event, "KeyReleased"):
                event.accept()
                return True
        return super().eventFilter(receiver, event)

    def keyPressEvent(self, event: QtGui.QKeyEvent) -> None:
        if not self._canForwardInputEvent():
            event.accept()
            return
        self._keyboardCaptureActive = True
        if self._forwardKeyEvent(event, "KeyPressed"):
            event.accept()
            return
        super().keyPressEvent(event)

    def keyReleaseEvent(self, event: QtGui.QKeyEvent) -> None:
        if not self._canForwardInputEvent():
            event.accept()
            return
        if self._forwardKeyEvent(event, "KeyReleased"):
            event.accept()
            return
        super().keyReleaseEvent(event)

    def mousePressEvent(self, event: QtGui.QMouseEvent) -> None:
        if not self._canForwardInputEvent():
            event.accept()
            return
        self._restoreKeyboardFocus()
        self._sendMouseButtonEvent(event, "MouseButtonPressed")
        event.accept()

    def mouseReleaseEvent(self, event: QtGui.QMouseEvent) -> None:
        if not self._canForwardInputEvent():
            event.accept()
            return
        self._sendMouseButtonEvent(event, "MouseButtonReleased")
        event.accept()

    def mouseMoveEvent(self, event: QtGui.QMouseEvent) -> None:
        if not self._canForwardInputEvent():
            event.accept()
            return
        x, y = self._eventPosition(event)
        data = {"type": "MouseMoved", "x": x, "y": y}
        self._sendEvent(data)
        event.accept()

    def wheelEvent(self, event: QtGui.QWheelEvent) -> None:
        if not self._canForwardInputEvent():
            event.accept()
            return
        delta = event.angleDelta().y() / 120.0
        x, y = self._eventPosition(event)
        data = {"type": "MouseWheelScrolled", "delta": delta, "x": x, "y": y}
        self._sendEvent(data)
        event.accept()

    def focusInEvent(self, event: QtGui.QFocusEvent) -> None:
        if self._canForwardInputEvent():
            self._keyboardCaptureActive = True
            self._sendEvent({"type": "FocusGained"})
        super().focusInEvent(event)

    def focusOutEvent(self, event: QtGui.QFocusEvent) -> None:
        self._keyboardCaptureActive = event.reason() == QtCore.Qt.ActiveWindowFocusReason
        self._sendEvent({"type": "FocusLost"})
        super().focusOutEvent(event)
