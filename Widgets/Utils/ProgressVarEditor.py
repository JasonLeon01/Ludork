# -*- encoding: utf-8 -*-

from __future__ import annotations

from typing import Any

from PyQt5 import QtCore, QtWidgets

from Widgets.Utils.MetaVarTypes import DEFAULT_PROGRESS_RANGE, ProgressRange


class ProgressVarEditor(QtWidgets.QWidget):
    VALUE_CHANGED = QtCore.pyqtSignal(object)

    def __init__(self, value: Any = 0.0, valueRange: ProgressRange | None = None, parent=None) -> None:
        super().__init__(parent)
        self._returnInt = self._shouldReturnInt(value, valueRange)
        self._minimum, self._maximum, self._step = self._normaliseRange(valueRange)
        self._decimals = self._decimalCount(self._step)
        self._value = self._clampValue(value)

        self.slider = QtWidgets.QSlider(QtCore.Qt.Horizontal, self)
        self.slider.setRange(0, self._stepCount())
        self.slider.setSingleStep(1)
        self.slider.setPageStep(max(1, min(10, self.slider.maximum() // 10 or 1)))

        self.spinBox = QtWidgets.QDoubleSpinBox(self)
        self.spinBox.setDecimals(self._decimals)
        self.spinBox.setRange(self._minimum, self._maximum)
        self.spinBox.setSingleStep(self._step)
        self.spinBox.setKeyboardTracking(False)
        self.spinBox.setFixedWidth(96)

        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(6)
        layout.addWidget(self.slider, 1)
        layout.addWidget(self.spinBox, 0)

        self.slider.valueChanged.connect(self._onSliderChanged)
        self.spinBox.valueChanged.connect(self._onSpinChanged)
        self.setValue(self._value, emit=False)

    def _normaliseRange(self, valueRange: ProgressRange | None) -> ProgressRange:
        if valueRange is None:
            return DEFAULT_PROGRESS_RANGE
        try:
            minimum, maximum, step = float(valueRange[0]), float(valueRange[1]), float(valueRange[2])
        except (TypeError, ValueError, IndexError):
            return DEFAULT_PROGRESS_RANGE
        if maximum < minimum:
            minimum, maximum = maximum, minimum
        if step <= 0:
            step = DEFAULT_PROGRESS_RANGE[2]
        return (minimum, maximum, step)

    def _shouldReturnInt(self, value: Any, valueRange: ProgressRange | None) -> bool:
        if not isinstance(value, int) or isinstance(value, bool):
            return False
        if valueRange is None:
            return True
        return all(isinstance(item, int) and not isinstance(item, bool) for item in valueRange)

    def _decimalCount(self, value: float) -> int:
        text = f"{value:.8f}".rstrip("0").rstrip(".")
        if "." not in text:
            return 0
        return min(6, len(text.split(".", 1)[1]))

    def _stepCount(self) -> int:
        span = max(0.0, self._maximum - self._minimum)
        return max(1, int(round(span / self._step)))

    def _valueFromSlider(self, sliderValue: int) -> float:
        return self._clampValue(self._minimum + float(sliderValue) * self._step)

    def _sliderFromValue(self, value: float) -> int:
        return max(0, min(self.slider.maximum(), int(round((value - self._minimum) / self._step))))

    def _clampValue(self, value: Any) -> float:
        try:
            number = float(value)
        except (TypeError, ValueError):
            number = self._minimum
        return max(self._minimum, min(self._maximum, number))

    def _displayValue(self, value: float) -> float | int:
        if self._returnInt:
            return int(round(value))
        return round(value, self._decimals)

    def getValue(self) -> float | int:
        return self._displayValue(self._value)

    def setValue(self, value: Any, emit: bool = True) -> None:
        newValue = self._clampValue(value)
        changed = self._displayValue(newValue) != self._displayValue(self._value)
        self._value = newValue

        self.slider.blockSignals(True)
        self.spinBox.blockSignals(True)
        try:
            self.slider.setValue(self._sliderFromValue(newValue))
            self.spinBox.setValue(newValue)
        finally:
            self.spinBox.blockSignals(False)
            self.slider.blockSignals(False)

        if emit and changed:
            self.VALUE_CHANGED.emit(self.getValue())

    def setEditable(self, editable: bool) -> None:
        self.slider.setEnabled(editable)
        self.spinBox.setEnabled(editable)
        self.spinBox.setReadOnly(not editable)

    def _onSliderChanged(self, sliderValue: int) -> None:
        self.setValue(self._valueFromSlider(sliderValue), emit=True)

    def _onSpinChanged(self, value: float) -> None:
        self.setValue(value, emit=True)
