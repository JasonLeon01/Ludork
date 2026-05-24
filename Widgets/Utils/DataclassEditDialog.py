# -*- encoding: utf-8 -*-

import dataclasses
from typing import get_type_hints

from PyQt5 import QtWidgets

from .TypedValueEditor import TypedValueEditor


class DataclassEditDialog(QtWidgets.QDialog):
    def __init__(self, parent, data_obj, title="Edit Data"):
        super().__init__(parent)
        self.setWindowTitle(title)
        self.data_obj = data_obj
        try:
            self._type_hints = get_type_hints(type(data_obj))
        except Exception:
            self._type_hints = {}
        self._initUI()

    def _initUI(self):
        layout = QtWidgets.QFormLayout(self)
        self.inputs = {}
        
        fields = dataclasses.fields(self.data_obj)
        for field in fields:
            value = getattr(self.data_obj, field.name)
            field_type = self._type_hints.get(field.name, field.type)
            widget = TypedValueEditor(value, field_type, self)
            
            layout.addRow(field.name, widget)
            self.inputs[field.name] = widget
            
        buttons = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def accept(self):
        for name, widget in self.inputs.items():
            setattr(self.data_obj, name, widget.getValue())
        super().accept()
