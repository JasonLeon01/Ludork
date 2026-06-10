# -*- encoding: utf-8 -*-

from __future__ import annotations
import io
import json
import os
from typing import Optional, Union
from PyQt5 import QtCore, QtGui, QtWidgets
from psutil import Popen
from Utils import Panel


class PipeReader(QtCore.QThread):
    NEW_LINE = QtCore.pyqtSignal(str, str)

    def __init__(self, stream: Union[io.BufferedReader, io.TextIOWrapper], level_override: Optional[str] = None):
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
            self.NEW_LINE.emit(text.rstrip("\n"), lvl)

    def stop(self) -> None:
        self._running = False
        try:
            self._stream.close()
        except Exception as e:
            print(f"Error while closing pipe reader: {e}")

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
    PERFORMANCE_SAMPLE = QtCore.pyqtSignal(float, float)
    _PERFORMANCE_SAMPLE_PREFIX = "__LUDORK_PERF__:"

    def __init__(self):
        super().__init__()
        self._proc: Optional[object] = None
        self._stdout_reader: Optional[PipeReader] = None
        self._stderr_reader: Optional[PipeReader] = None
        self._history: list[str] = []
        self._history_index: Optional[int] = None
        self._log_entries: list[tuple[str, str]] = []
        self._log_file_path: Optional[str] = None

        self._view = QtWidgets.QTextEdit()
        self._view.setReadOnly(True)
        f = QtGui.QFont()
        f.setFamily("Consolas")
        f.setStyleHint(QtGui.QFont.Monospace)
        self._view.setFont(f)

        self._input = QtWidgets.QLineEdit()
        self._input.setPlaceholderText(ELOC("SEND_HINT"))
        self._send = QtWidgets.QPushButton(ELOC("SEND"))
        self._send.clicked.connect(self._onSend)
        self._input.returnPressed.connect(self._onSend)
        self._input.installEventFilter(self)
        self._send.setEnabled(False)
        self._input.setEnabled(False)

        self._messageFilter = QtWidgets.QAction(ELOC("CONSOLE_FILTER_MESSAGE"), self)
        self._warningFilter = QtWidgets.QAction(ELOC("CONSOLE_FILTER_WARNING"), self)
        self._errorFilter = QtWidgets.QAction(ELOC("CONSOLE_FILTER_ERROR"), self)
        for action in (self._messageFilter, self._warningFilter, self._errorFilter):
            action.setCheckable(True)
            action.setChecked(True)
            action.toggled.connect(self._refreshView)

        filterMenu = QtWidgets.QMenu(self)
        filterMenu.addAction(self._messageFilter)
        filterMenu.addAction(self._warningFilter)
        filterMenu.addAction(self._errorFilter)

        self._filterButton = QtWidgets.QToolButton()
        self._filterButton.setText(ELOC("CONSOLE_FILTER_LOGS"))
        self._filterButton.setPopupMode(QtWidgets.QToolButton.InstantPopup)
        self._filterButton.setMenu(filterMenu)

        self._search = QtWidgets.QLineEdit()
        self._search.setPlaceholderText(ELOC("CONSOLE_SEARCH_HINT"))
        self._search.textChanged.connect(self._refreshView)

        tl = QtWidgets.QHBoxLayout()
        tl.setContentsMargins(0, 0, 0, 0)
        tl.setSpacing(8)
        tl.addWidget(self._filterButton, 0)
        tl.addWidget(self._search, 1)

        bl = QtWidgets.QHBoxLayout()
        bl.setContentsMargins(0, 0, 0, 0)
        bl.setSpacing(6)
        bl.addWidget(self._input, 1)
        bl.addWidget(self._send, 0)

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)
        layout.addLayout(tl)
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

    def attach_process(self, proc: Popen, logFilePath: Optional[str] = None, resetLog: bool = False) -> None:
        self.detach_process()
        self.setLogFile(logFilePath, resetLog)
        self._proc = proc
        self._stdout_reader = PipeReader(proc.stdout, None)
        self._stderr_reader = PipeReader(proc.stderr, None)
        self._stdout_reader.NEW_LINE.connect(self._handle_line)
        self._stderr_reader.NEW_LINE.connect(self._append_line)
        self._stdout_reader.start()
        self._stderr_reader.start()
        ok = getattr(proc, "stdin", None) is not None
        self._send.setEnabled(ok)
        self._input.setEnabled(ok)
        Panel.ApplyDisabledOpacity(self._send)
        Panel.ApplyDisabledOpacity(self._input)

    def detach_process(self) -> None:
        if self._stdout_reader:
            try:
                self._stdout_reader.stop()
                self._stdout_reader.wait()
            except Exception as e:
                print(f"Error while stopping stdout reader: {e}")
            self._stdout_reader = None
        if self._stderr_reader:
            try:
                self._stderr_reader.stop()
                self._stderr_reader.wait()
            except Exception as e:
                print(f"Error while stopping stderr reader: {e}")
            self._stderr_reader = None
        if self._proc:
            try:
                self._proc.terminate()
            except Exception:
                pass
            finally:
                self._proc = None
        self._send.setEnabled(False)
        self._input.setEnabled(False)
        Panel.ApplyDisabledOpacity(self._send)
        Panel.ApplyDisabledOpacity(self._input)
        self.setLogFile(None)

    def clear(self) -> None:
        self._log_entries.clear()
        self._view.clear()

    def setLogFile(self, logFilePath: Optional[str], resetLog: bool = False) -> None:
        self._log_file_path = logFilePath
        if not logFilePath or not resetLog:
            return
        try:
            logDir = os.path.dirname(logFilePath)
            if logDir:
                os.makedirs(logDir, exist_ok=True)
            with open(logFilePath, "w", encoding="utf-8"):
                pass
        except Exception as e:
            self._log_file_path = None
            print(f"Error while resetting console log: {e}")

    def _append_line(self, text: str, level: str) -> None:
        self._log_entries.append((text, level))
        self._writeLogLine(text, level)
        if not self._acceptsEntry(text, level):
            return
        self._insertLine(text, level)

    def _writeLogLine(self, text: str, level: str) -> None:
        if not self._log_file_path:
            return
        try:
            with open(self._log_file_path, "a", encoding="utf-8") as logFile:
                logFile.write(f"[{level}] {text}\n")
        except Exception as e:
            print(f"Error while writing console log: {e}")

    def _insertLine(self, text: str, level: str) -> None:
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

    def _refreshView(self) -> None:
        self._view.clear()
        for text, level in self._log_entries:
            if self._acceptsEntry(text, level):
                self._insertLine(text, level)

    def _acceptsEntry(self, text: str, level: str) -> bool:
        search = self._search.text().strip().lower()
        if search and search not in text.lower():
            return False

        category = self._filterCategory(level)
        if category == "ERROR":
            return self._errorFilter.isChecked()
        if category == "WARNING":
            return self._warningFilter.isChecked()
        return self._messageFilter.isChecked()

    def _filterCategory(self, level: str) -> str:
        if level == "ERROR":
            return "ERROR"
        if level == "WARNING":
            return "WARNING"
        return "MESSAGE"

    def _handle_line(self, text: str, level: str) -> None:
        if text.startswith(self._PERFORMANCE_SAMPLE_PREFIX):
            raw = text[len(self._PERFORMANCE_SAMPLE_PREFIX) :].strip()
            try:
                payload = json.loads(raw)
                self.PERFORMANCE_SAMPLE.emit(float(payload.get("fps", 0.0)), float(payload.get("memory", 0.0)))
            except Exception:
                self._append_line(text, level)
            return
        self._append_line(text, level)

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
                print(f"Error while writing to stdin: {e}")
        self._input.clear()
