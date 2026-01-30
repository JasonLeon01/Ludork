# -*- encoding: utf-8 -*-

from PyQt5 import QtWidgets, QtCore
import dataclasses
from typing import Dict, Any, Type


class DataclassWidget(QtWidgets.QWidget):
    valueChanged = QtCore.pyqtSignal(dict)

    def __init__(self, dc_type: Type, data: Dict[str, Any], parent=None):
        super().__init__(parent)
        self.dc_type = dc_type
        self.data = data if isinstance(data, dict) else {}
        self._inputs = {}
        self._initUI()

    def _initUI(self):
        layout = QtWidgets.QFormLayout(self)
        layout.setContentsMargins(10, 0, 0, 0)

        for field in dataclasses.fields(self.dc_type):
            val = self.data.get(field.name)

            if val is None:
                if field.default is not dataclasses.MISSING:
                    val = field.default
                elif field.default_factory is not dataclasses.MISSING:
                    try:
                        val = field.default_factory()
                    except:
                        pass

            if val is not None:
                self.data[field.name] = val

            widget = self._createFieldWidget(field, val)
            layout.addRow(field.name, widget)
            self._inputs[field.name] = widget

    def _createFieldWidget(self, field: dataclasses.Field, value: Any):
        ftype = field.type

        if dataclasses.is_dataclass(ftype):
            if not isinstance(value, dict):
                value = {}

            gb = QtWidgets.QGroupBox()
            gb.setFlat(True)
            gb.setStyleSheet(
                "QGroupBox { border: 1px solid #444; margin-top: 0.5em; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px 0 3px; }"
            )
            gb_layout = QtWidgets.QVBoxLayout(gb)
            gb_layout.setContentsMargins(0, 5, 0, 0)

            dc_widget = DataclassWidget(ftype, value)
            dc_widget.valueChanged.connect(lambda v, k=field.name: self._onFieldChanged(k, v))
            gb_layout.addWidget(dc_widget)
            return gb

        if ftype == bool:
            w = QtWidgets.QCheckBox()
            w.setChecked(bool(value))
            w.toggled.connect(lambda checked, k=field.name: self._onFieldChanged(k, checked))
            return w
        elif ftype == float:
            w = QtWidgets.QDoubleSpinBox()
            w.setRange(-999999.0, 999999.0)
            w.setSingleStep(0.1)
            w.setValue(float(value) if value is not None else 0.0)
            w.valueChanged.connect(lambda v, k=field.name: self._onFieldChanged(k, v))
            return w
        elif ftype == int:
            w = QtWidgets.QSpinBox()
            w.setRange(-999999, 999999)
            w.setValue(int(value) if value is not None else 0)
            w.valueChanged.connect(lambda v, k=field.name: self._onFieldChanged(k, v))
            return w
        else:
            w = QtWidgets.QLineEdit(str(value) if value is not None else "")
            w.textChanged.connect(lambda v, k=field.name: self._onFieldChanged(k, v))
            return w

    def _onFieldChanged(self, key, value):
        self.data[key] = value
        self.valueChanged.emit(self.data)
