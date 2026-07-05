# -*- encoding: utf-8 -*-

import os
from typing import Any, Optional
from PyQt5 import QtCore, QtWidgets
from Utils import System
from Utils.DataConfig import (
    DATA_FILE_EXTENSIONS,
    DATA_FORMAT_DAT,
    DATA_FORMAT_EXTENSIONS,
    DATA_FORMAT_JSON,
    DATA_FORMAT_LABELS,
)
from EditorGlobal import GameData, EditorStatus
from Widgets.Utils.FileSelectorDialog import FileSelectorDialog
from Widgets.Utils.FilterEditDialog import EditFilterData


class MapEditDialog(QtWidgets.QDialog):
    def __init__(
        self,
        parent: QtWidgets.QWidget,
        data: dict[str, Any],
        current_key: str = "",
        title: Optional[str] = None,
        allow_current_key: bool = True,
        data_format: Optional[str] = None,
    ) -> None:
        super().__init__(parent)
        self._data = data
        self._current_key = current_key
        self._allow_current_key = allow_current_key
        existing_format = DATA_FORMAT_JSON if data.get("isJson") else DATA_FORMAT_DAT
        self._data_format = self._NormaliseDataFormat(data_format or existing_format)
        old_name = str(data.get("mapName", ""))
        old_w = int(data.get("width", 0))
        old_h = int(data.get("height", 0))
        if title is None:
            title = ELOC("MAPLIST_EDIT")
        self.setWindowTitle(title)
        self.setMinimumWidth(640)
        form = QtWidgets.QFormLayout(self)
        form.setContentsMargins(12, 12, 12, 12)
        form.setSpacing(8)
        System.SetStyle(self, "mapEdit.qss")
        self.fileEdit = QtWidgets.QLineEdit(self)
        self.fileEdit.setText(current_key)
        self.fileEdit.setStyleSheet("")
        self.dataFormatCombo: Optional[QtWidgets.QComboBox] = None
        self.nameEdit = QtWidgets.QLineEdit(self)
        self.nameEdit.setText(old_name)
        self.wSpin = QtWidgets.QSpinBox(self)
        self.hSpin = QtWidgets.QSpinBox(self)
        self.wSpin.setMinimum(1)
        self.hSpin.setMinimum(1)
        self.wSpin.setMaximum(1 << 15)
        self.hSpin.setMaximum(1 << 15)
        self.wSpin.setValue(max(1, old_w))
        self.hSpin.setValue(max(1, old_h))
        self.nameEdit.setStyleSheet("")
        wLineEdit = self.wSpin.lineEdit()
        hLineEdit = self.hSpin.lineEdit()
        if wLineEdit:
            wLineEdit.setStyleSheet("")
        else:
            self.wSpin.setStyleSheet("")
        if hLineEdit:
            hLineEdit.setStyleSheet("")
        else:
            self.hSpin.setStyleSheet("")
        form.addRow(ELOC("FILE_NAME"), self.fileEdit)
        if data_format is not None:
            self.dataFormatCombo = QtWidgets.QComboBox(self)
            for dataFormat, label in DATA_FORMAT_LABELS.items():
                self.dataFormatCombo.addItem(label, dataFormat)
            currentIndex = self.dataFormatCombo.findData(self._data_format)
            if currentIndex >= 0:
                self.dataFormatCombo.setCurrentIndex(currentIndex)
            self.dataFormatCombo.currentIndexChanged.connect(self._syncFileExtensionToSelectedFormat)
            form.addRow(ELOC("DATA_FORMAT"), self.dataFormatCombo)
        form.addRow(ELOC("EDIT_MAP"), self.nameEdit)
        form.addRow(ELOC("MAP_WIDTH"), self.wSpin)
        form.addRow(ELOC("MAP_HEIGHT"), self.hSpin)

        current_light = data.get("ambientLight", [255, 255, 255, 255])
        if not isinstance(current_light, (list, tuple)) or len(current_light) < 4:
            current_light = [255, 255, 255, 255]
        from Widgets.Utils.ColourPickerDialog import ColourVarEditor

        self.ambientEditor = ColourVarEditor(current_light, self)
        form.addRow(ELOC("AMBIENT_LIGHT"), self.ambientEditor)

        self.bgmEdit = QtWidgets.QLineEdit(self)
        self.bgmEdit.setReadOnly(True)
        self.bgmEdit.setStyleSheet("")
        self.bgmEdit.setText(data.get("bgm", ""))
        self.bgmBtn = QtWidgets.QPushButton("...", self)
        self.bgmFilterBtn = QtWidgets.QPushButton(ELOC("FILTER"), self)
        self._bgmFilterData = data.get("bgmFilter") if "bgmFilter" in data else {}
        self.bgmLayout = QtWidgets.QHBoxLayout()
        self.bgmLayout.setContentsMargins(0, 0, 0, 0)
        self.bgmLayout.setSpacing(6)
        self.bgmClearBtn = QtWidgets.QPushButton(ELOC("CLEAR"), self)
        self.bgmLayout.addWidget(self.bgmEdit, 1)
        self.bgmLayout.addWidget(self.bgmBtn, 0)
        self.bgmLayout.addWidget(self.bgmClearBtn, 0)
        self.bgmLayout.addWidget(self.bgmFilterBtn, 0)

        self.bgsEdit = QtWidgets.QLineEdit(self)
        self.bgsEdit.setReadOnly(True)
        self.bgsEdit.setStyleSheet("")
        self.bgsEdit.setText(data.get("bgs", ""))
        self.bgsBtn = QtWidgets.QPushButton("...", self)
        self.bgsFilterBtn = QtWidgets.QPushButton(ELOC("FILTER"), self)
        self._bgsFilterData = data.get("bgsFilter") if "bgsFilter" in data else {}
        self.bgsLayout = QtWidgets.QHBoxLayout()
        self.bgsLayout.setContentsMargins(0, 0, 0, 0)
        self.bgsLayout.setSpacing(6)
        self.bgsClearBtn = QtWidgets.QPushButton(ELOC("CLEAR"), self)
        self.bgsLayout.addWidget(self.bgsEdit, 1)
        self.bgsLayout.addWidget(self.bgsBtn, 0)
        self.bgsLayout.addWidget(self.bgsClearBtn, 0)
        self.bgsLayout.addWidget(self.bgsFilterBtn, 0)

        bgmRoot = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Musics")
        bgsRoot = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Musics")

        def onBrowseBgm():
            dlg = FileSelectorDialog(self, bgmRoot, FileSelectorDialog.audioFilesFilter())
            fp = dlg.execSelect()
            if fp:
                self.bgmEdit.setText(os.path.basename(fp))

        def onClearBgm():
            self.bgmEdit.clear()

        def onBrowseBgs():
            dlg = FileSelectorDialog(self, bgsRoot, FileSelectorDialog.audioFilesFilter())
            fp = dlg.execSelect()
            if fp:
                self.bgsEdit.setText(os.path.basename(fp))

        def onClearBgs():
            self.bgsEdit.clear()

        def onEditBgmFilter():
            result = EditFilterData(self, self._bgmFilterData, "bgm")
            if result is not None:
                self._bgmFilterData = result

        def onEditBgsFilter():
            result = EditFilterData(self, self._bgsFilterData, "bgs")
            if result is not None:
                self._bgsFilterData = result

        self.bgmBtn.clicked.connect(onBrowseBgm)
        self.bgmClearBtn.clicked.connect(onClearBgm)
        self.bgsBtn.clicked.connect(onBrowseBgs)
        self.bgsClearBtn.clicked.connect(onClearBgs)
        self.bgmFilterBtn.clicked.connect(onEditBgmFilter)
        self.bgsFilterBtn.clicked.connect(onEditBgsFilter)
        form.addRow(ELOC("MAP_BGM"), self.bgmLayout)
        form.addRow(ELOC("MAP_BGS"), self.bgsLayout)

        self.fogEdit = QtWidgets.QLineEdit(self)
        self.fogEdit.setReadOnly(True)
        self.fogEdit.setStyleSheet("")
        self.fogEdit.setText(data.get("fog", ""))
        self.fogBtn = QtWidgets.QPushButton("...", self)
        self.fogClearBtn = QtWidgets.QPushButton(ELOC("CLEAR"), self)
        self.fogLayout = QtWidgets.QHBoxLayout()
        self.fogLayout.setContentsMargins(0, 0, 0, 0)
        self.fogLayout.setSpacing(6)
        self.fogLayout.addWidget(self.fogEdit, 1)
        self.fogLayout.addWidget(self.fogBtn, 0)
        self.fogLayout.addWidget(self.fogClearBtn, 0)
        fogRoot = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Fogs")

        def onBrowseFog():
            dlg = FileSelectorDialog(self, fogRoot, FileSelectorDialog.imageFilesFilter())
            fp = dlg.execSelect()
            if fp:
                self.fogEdit.setText(os.path.basename(fp))
                self._updateFogOptionsVisible()

        def onClearFog():
            self.fogEdit.clear()
            self._updateFogOptionsVisible()

        self.fogBtn.clicked.connect(onBrowseFog)
        self.fogClearBtn.clicked.connect(onClearFog)
        form.addRow(ELOC("MAP_FOG"), self.fogLayout)

        self._fogOptionsWidget = QtWidgets.QWidget(self)
        self._fogOptionsWidget.setSizePolicy(
            QtWidgets.QSizePolicy.Preferred,
            QtWidgets.QSizePolicy.Minimum,
        )
        fogOptionsForm = QtWidgets.QFormLayout(self._fogOptionsWidget)
        fogOptionsForm.setContentsMargins(0, 0, 0, 0)
        fogOptionsForm.setSpacing(8)
        fogOptionsForm.setFieldGrowthPolicy(QtWidgets.QFormLayout.AllNonFixedFieldsGrow)

        self.fogPowerSpin = QtWidgets.QSpinBox(self)
        self.fogPowerSpin.setRange(0, 100)
        self.fogPowerSpin.setValue(int(data.get("fogPower", 0)))
        fogPowerLineEdit = self.fogPowerSpin.lineEdit()
        if fogPowerLineEdit:
            fogPowerLineEdit.setStyleSheet("")
        else:
            self.fogPowerSpin.setStyleSheet("")
        fogOptionsForm.addRow(ELOC("MAP_FOG_POWER"), self.fogPowerSpin)

        self.fogOxSpin = QtWidgets.QDoubleSpinBox(self)
        self.fogOySpin = QtWidgets.QDoubleSpinBox(self)
        for spin in (self.fogOxSpin, self.fogOySpin):
            spin.setRange(-9999.0, 9999.0)
            spin.setDecimals(2)
            spin.setSingleStep(1.0)
            lineEdit = spin.lineEdit()
            if lineEdit:
                lineEdit.setStyleSheet("")
            else:
                spin.setStyleSheet("")
        self.fogOxSpin.setValue(float(data.get("fogOx", 0)))
        self.fogOySpin.setValue(float(data.get("fogOy", 0)))
        fogOptionsForm.addRow(ELOC("MAP_FOG_OX"), self.fogOxSpin)
        fogOptionsForm.addRow(ELOC("MAP_FOG_OY"), self.fogOySpin)

        self.fogDistortSpin = QtWidgets.QSpinBox(self)
        self.fogDistortSpin.setRange(0, 100)
        self.fogDistortSpin.setValue(int(data.get("fogDistort", 0)))
        fogDistortLineEdit = self.fogDistortSpin.lineEdit()
        if fogDistortLineEdit:
            fogDistortLineEdit.setStyleSheet("")
        else:
            self.fogDistortSpin.setStyleSheet("")
        fogOptionsForm.addRow(ELOC("MAP_FOG_DISTORT"), self.fogDistortSpin)

        form.addRow(self._fogOptionsWidget)
        self.fogEdit.textChanged.connect(self._updateFogOptionsVisible)

        self.btns = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel, self)
        form.addRow(self.btns)
        self._updateFogOptionsVisible()
        confirm_label = ELOC("CONFIRM")
        cancel_label = ELOC("CANCEL")
        ok_btn = self.btns.button(QtWidgets.QDialogButtonBox.Ok)
        cancel_btn = self.btns.button(QtWidgets.QDialogButtonBox.Cancel)
        if ok_btn:
            ok_btn.setText(confirm_label)
        if cancel_btn:
            cancel_btn.setText(cancel_label)
        self.btns.accepted.connect(self.accept)
        self.btns.rejected.connect(self.reject)

    @staticmethod
    def _NormaliseFileKey(name: str) -> str:
        key = name.strip()
        if os.path.splitext(key)[1].lower() in DATA_FILE_EXTENSIONS:
            return os.path.splitext(key)[0]
        return key

    @staticmethod
    def _NormaliseDataFormat(dataFormat: Optional[str]) -> str:
        if dataFormat in DATA_FORMAT_EXTENSIONS:
            return dataFormat
        return DATA_FORMAT_JSON

    def _getFileExtensionForFormat(self) -> str:
        return DATA_FORMAT_EXTENSIONS[self.getDataFormat()]

    def _syncFileExtensionToSelectedFormat(self, _index: int = 0) -> None:
        name = self.fileEdit.text().strip()
        if not name:
            return
        self.fileEdit.setText(self._NormaliseFileKey(name) + self._getFileExtensionForFormat())

    def _hasFogGraphic(self) -> bool:
        return bool(self.fogEdit.text().strip())

    def _updateFogOptionsVisible(self) -> None:
        visible = self._hasFogGraphic()
        self._fogOptionsWidget.setVisible(visible)
        if visible:
            self._fogOptionsWidget.setMinimumHeight(self._fogOptionsWidget.sizeHint().height())
        else:
            self._fogOptionsWidget.setMinimumHeight(0)
        self._relayoutDialog()

    def _relayoutDialog(self) -> None:
        layout = self.layout()
        if layout is not None:
            layout.activate()
        self.adjustSize()
        hint = self.sizeHint()
        w = max(640, hint.width())
        h = hint.height()
        self.setMinimumSize(w, h)
        self.resize(w, h)

    def accept(self) -> None:
        fname = self.fileEdit.text().strip()
        if not fname:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("MAP_FILE_NAME_EMPTY"))
            return
        fname = self._NormaliseFileKey(fname) + self._getFileExtensionForFormat()
        self.fileEdit.setText(fname)

        existing = GameData.mapData
        key = self._NormaliseFileKey(fname)
        if key in existing:
            current_key = self._NormaliseFileKey(self._current_key)
            is_same = self._allow_current_key and current_key and key == current_key
            if not is_same:
                QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("MAP_FILE_NAME_EXISTS"))
                return

        super().accept()

    def getFileName(self) -> str:
        return self._NormaliseFileKey(self.fileEdit.text())

    def getDataFormat(self) -> str:
        if self.dataFormatCombo is not None:
            currentData = self.dataFormatCombo.currentData()
            if currentData in DATA_FORMAT_EXTENSIONS:
                return currentData
        return self._data_format

    def execApply(self) -> bool:
        if self.exec_() != QtWidgets.QDialog.Accepted:
            return False
        GameData.RecordSnapshot()

        new_key = self.getFileName()
        if self._current_key and self._current_key in GameData.mapData and new_key != self._current_key:
            new_map = {}
            for k, v in GameData.mapData.items():
                if k == self._current_key:
                    new_map[new_key] = v
                else:
                    new_map[k] = v
            GameData.mapData.clear()
            GameData.mapData.update(new_map)

        data = self._data
        old_w = int(data.get("width", 0))
        old_h = int(data.get("height", 0))
        new_name = self.nameEdit.text().strip()
        new_w = int(self.wSpin.value())
        new_h = int(self.hSpin.value())
        if new_name:
            data["mapName"] = new_name

        data["ambientLight"] = list(self.ambientEditor.getValue())

        bgm = self.bgmEdit.text().strip()
        data["bgm"] = bgm if bgm else ""
        data["bgmFilter"] = self._bgmFilterData
        bgs = self.bgsEdit.text().strip()
        data["bgs"] = bgs if bgs else ""
        data["bgsFilter"] = self._bgsFilterData
        fog = self.fogEdit.text().strip()
        data["fog"] = fog if fog else ""
        if fog:
            data["fogPower"] = int(self.fogPowerSpin.value())
            data["fogOx"] = float(self.fogOxSpin.value())
            data["fogOy"] = float(self.fogOySpin.value())
            data["fogDistort"] = int(self.fogDistortSpin.value())
        else:
            data["fogPower"] = 0
            data["fogOx"] = 0
            data["fogOy"] = 0
            data["fogDistort"] = 0

        if new_w != old_w or new_h != old_h:
            layers = data.get("layers", {})
            for _, layer in layers.items():
                tiles = layer.get("tiles")
                if not isinstance(tiles, list):
                    continue
                resized = []
                min_h = min(len(tiles), new_h)
                for y in range(min_h):
                    row = list(tiles[y])
                    if new_w < len(row):
                        row = row[:new_w]
                    elif new_w > len(row):
                        row.extend([None] * (new_w - len(row)))
                    resized.append(row)
                if new_h > len(resized):
                    for _ in range(new_h - len(resized)):
                        resized.append([None] * new_w)
                layer["tiles"] = resized
            data["width"] = new_w
            data["height"] = new_h
        if self.getDataFormat() == DATA_FORMAT_JSON:
            data["isJson"] = True
        else:
            data.pop("isJson", None)
        return True


def EditMapInfo(parent: QtWidgets.QWidget, data: dict[str, Any], current_key: str = "") -> bool:
    dlg = MapEditDialog(parent, data, current_key)
    return dlg.execApply()
