# -*- encoding: utf-8 -*-

from typing import Any, Optional
from PyQt5 import QtCore, QtWidgets
from Utils import Locale, System
from Data import GameData


class MapEditDialog(QtWidgets.QDialog):
    def __init__(
        self, parent: QtWidgets.QWidget, data: dict[str, Any], current_key: str = "", title: Optional[str] = None
    ) -> None:
        super().__init__(parent)
        self._data = data
        self._current_key = current_key
        old_name = str(data.get("mapName", ""))
        old_w = int(data.get("width", 0))
        old_h = int(data.get("height", 0))
        if title is None:
            title = Locale.getContent("MAPLIST_EDIT")
        self.setWindowTitle(title)
        self.setMinimumSize(640, 256)
        form = QtWidgets.QFormLayout(self)
        form.setContentsMargins(12, 12, 12, 12)
        form.setSpacing(8)
        System.setStyle(self, "mapEdit.qss")
        self.fileEdit = QtWidgets.QLineEdit(self)
        self.fileEdit.setText(current_key)
        self.fileEdit.setStyleSheet("")
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
        if self.wSpin.lineEdit():
            self.wSpin.lineEdit().setStyleSheet("")
        else:
            self.wSpin.setStyleSheet("")
        if self.hSpin.lineEdit():
            self.hSpin.lineEdit().setStyleSheet("")
        else:
            self.hSpin.setStyleSheet("")
        form.addRow("File Name", self.fileEdit)
        form.addRow(Locale.getContent("EDIT_MAP"), self.nameEdit)
        form.addRow(Locale.getContent("MAP_WIDTH"), self.wSpin)
        form.addRow(Locale.getContent("MAP_HEIGHT"), self.hSpin)

        self.ambientLayout = QtWidgets.QHBoxLayout()
        self.rSpin = QtWidgets.QSpinBox(self)
        self.gSpin = QtWidgets.QSpinBox(self)
        self.bSpin = QtWidgets.QSpinBox(self)
        self.aSpin = QtWidgets.QSpinBox(self)
        current_light = data.get("ambientLight", [255, 255, 255, 255])
        if not isinstance(current_light, (list, tuple)) or len(current_light) < 4:
            current_light = [255, 255, 255, 255]
        for i, spin in enumerate((self.rSpin, self.gSpin, self.bSpin, self.aSpin)):
            spin.setRange(0, 255)
            spin.setValue(int(current_light[i]))
            if spin.lineEdit():
                spin.lineEdit().setStyleSheet("")
            else:
                spin.setStyleSheet("")
            self.ambientLayout.addWidget(spin)
        form.addRow(Locale.getContent("AMBIENT_LIGHT"), self.ambientLayout)

        self.btns = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel, self)
        form.addRow(self.btns)
        confirm_label = Locale.getContent("CONFIRM")
        cancel_label = Locale.getContent("CANCEL")
        ok_btn = self.btns.button(QtWidgets.QDialogButtonBox.Ok)
        cancel_btn = self.btns.button(QtWidgets.QDialogButtonBox.Cancel)
        if ok_btn:
            ok_btn.setText(confirm_label)
        if cancel_btn:
            cancel_btn.setText(cancel_label)
        self.btns.accepted.connect(self.accept)
        self.btns.rejected.connect(self.reject)

    def accept(self) -> None:
        fname = self.fileEdit.text().strip()
        if not fname:
            QtWidgets.QMessageBox.warning(self, "Hint", "File Name cannot be empty.")
            return
        if not fname.endswith(".dat"):
            fname += ".dat"
            self.fileEdit.setText(fname)

        existing = GameData.mapData
        if fname in existing:
            is_same = self._current_key and fname == self._current_key
            if not is_same:
                QtWidgets.QMessageBox.warning(self, "Hint", "File Name already exists.")
                return

        super().accept()

    def getFileName(self) -> str:
        return self.fileEdit.text().strip()

    def execApply(self) -> bool:
        if self.exec_() != QtWidgets.QDialog.Accepted:
            return False
        GameData.recordSnapshot()

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

        new_r = int(self.rSpin.value())
        new_g = int(self.gSpin.value())
        new_b = int(self.bSpin.value())
        new_a = int(self.aSpin.value())
        data["ambientLight"] = [new_r, new_g, new_b, new_a]

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
        return True


def editMapInfo(parent: QtWidgets.QWidget, data: dict[str, Any], current_key: str = "") -> bool:
    dlg = MapEditDialog(parent, data, current_key)
    return dlg.execApply()
