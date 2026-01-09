# -*- encoding: utf-8 -*-

from __future__ import annotations

from typing import Any, Dict, List, Optional, Tuple

from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import Locale, System


class LightPanel(QtWidgets.QWidget):
    lightEdited = QtCore.pyqtSignal(object)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        System.applyStyle(self, "config.qss")
        self._form = QtWidgets.QFormLayout()
        self._form.setContentsMargins(0, 0, 0, 0)
        self._form.setSpacing(8)

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)
        layout.addLayout(self._form)
        layout.addStretch(1)

        self._editMap: Dict[str, List[QtWidgets.QLineEdit]] = {}
        self._isLoading = False
        self._lastData: Optional[Dict[str, Any]] = None

        self.setLight(None)

    def setLight(self, lightData: Optional[Dict[str, Any]]) -> None:
        self._clearRows()
        self._editMap.clear()
        self._lastData = None

        if lightData is None:
            return

        self._isLoading = True
        for key, kind in self._fieldSchema():
            label = QtWidgets.QLabel(Locale.getContent(key))
            edits = self._createEditorsForKind(kind)
            values = self._extractValues(lightData, key, kind)

            for i in range(len(edits)):
                edits[i].setText(self._formatNumber(values[i]))
                edits[i].editingFinished.connect(self._onAnyEditFinished)

            rowWidget = self._packEditors(edits)
            self._form.addRow(label, rowWidget)
            self._editMap[key] = edits
        self._isLoading = False

    def updateLight(self, lightData: Optional[Dict[str, Any]]) -> None:
        if lightData is None:
            self.setLight(None)
            return
        if not self._editMap:
            self.setLight(lightData)
            return

        self._isLoading = True
        for key, kind in self._fieldSchema():
            edits = self._editMap.get(key)
            if not edits:
                continue
            values = self._extractValues(lightData, key, kind)
            for i in range(min(len(edits), len(values))):
                edits[i].setText(self._formatNumber(values[i]))
        self._isLoading = False
        self._lastData = self._collectData()

    def _fieldSchema(self) -> List[Tuple[str, str]]:
        return [
            ("position", "Vector2"),
            ("color", "Color"),
            ("radius", "float"),
            ("intensity", "float"),
        ]

    def _onAnyEditFinished(self) -> None:
        if self._isLoading:
            return
        data = self._collectData()
        if data is None:
            return
        if self._lastData != data:
            self._lastData = data
            self.lightEdited.emit(data)

    def _collectData(self) -> Optional[Dict[str, Any]]:
        if not self._editMap:
            return None

        out: Dict[str, Any] = {}
        for key, kind in self._fieldSchema():
            edits = self._editMap.get(key)
            if not edits:
                continue
            if kind == "Vector2":
                out[key] = [self._parseFloat(edits[0].text()), self._parseFloat(edits[1].text())]
            elif kind == "Vector3":
                out[key] = [
                    self._parseFloat(edits[0].text()),
                    self._parseFloat(edits[1].text()),
                    self._parseFloat(edits[2].text()),
                ]
            elif kind == "Color":
                out[key] = [
                    self._parseInt(edits[0].text()),
                    self._parseInt(edits[1].text()),
                    self._parseInt(edits[2].text()),
                    self._parseInt(edits[3].text()),
                ]
            else:
                out[key] = self._parseFloat(edits[0].text())
        return out

    def _parseFloat(self, text: str) -> float:
        try:
            return float(text)
        except Exception:
            return 0.0

    def _parseInt(self, text: str) -> int:
        try:
            return int(float(text))
        except Exception:
            return 0

    def _clearRows(self) -> None:
        while self._form.count():
            item = self._form.takeAt(0)
            w = item.widget()
            if w is not None:
                w.setParent(None)
                w.deleteLater()
            else:
                subLayout = item.layout()
                if subLayout is not None:
                    self._clearLayout(subLayout)

    def _clearLayout(self, layout: QtWidgets.QLayout) -> None:
        while layout.count():
            item = layout.takeAt(0)
            w = item.widget()
            if w is not None:
                w.setParent(None)
                w.deleteLater()
            else:
                subLayout = item.layout()
                if subLayout is not None:
                    self._clearLayout(subLayout)

    def _packEditors(self, edits: List[QtWidgets.QLineEdit]) -> QtWidgets.QWidget:
        w = QtWidgets.QWidget(self)
        h = QtWidgets.QHBoxLayout(w)
        h.setContentsMargins(0, 0, 0, 0)
        h.setSpacing(6)
        for e in edits:
            h.addWidget(e, 1)
        return w

    def _createEditorsForKind(self, kind: str) -> List[QtWidgets.QLineEdit]:
        if kind == "Vector2":
            return [self._createFloatEdit(), self._createFloatEdit()]
        if kind == "Vector3":
            return [self._createFloatEdit(), self._createFloatEdit(), self._createFloatEdit()]
        if kind == "Color":
            return [
                self._createIntEdit(),
                self._createIntEdit(),
                self._createIntEdit(),
                self._createIntEdit(),
            ]
        return [self._createFloatEdit()]

    def _createFloatEdit(self) -> QtWidgets.QLineEdit:
        edit = QtWidgets.QLineEdit(self)
        edit.setValidator(QtGui.QDoubleValidator(edit))
        edit.setText("0")
        return edit

    def _createIntEdit(self) -> QtWidgets.QLineEdit:
        edit = QtWidgets.QLineEdit(self)
        edit.setValidator(QtGui.QIntValidator(edit))
        edit.setText("0")
        return edit

    def _extractValues(self, data: Dict[str, Any], key: str, kind: str) -> List[float]:
        if kind == "Vector2":
            return self._extractVector(data.get(key), 2)
        if kind == "Vector3":
            return self._extractVector(data.get(key), 3)
        if kind == "Color":
            return self._extractVector(data.get(key), 4)
        return [self._toNumberOrZero(data.get(key))]

    def _extractVector(self, value: Any, n: int) -> List[float]:
        out: List[float] = [0.0] * n
        if isinstance(value, (list, tuple)):
            for i in range(min(n, len(value))):
                out[i] = self._toNumberOrZero(value[i])
        return out

    def _toNumberOrZero(self, v: Any) -> float:
        if isinstance(v, bool):
            return 0.0
        if isinstance(v, (int, float)):
            return float(v)
        return 0.0

    def _formatNumber(self, v: float) -> str:
        if isinstance(v, float) and v.is_integer():
            return str(int(v))
        return str(v)
