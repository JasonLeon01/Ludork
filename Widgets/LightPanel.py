# -*- encoding: utf-8 -*-

from __future__ import annotations

from typing import Any, Dict, List, Optional, Tuple

from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import System


class LightPanel(QtWidgets.QWidget):
    LIGHT_EDITED = QtCore.pyqtSignal(object)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        System.ApplyStyle(self, "config.qss")
        self._form = QtWidgets.QFormLayout()
        self._form.setContentsMargins(0, 0, 0, 0)
        self._form.setSpacing(8)

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)
        layout.addLayout(self._form)
        layout.addStretch(1)

        self._editMap: Dict[str, Any] = {}
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
            label = QtWidgets.QLabel(ELOC(key))
            values = self._extractValues(lightData, key, kind)

            if kind == "Color":
                from Widgets.Utils.ColourPickerDialog import ColourVarEditor

                rowWidget = ColourVarEditor(values, self)
                rowWidget.VALUE_CHANGED.connect(lambda _: self._onAnyEditFinished())
                self._editMap[key] = rowWidget
            else:
                edits = self._createEditorsForKind(kind)
                for i in range(len(edits)):
                    edits[i].setText(self._formatNumber(values[i]))
                    edits[i].editingFinished.connect(self._onAnyEditFinished)
                rowWidget = self._packEditors(edits)
                self._editMap[key] = edits
            self._form.addRow(label, rowWidget)
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
            editor = self._editMap.get(key)
            if editor is None:
                continue
            values = self._extractValues(lightData, key, kind)
            if kind == "Color":
                from Widgets.Utils.ColourPickerDialog import ColourVarEditor

                if isinstance(editor, ColourVarEditor):
                    editor.setValue(values, emit=False)
            elif isinstance(editor, list):
                for i in range(min(len(editor), len(values))):
                    editor[i].setText(self._formatNumber(values[i]))
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
            self.LIGHT_EDITED.emit(data)

    def _collectData(self) -> Optional[Dict[str, Any]]:
        if not self._editMap:
            return None

        out: Dict[str, Any] = {}
        for key, kind in self._fieldSchema():
            editor = self._editMap.get(key)
            if editor is None:
                continue
            if kind == "Color":
                from Widgets.Utils.ColourPickerDialog import ColourVarEditor

                if isinstance(editor, ColourVarEditor):
                    out[key] = list(editor.getValue())
            elif not isinstance(editor, list):
                continue
            elif kind == "Vector2":
                out[key] = [self._parseFloat(editor[0].text()), self._parseFloat(editor[1].text())]
            elif kind == "Vector3":
                out[key] = [
                    self._parseFloat(editor[0].text()),
                    self._parseFloat(editor[1].text()),
                    self._parseFloat(editor[2].text()),
                ]
            else:
                out[key] = self._parseFloat(editor[0].text())
        return out

    def _parseFloat(self, text: str) -> float:
        try:
            return float(text)
        except Exception:
            return 0.0

    def _clearRows(self) -> None:
        while self._form.count():
            item = self._form.takeAt(0)
            if item is None:
                continue
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
            if item is None:
                continue
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
        return [self._createFloatEdit()]

    def _createFloatEdit(self) -> QtWidgets.QLineEdit:
        edit = QtWidgets.QLineEdit(self)
        edit.setValidator(QtGui.QDoubleValidator(edit))
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
