# -*- encoding: utf-8 -*-

import os
import configparser
from typing import Any, Optional
from PyQt5 import QtCore, QtWidgets
from EditorGlobal import EditorStatus


class _WidePopupComboBox(QtWidgets.QComboBox):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        view = QtWidgets.QListView(self)
        view.setTextElideMode(QtCore.Qt.ElideNone)
        view.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self.setView(view)
        self.setSizeAdjustPolicy(QtWidgets.QComboBox.AdjustToContents)

    def showPopup(self) -> None:
        self._syncPopupWidth()
        super().showPopup()
        self._syncPopupWidth()

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self._syncPopupWidth()

    def _syncPopupWidth(self) -> None:
        view = self.view()
        if view is None:
            return
        frameWidth = view.frameWidth() * 2
        contentWidth = max(0, view.sizeHintForColumn(0))
        scrollWidth = view.verticalScrollBar().sizeHint().width() if self.count() > self.maxVisibleItems() else 0
        popupWidth = max(self.width(), contentWidth + frameWidth + scrollWidth + 24)
        view.setMinimumWidth(popupWidth)
        popupWindow = view.window()
        if popupWindow is not None and popupWindow.windowFlags() & QtCore.Qt.Popup:
            popupWindow.setMinimumWidth(popupWidth)


