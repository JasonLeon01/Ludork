# -*- encoding: utf-8 -*-

from __future__ import annotations

import ast
import re
from typing import Any, Optional

from PyQt5 import QtWidgets, QtCore


_VECTOR_VAR_ALIASES = {
    "Pair": "Vector2Var",
    "PairVar": "Vector2Var",
    "PairFloatVar": "Vector2fVar",
    "PairIntVar": "Vector2iVar",
    "Vector2": "Vector2Var",
    "Vector2Var": "Vector2Var",
    "Vector2f": "Vector2fVar",
    "Vector2fVar": "Vector2fVar",
    "Vector2i": "Vector2iVar",
    "Vector2iVar": "Vector2iVar",
    "Vector2u": "Vector2uVar",
    "Vector2uVar": "Vector2uVar",
    "Vector3": "Vector3Var",
    "Vector3Var": "Vector3Var",
    "Vector3f": "Vector3fVar",
    "Vector3fVar": "Vector3fVar",
    "Vector3i": "Vector3iVar",
    "Vector3iVar": "Vector3iVar",
    "Vector3u": "Vector3uVar",
    "Vector3uVar": "Vector3uVar",
}

_VECTOR_VAR_SPECS = {
    "Vector2Var": (2, float, -999999999.0, 999999999.0),
    "Vector2fVar": (2, float, -999999999.0, 999999999.0),
    "Vector2iVar": (2, int, -2147483648, 2147483647),
    "Vector2uVar": (2, int, 0, 2147483647),
    "Vector3Var": (3, float, -999999999.0, 999999999.0),
    "Vector3fVar": (3, float, -999999999.0, 999999999.0),
    "Vector3iVar": (3, int, -2147483648, 2147483647),
    "Vector3uVar": (3, int, 0, 2147483647),
}

_NUMBER_PATTERN = re.compile(r"[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?")


def NormaliseVectorVarType(varType: Any) -> str:
    if not isinstance(varType, str):
        return ""
    return _VECTOR_VAR_ALIASES.get(varType, "")


def IsVectorVarType(varType: Any) -> bool:
    return bool(NormaliseVectorVarType(varType))


class VectorVarEditor(QtWidgets.QWidget):
    VALUE_CHANGED = QtCore.pyqtSignal(object)

    def __init__(self, varType: str, value: Any = None, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super(VectorVarEditor, self).__init__(parent)
        self._varType = NormaliseVectorVarType(varType)
        self._count, self._numberType, self._minimum, self._maximum = _VECTOR_VAR_SPECS[self._varType]
        self._asTuple = not isinstance(value, list)
        self._spins: list[QtWidgets.QAbstractSpinBox] = []
        self._initUI()
        self.setValue(value, emit=False)

    def getValue(self) -> tuple | list:
        values = [self._coerceNumber(spin.value()) for spin in self._spins]
        return tuple(values) if self._asTuple else list(values)

    def setValue(self, value: Any, emit: bool = True, preserveContainer: bool = False) -> None:
        if not preserveContainer:
            self._asTuple = not isinstance(value, list)
        oldValue = self.getValue()
        values = self._normaliseValue(value)
        for spin, number in zip(self._spins, values):
            wasBlocked = spin.blockSignals(True)
            spin.setValue(number)
            spin.blockSignals(wasBlocked)
        if emit and oldValue != self.getValue():
            self.VALUE_CHANGED.emit(self.getValue())

    def setEditable(self, editable: bool) -> None:
        self.setEnabled(editable)
        for spin in self._spins:
            spin.setEnabled(editable)
            spin.setReadOnly(not editable)

    def _initUI(self) -> None:
        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)
        for i in range(self._count):
            spin = self._createSpin()
            spin.valueChanged.connect(lambda _=None: self.VALUE_CHANGED.emit(self.getValue()))
            self._spins.append(spin)
            layout.addWidget(spin)
        layout.addStretch()

    def _createSpin(self) -> QtWidgets.QAbstractSpinBox:
        if self._numberType is int:
            spin = QtWidgets.QSpinBox(self)
            spin.setRange(int(self._minimum), int(self._maximum))
        else:
            spin = QtWidgets.QDoubleSpinBox(self)
            spin.setRange(float(self._minimum), float(self._maximum))
            spin.setDecimals(2)
            spin.setSingleStep(0.1)
        spin.setMinimumWidth(86)
        return spin

    def _normaliseValue(self, value: Any) -> list:
        values = self._flattenValue(value)
        result = [0 for _ in range(self._count)]
        for i, item in enumerate(values[: self._count]):
            result[i] = self._coerceNumber(item)
        return result

    def _flattenValue(self, value: Any) -> list:
        value = self._parseStringValue(value)
        if value is None:
            return []
        if hasattr(value, "x") and hasattr(value, "y"):
            values = [value.x, value.y]
            if self._count >= 3 and hasattr(value, "z"):
                values.append(value.z)
            return values
        if isinstance(value, (list, tuple)):
            result = []
            for item in value:
                if isinstance(item, (list, tuple)) or hasattr(item, "x"):
                    result.extend(self._flattenValue(item))
                else:
                    result.append(item)
            return result
        return []

    def _parseStringValue(self, value: Any) -> Any:
        if not isinstance(value, str):
            return value
        text = value.strip()
        if not text:
            return None
        try:
            return ast.literal_eval(text)
        except (ValueError, SyntaxError):
            numberText = text
            start = text.find("(")
            end = text.rfind(")")
            if start != -1 and end > start:
                numberText = text[start + 1 : end]
            matches = _NUMBER_PATTERN.findall(numberText)
            if matches:
                return matches
        return value

    def _coerceNumber(self, value: Any) -> int | float:
        try:
            if self._numberType is int:
                return int(float(value))
            return float(value)
        except (TypeError, ValueError):
            return 0
