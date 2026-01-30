# -*- encoding: utf-8 -*-

from PyQt5 import QtWidgets, QtCore
import dataclasses

class DataclassEditDialog(QtWidgets.QDialog):
    def __init__(self, parent, data_obj, title="Edit Data"):
        super().__init__(parent)
        self.setWindowTitle(title)
        self.data_obj = data_obj
        self._initUI()

    def _initUI(self):
        layout = QtWidgets.QFormLayout(self)
        self.inputs = {}
        
        fields = dataclasses.fields(self.data_obj)
        for field in fields:
            value = getattr(self.data_obj, field.name)
            field_type = field.type
            
            widget = None
            if field_type == bool:
                widget = QtWidgets.QCheckBox()
                widget.setChecked(bool(value))
            elif field_type == float:
                widget = QtWidgets.QDoubleSpinBox()
                widget.setRange(-999999.0, 999999.0)
                widget.setSingleStep(0.1)
                widget.setValue(float(value))
            elif field_type == int:
                widget = QtWidgets.QSpinBox()
                widget.setRange(-999999, 999999)
                widget.setValue(int(value))
            else:
                widget = QtWidgets.QLineEdit()
                widget.setText(str(value))
            
            layout.addRow(field.name, widget)
            self.inputs[field.name] = (field_type, widget)
            
        buttons = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def accept(self):
        for name, (ftype, widget) in self.inputs.items():
            val = None
            if ftype == bool:
                val = widget.isChecked()
            elif ftype == float:
                val = widget.value()
            elif ftype == int:
                val = widget.value()
            else:
                val = widget.text()
            setattr(self.data_obj, name, val)
        super().accept()