class GameConfigDialog(QtWidgets.QDialog):
    def __init__(
        self, parent: Optional[QtWidgets.QWidget] = None, initialData: Optional[dict[str, Any]] = None
    ) -> None:
        super().__init__(parent)
        self._iniPath = os.path.join(EditorStatus.PROJ_PATH, "Main.ini")
        self._config = configparser.ConfigParser()
        self._data = {
            "script": "Entry.py",
            "language": "en_GB",
            "scale": 2.0,
            "framerate": 120,
            "verticalsync": True,
            "musicon": True,
            "soundon": True,
            "voiceon": True,
            "musicvolume": 100.0,
            "soundvolume": 100.0,
            "voicevolume": 100.0,
        }
        self._resultData: dict[str, Any] = dict(self._data)
        self._changed: bool = False
        self._load()
        if isinstance(initialData, dict):
            for key in self._data.keys():
                if key in initialData:
                    self._data[key] = initialData[key]
            self._resultData = dict(self._data)
        self._setupUi()

    def _toInt(self, value: Any, default: int) -> int:
        if isinstance(value, bool):
            return default
        try:
            return int(value)
        except Exception:
            return default

    def _toFloat(self, value: Any, default: float) -> float:
        if isinstance(value, bool):
            return default
        try:
            return float(value)
        except Exception:
            return default

    def _toBool(self, value: Any, default: bool) -> bool:
        if isinstance(value, bool):
            return value
        if not isinstance(value, str):
            return default
        s = value.strip().lower()
        if s in ("1", "true", "yes", "on"):
            return True
        if s in ("0", "false", "no", "off"):
            return False
        return default

    def _load(self) -> None:
        try:
            if os.path.exists(self._iniPath):
                self._config.read(self._iniPath, encoding="utf-8")
            if "Main" not in self._config:
                self._config["Main"] = {}
            sec = self._config["Main"]
            self._data["script"] = str(sec.get("script", self._data["script"])).strip() or "Entry.py"
            self._data["language"] = str(sec.get("language", self._data["language"])).strip()
            self._data["scale"] = self._toFloat(sec.get("scale", self._data["scale"]), 1.0)
            self._data["framerate"] = max(1, self._toInt(sec.get("framerate", self._data["framerate"]), 60))
            self._data["verticalsync"] = self._toBool(sec.get("verticalsync", self._data["verticalsync"]), False)
            self._data["musicon"] = self._toBool(sec.get("musicon", self._data["musicon"]), True)
            self._data["soundon"] = self._toBool(sec.get("soundon", self._data["soundon"]), True)
            self._data["voiceon"] = self._toBool(sec.get("voiceon", self._data["voiceon"]), True)
            self._data["musicvolume"] = self._toFloat(sec.get("musicvolume", self._data["musicvolume"]), 100.0)
            self._data["soundvolume"] = self._toFloat(sec.get("soundvolume", self._data["soundvolume"]), 100.0)
            self._data["voicevolume"] = self._toFloat(sec.get("voicevolume", self._data["voicevolume"]), 100.0)
        except Exception as e:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("GAME_CONFIG_LOAD_FAILED") + "\n" + str(e))
            self._config = configparser.ConfigParser()
            self._config["Main"] = {}

    def _getLanguageOptions(self) -> list[str]:
        localeDir = os.path.join(EditorStatus.PROJ_PATH, "Data", "Locale")
        langs: list[str] = []
        if os.path.exists(localeDir):
            for name in os.listdir(localeDir):
                if os.path.splitext(name)[1].lower() == ".xlsx":
                    continue
                if os.path.splitext(name)[1]:
                    continue
                fp = os.path.join(localeDir, name)
                if os.path.isfile(fp):
                    langs.append(name)
        langs.sort()
        return langs

    def _setupUi(self) -> None:
        self.setWindowTitle(ELOC("GAME_CONFIG"))
        self.setMinimumSize(560, 320)
        form = QtWidgets.QFormLayout(self)
        form.setContentsMargins(12, 12, 12, 12)
        form.setSpacing(8)
        form.setFieldGrowthPolicy(QtWidgets.QFormLayout.AllNonFixedFieldsGrow)

        self.scriptEdit = QtWidgets.QLineEdit(self)
        self.scriptEdit.setReadOnly(True)
        self.scriptEdit.setText(str(self._data["script"]))
        form.addRow(ELOC("script"), self.scriptEdit)

        self.languageCombo = _WidePopupComboBox(self)
        langs = self._getLanguageOptions()
        currentLang = str(self._data["language"])
        if currentLang and currentLang not in langs:
            langs.append(currentLang)
            langs.sort()
        self.languageCombo.addItems(langs)
        if currentLang:
            idx = self.languageCombo.findText(currentLang)
            if idx >= 0:
                self.languageCombo.setCurrentIndex(idx)
        form.addRow(ELOC("language"), self.languageCombo)

        self.scaleCombo = _WidePopupComboBox(self)
        scaleItems = [1.0, 1.25, 1.5, 1.75, 2.0]
        currentScale = round(float(self._data["scale"]), 2)
        if currentScale not in scaleItems:
            currentScale = 1.0
        for v in scaleItems:
            self.scaleCombo.addItem(f"{v:.2f}", float(v))
        scaleIdx = self.scaleCombo.findData(currentScale)
        if scaleIdx >= 0:
            self.scaleCombo.setCurrentIndex(scaleIdx)
        form.addRow(ELOC("scale"), self.scaleCombo)

        self.framerateCombo = _WidePopupComboBox(self)
        frItems = [30, 60, 90, 120]
        currentFr = int(self._data["framerate"])
        if currentFr not in frItems:
            nearest = min(frItems, key=lambda x: abs(x - currentFr))
            currentFr = nearest
        for v in frItems:
            self.framerateCombo.addItem(str(v), int(v))
        frIdx = self.framerateCombo.findData(currentFr)
        if frIdx >= 0:
            self.framerateCombo.setCurrentIndex(frIdx)
        form.addRow(ELOC("framerate"), self.framerateCombo)

        self.verticalSyncCheck = QtWidgets.QCheckBox(self)
        self.verticalSyncCheck.setChecked(bool(self._data["verticalsync"]))
        form.addRow(ELOC("verticalsync"), self.verticalSyncCheck)

        self.musicOnCheck = QtWidgets.QCheckBox(self)
        self.musicOnCheck.setChecked(bool(self._data["musicon"]))
        form.addRow(ELOC("musicon"), self.musicOnCheck)

        self.soundOnCheck = QtWidgets.QCheckBox(self)
        self.soundOnCheck.setChecked(bool(self._data["soundon"]))
        form.addRow(ELOC("soundon"), self.soundOnCheck)

        self.voiceOnCheck = QtWidgets.QCheckBox(self)
        self.voiceOnCheck.setChecked(bool(self._data["voiceon"]))
        form.addRow(ELOC("voiceon"), self.voiceOnCheck)

        self.musicVolumeSpin = QtWidgets.QDoubleSpinBox(self)
        self.musicVolumeSpin.setDecimals(2)
        self.musicVolumeSpin.setRange(0.0, 100.0)
        self.musicVolumeSpin.setSingleStep(1.0)
        self.musicVolumeSpin.setValue(float(self._data["musicvolume"]))
        form.addRow(ELOC("musicvolume"), self.musicVolumeSpin)

        self.soundVolumeSpin = QtWidgets.QDoubleSpinBox(self)
        self.soundVolumeSpin.setDecimals(2)
        self.soundVolumeSpin.setRange(0.0, 100.0)
        self.soundVolumeSpin.setSingleStep(1.0)
        self.soundVolumeSpin.setValue(float(self._data["soundvolume"]))
        form.addRow(ELOC("soundvolume"), self.soundVolumeSpin)

        self.voiceVolumeSpin = QtWidgets.QDoubleSpinBox(self)
        self.voiceVolumeSpin.setDecimals(2)
        self.voiceVolumeSpin.setRange(0.0, 100.0)
        self.voiceVolumeSpin.setSingleStep(1.0)
        self.voiceVolumeSpin.setValue(float(self._data["voicevolume"]))
        form.addRow(ELOC("voicevolume"), self.voiceVolumeSpin)

        self.btns = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel, self)
        okBtn = self.btns.button(QtWidgets.QDialogButtonBox.Ok)
        cancelBtn = self.btns.button(QtWidgets.QDialogButtonBox.Cancel)
        if okBtn:
            okBtn.setText(ELOC("CONFIRM"))
        if cancelBtn:
            cancelBtn.setText(ELOC("CANCEL"))
        self.btns.accepted.connect(self.accept)
        self.btns.rejected.connect(self.reject)
        form.addRow(self.btns)

    def _buildCurrentData(self) -> dict[str, Any]:
        return {
            "script": str(self._data["script"]),
            "language": self.languageCombo.currentText().strip(),
            "scale": round(float(self.scaleCombo.currentData()), 2),
            "framerate": int(self.framerateCombo.currentData()),
            "verticalsync": bool(self.verticalSyncCheck.isChecked()),
            "musicon": bool(self.musicOnCheck.isChecked()),
            "soundon": bool(self.soundOnCheck.isChecked()),
            "voiceon": bool(self.voiceOnCheck.isChecked()),
            "musicvolume": round(float(self.musicVolumeSpin.value()), 2),
            "soundvolume": round(float(self.soundVolumeSpin.value()), 2),
            "voicevolume": round(float(self.voiceVolumeSpin.value()), 2),
        }

    def accept(self) -> None:
        try:
            self._resultData = self._buildCurrentData()
            self._changed = self._resultData != self._data
            super().accept()
        except Exception as e:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("GAME_CONFIG_SAVE_FAILED") + "\n" + str(e))

    def isChanged(self) -> bool:
        return self._changed

    def getData(self) -> dict[str, Any]:
        return dict(self._resultData)
