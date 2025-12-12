# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import Locale, Panel


class PipeReader(QtCore.QThread):
    new_line = QtCore.pyqtSignal(str, str)

    def __init__(self, stream, level_override: Optional[str] = None):
        super().__init__()
        self._stream = stream
        self._level_override = level_override
        self._running = True

    def run(self) -> None:
        while self._running:
            try:
                line = self._stream.readline()
            except Exception:
                break
            if not line:
                break
            if isinstance(line, bytes):
                try:
                    text = line.decode("utf-8", errors="replace")
                except Exception:
                    text = line.decode(errors="replace")
            else:
                text = str(line)
            lvl = self._detect_level(text) if self._level_override is None else self._level_override
            self.new_line.emit(text.rstrip("\n"), lvl)

    def stop(self) -> None:
        self._running = False
        try:
            if hasattr(self._stream, "close"):
                self._stream.close()
        except Exception as e:
            print(e)

    def _detect_level(self, text: str) -> str:
        t = text.lower()
        if "traceback" in t or "error" in t or "exception" in t or "critical" in t or t.startswith("fatal"):
            return "ERROR"
        if "warning" in t or t.startswith("warn") or "deprecated" in t:
            return "WARNING"
        if "debug" in t:
            return "DEBUG"
        return "INFO"


class ConsoleWidget(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self._proc: Optional[object] = None
        self._stdout_reader: Optional[PipeReader] = None
        self._stderr_reader: Optional[PipeReader] = None
        self._history: list[str] = []
        self._history_index: Optional[int] = None

        self._view = QtWidgets.QTextEdit()
        self._view.setReadOnly(True)
        f = QtGui.QFont()
        f.setFamily("Consolas")
        f.setStyleHint(QtGui.QFont.Monospace)
        self._view.setFont(f)

        self._input = QtWidgets.QLineEdit()
        self._input.setPlaceholderText(Locale.getContent("SEND_HINT"))
        self._send = QtWidgets.QPushButton(Locale.getContent("SEND"))
        self._send.clicked.connect(self._onSend)
        self._input.returnPressed.connect(self._onSend)
        self._input.installEventFilter(self)
        self._send.setEnabled(False)
        self._input.setEnabled(False)

        bl = QtWidgets.QHBoxLayout()
        bl.setContentsMargins(0, 0, 0, 0)
        bl.setSpacing(6)
        bl.addWidget(self._input, 1)
        bl.addWidget(self._send, 0)

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)
        layout.addWidget(self._view, 1)
        layout.addLayout(bl)

    def eventFilter(self, obj, event):
        if obj is self._input and event.type() == QtCore.QEvent.KeyPress:
            key = event.key()
            if key == QtCore.Qt.Key_Up:
                if self._history:
                    if self._history_index is None:
                        self._history_index = len(self._history) - 1
                    elif self._history_index > 0:
                        self._history_index -= 1
                    self._input.setText(self._history[self._history_index])
                    self._input.setCursorPosition(len(self._input.text()))
                return True
            if key == QtCore.Qt.Key_Down:
                if self._history_index is None:
                    return True
                if self._history_index < len(self._history) - 1:
                    self._history_index += 1
                    self._input.setText(self._history[self._history_index])
                    self._input.setCursorPosition(len(self._input.text()))
                else:
                    self._history_index = None
                    self._input.clear()
                return True
            self._history_index = None
        return super().eventFilter(obj, event)

    def attach_process(self, proc) -> None:
        self.detach_process()
        self._proc = proc
        self._stdout_reader = PipeReader(proc.stdout, None)
        self._stderr_reader = PipeReader(proc.stderr, None)
        self._stdout_reader.new_line.connect(self._append_line)
        self._stderr_reader.new_line.connect(self._append_line)
        self._stdout_reader.start()
        self._stderr_reader.start()
        ok = getattr(proc, "stdin", None) is not None
        self._send.setEnabled(ok)
        self._input.setEnabled(ok)
        Panel.applyDisabledOpacity(self._send)
        Panel.applyDisabledOpacity(self._input)

    def detach_process(self) -> None:
        if self._stdout_reader:
            try:
                self._stdout_reader.stop()
                self._stdout_reader.wait()
            except Exception as e:
                print(e)
            self._stdout_reader = None
        if self._stderr_reader:
            try:
                self._stderr_reader.stop()
                self._stderr_reader.wait()
            except Exception as e:
                print(e)
            self._stderr_reader = None
        if self._proc:
            self._proc.terminate()
            self._proc = None
        self._send.setEnabled(False)
        self._input.setEnabled(False)
        Panel.applyDisabledOpacity(self._send)
        Panel.applyDisabledOpacity(self._input)

    def clear(self) -> None:
        self._view.clear()

    def _append_line(self, text: str, level: str) -> None:
        c = QtGui.QColor()
        if level == "ERROR":
            c = QtGui.QColor(200, 60, 60)
        elif level == "WARNING":
            c = QtGui.QColor(220, 180, 60)
        elif level == "DEBUG":
            c = QtGui.QColor(120, 120, 120)
        else:
            c = QtGui.QColor(220, 220, 220)
        tc = QtGui.QTextCharFormat()
        tc.setForeground(QtGui.QBrush(c))
        cursor = self._view.textCursor()
        cursor.movePosition(QtGui.QTextCursor.End)
        cursor.insertText(text + "\n", tc)
        self._view.setTextCursor(cursor)
        self._view.ensureCursorVisible()

    def _onSend(self) -> None:
        t = self._input.text()
        if not t:
            return
        self._append_line(">>> " + t, "INFO")
        self._history.append(t)
        self._history_index = None
        if self._proc and getattr(self._proc, "stdin", None) is not None:
            try:
                self._proc.stdin.write(t + "\n")
                self._proc.stdin.flush()
            except Exception as e:
                print(e)
        self._input.clear()
