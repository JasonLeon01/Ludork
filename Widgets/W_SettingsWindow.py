# -*- encoding: utf-8 -*-

from typing import Any, Optional
import os
import json
import platform
from PyQt5 import QtCore, QtGui, QtWidgets
import EditorStatus
from Utils import Locale, Panel


class _BoolSwitch(QtWidgets.QWidget):
    toggled = QtCore.pyqtSignal(bool)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None, initial: bool = False) -> None:
        super().__init__(parent)
        self._checked = bool(initial)
        self.setFixedSize(56, 28)
        self.setCursor(QtCore.Qt.PointingHandCursor)

    def isChecked(self) -> bool:
        return self._checked

    def setChecked(self, v: bool) -> None:
        v = bool(v)
        if v != self._checked:
            self._checked = v
            self.update()

    def sizeHint(self) -> QtCore.QSize:
        return QtCore.QSize(56, 28)

    def paintEvent(self, e: QtGui.QPaintEvent) -> None:
        p = QtGui.QPainter(self)
        p.setRenderHint(QtGui.QPainter.Antialiasing, True)
        w = self.width()
        h = self.height()
        r = h // 2
        bg_off = QtGui.QColor(120, 120, 120)
        bg_on = QtWidgets.QApplication.palette().highlight().color()
        bg = bg_on if self._checked else bg_off
        p.setPen(QtCore.Qt.NoPen)
        p.setBrush(bg)
        p.drawRoundedRect(0, 0, w, h, r, r)
        knob = QtGui.QColor(240, 240, 240)
        p.setBrush(knob)
        pad = 2
        d = h - pad * 2
        cx = w - r if self._checked else r
        p.drawEllipse(QtCore.QPointF(cx, h / 2.0), d / 2.0, d / 2.0)

    def mousePressEvent(self, e: QtGui.QMouseEvent) -> None:
        if not self.isEnabled():
            return
        self._checked = not self._checked
        self.update()
        self.toggled.emit(self._checked)

    def changeEvent(self, e: QtCore.QEvent) -> None:
        if e.type() == QtCore.QEvent.EnabledChange:
            Panel.applyDisabledOpacity(self)
        super().changeEvent(e)


class SettingsWindow(QtWidgets.QMainWindow):
    modified = QtCore.pyqtSignal()

    def __init__(
        self,
        parent: Optional[QtWidgets.QWidget] = None,
        proj_config: Optional[dict[str, Any]] = None,
    ):
        super().__init__(parent)
        self._projConfig: dict[str, Any] = {}
        self._projPath = os.path.join(EditorStatus.PROJ_PATH, "Main.proj")
        self._hiddenKeys = set(["lastMap", "lastFileExplorerPath"])
        self._disabledKeys = {"darwin": ["IndividualWindow"]}
        if isinstance(proj_config, dict):
            self._projConfig = proj_config
        else:
            self._projConfig = {}
            if os.path.exists(self._projPath):
                try:
                    with open(self._projPath, "r", encoding="utf-8") as f:
                        content = f.read().strip()
                        if content:
                            self._projConfig = json.loads(content)
                except Exception as e:
                    QtWidgets.QMessageBox.warning(self, "Hint", str(e))
                    self._projConfig = {}
        self.setWindowTitle(Locale.getContent("GAME_SETTINGS"))
        self.setMinimumSize(640, 320)
        central = QtWidgets.QWidget(self)
        self.setCentralWidget(central)
        outer = QtWidgets.QVBoxLayout(central)
        outer.setContentsMargins(8, 8, 8, 8)
        outer.setSpacing(8)
        self.scroll = QtWidgets.QScrollArea(self)
        self.scroll.setWidgetResizable(True)
        outer.addWidget(self.scroll)
        self.container = QtWidgets.QWidget(self)
        self.form = QtWidgets.QFormLayout(self.container)
        self.form.setContentsMargins(16, 16, 16, 16)
        self.form.setSpacing(12)
        self.scroll.setWidget(self.container)
        self._populate()

    def _labelForKey(self, key: str) -> QtWidgets.QLabel:
        lab = QtWidgets.QLabel(Locale.getContent(key), self.container)
        return lab

    def _populate(self) -> None:
        for i in reversed(range(self.form.count())):
            item = self.form.itemAt(i)
            if item and item.widget():
                w = item.widget()
                self.form.removeWidget(w)
                w.setParent(None)
        for key, val in self._projConfig.items():
            if key in self._hiddenKeys:
                continue
            os_name = platform.system().lower()
            disabled_list = self._disabledKeys.get(os_name, [])
            is_disabled = key in disabled_list
            lab = self._labelForKey(key)
            if isinstance(val, bool):
                w = _BoolSwitch(self.container, bool(val))

                if not is_disabled:

                    def on_toggled(v: bool, k: str = key):
                        self._projConfig[k] = v
                        self._save()
                        self.modified.emit()

                    w.toggled.connect(on_toggled)
                w.setEnabled(not is_disabled)
                self.form.addRow(lab, w)
            else:
                edit = QtWidgets.QLineEdit(self.container)
                edit.setText("" if val is None else str(val))

                if not is_disabled:

                    def on_changed(text: str, k: str = key, orig=val):
                        new_val: Any
                        if isinstance(orig, int):
                            try:
                                new_val = int(text) if text.strip() else 0
                            except Exception:
                                new_val = orig
                        elif isinstance(orig, float):
                            try:
                                new_val = float(text) if text.strip() else 0.0
                            except Exception:
                                new_val = orig
                        else:
                            new_val = text
                        self._projConfig[k] = new_val
                        self._save()
                        self.modified.emit()

                    edit.textChanged.connect(on_changed)
                edit.setReadOnly(is_disabled)
                self.form.addRow(lab, edit)

    def _save(self) -> None:
        try:
            if self._projPath:
                data = {}
                if isinstance(self._projConfig, dict):
                    data.update(self._projConfig)
                if os.path.exists(self._projPath):
                    with open(self._projPath, "r", encoding="utf-8") as f:
                        content = f.read().strip()
                        if content:
                            old = json.loads(content)
                            if isinstance(old, dict):
                                for k in self._hiddenKeys:
                                    if k in old:
                                        data[k] = old[k]
                with open(self._projPath, "w", encoding="utf-8") as f:
                    json.dump(data, f, ensure_ascii=False, indent=4)
        except Exception as e:
            QtWidgets.QMessageBox.warning(self, "Hint", str(e))
