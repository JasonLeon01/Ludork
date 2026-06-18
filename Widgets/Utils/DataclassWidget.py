# -*- encoding: utf-8 -*-

import dataclasses
from typing import Dict, Any, Type, get_type_hints

from PyQt5 import QtWidgets, QtCore

from .ColourPickerDialog import ColourVarEditor
from .MetaVarTypes import getMetaVarTypes
from .TypedValueEditor import TypedValueEditor
from .VectorVarEditor import VectorVarEditor, isVectorVarType


class DataclassWidget(QtWidgets.QWidget):
    VALUE_CHANGED = QtCore.pyqtSignal(dict)

    def __init__(self, dc_type: Type, data: Dict[str, Any], parent=None):
        super().__init__(parent)
        self.dc_type = dc_type
        self.data = data if isinstance(data, dict) else {}
        try:
            self._type_hints = get_type_hints(dc_type)
        except Exception:
            self._type_hints = {}
        self._metaVarTypes = getMetaVarTypes(getattr(dc_type, "_meta", {}))
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
                    except Exception:
                        pass

            if val is not None:
                self.data[field.name] = val

            widget = self._createFieldWidget(field, val)
            if isinstance(widget, TypedValueEditor) and val is not None:
                self.data[field.name] = widget.getValue()
            layout.addRow(field.name, widget)
            self._inputs[field.name] = widget

    def _createFieldWidget(self, field: dataclasses.Field, value: Any):
        ftype = self._type_hints.get(field.name, field.type)
        varType = self._getFieldVarType(field)
        if varType == "ColourVar":
            w = ColourVarEditor(value, self)
            w.VALUE_CHANGED.connect(lambda v, k=field.name: self._onFieldChanged(k, v))
            return w
        if isVectorVarType(varType):
            w = VectorVarEditor(varType, value, self)
            w.VALUE_CHANGED.connect(lambda v, k=field.name: self._onFieldChanged(k, v))
            return w

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
            dc_widget.VALUE_CHANGED.connect(lambda v, k=field.name: self._onFieldChanged(k, v))
            gb_layout.addWidget(dc_widget)
            return gb

        w = TypedValueEditor(value, ftype, self)
        w.VALUE_CHANGED.connect(lambda v, k=field.name: self._onFieldChanged(k, v))
        return w

    def _getFieldVarType(self, field: dataclasses.Field) -> str:
        value = self._metaVarTypes.get(field.name)
        if not value:
            value = field.metadata.get("varType") or field.metadata.get("type")
        return value if isinstance(value, str) else ""

    def _onFieldChanged(self, key, value):
        self.data[key] = value
        self.VALUE_CHANGED.emit(self.data)
