# -*- encoding: utf-8 -*-

from typing import get_type_hints

from PyQt5 import QtWidgets

from .ColourPickerDialog import ColourVarEditor
from .MetaVarTypes import getMetaVarTypes
from .StructuredFields import structuredFields
from .TypedValueEditor import TypedValueEditor
from .VectorVarEditor import VectorVarEditor, isVectorVarType


class DataclassEditDialog(QtWidgets.QDialog):
    def __init__(self, parent, data_obj, title="Edit Data"):
        super().__init__(parent)
        self.setWindowTitle(title)
        self.data_obj = data_obj
        try:
            self._type_hints = get_type_hints(type(data_obj))
        except Exception:
            self._type_hints = {}
        self._metaVarTypes = getMetaVarTypes(getattr(type(data_obj), "_meta", {}))
        self._initUI()

    def _initUI(self):
        layout = QtWidgets.QFormLayout(self)
        self.inputs = {}

        fields = structuredFields(type(self.data_obj), self.data_obj)
        for field in fields:
            value = getattr(self.data_obj, field.name)
            field_type = self._type_hints.get(field.name, field.type)
            varType = self._getFieldVarType(field)
            if varType == "ColourVar":
                widget = ColourVarEditor(value, self)
            elif isVectorVarType(varType):
                widget = VectorVarEditor(varType, value, self)
            else:
                widget = TypedValueEditor(value, field_type, self)
            
            layout.addRow(field.name, widget)
            self.inputs[field.name] = widget
            
        buttons = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        ok_btn = buttons.button(QtWidgets.QDialogButtonBox.Ok)
        cancel_btn = buttons.button(QtWidgets.QDialogButtonBox.Cancel)
        if ok_btn:
            ok_btn.setText(ELOC("CONFIRM"))
        if cancel_btn:
            cancel_btn.setText(ELOC("CANCEL"))
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def accept(self):
        for name, widget in self.inputs.items():
            if isinstance(widget, (TypedValueEditor, ColourVarEditor, VectorVarEditor)):
                setattr(self.data_obj, name, widget.getValue())
        super().accept()

    def _getFieldVarType(self, field) -> str:
        value = self._metaVarTypes.get(field.name)
        if not value:
            value = getattr(field, "varType", "")
        return value if isinstance(value, str) else ""
