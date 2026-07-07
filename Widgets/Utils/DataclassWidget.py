# -*- encoding: utf-8 -*-

import logging
from typing import Dict, Any, Type, get_type_hints, Optional, Set

from PyQt5 import QtWidgets, QtCore

from .ColourPickerDialog import ColourVarEditor
from .MetaVarTypes import _PROGRESS_VAR_TYPE, GetMetaVarTypes, GetProgressVarRanges
from .ProgressVarEditor import ProgressVarEditor
from .StructuredFields import IsStructuredType, StructuredFields, StructuredValueToDict
from .TypedValueEditor import TypedValueEditor
from .VariableNameLabel import VariableNameLabel
from .VectorVarEditor import VectorVarEditor, IsVectorVarType

log = logging.getLogger(__name__)


class DataclassWidget(QtWidgets.QWidget):
    VALUE_CHANGED = QtCore.pyqtSignal(dict)

    def __init__(
        self,
        dc_type: Type,
        data: Dict[str, Any],
        parent=None,
        readOnlyFields: Optional[Set[str]] = None,
    ):
        super().__init__(parent)
        self.dc_type = dc_type
        self.data = data if isinstance(data, dict) else StructuredValueToDict(data)
        self._readOnlyFields = readOnlyFields or set()
        try:
            self._type_hints = get_type_hints(dc_type)
        except (NameError, TypeError, AttributeError) as e:
            log.warning("Failed to resolve type hints for %s: %s", dc_type, e)
            self._type_hints = {}
        meta = self._getMergedMeta(dc_type)
        self._metaVarTypes = GetMetaVarTypes(meta)
        self._progressVarRanges = GetProgressVarRanges(meta)
        self._displayNames = self._getVariableDisplayMap(meta, "VariableDisplayNames")
        self._displayDescs = self._getVariableDisplayMap(meta, "VariableDisplayDescs")
        self._inputs = {}
        self._initUI()

    def _getMergedMeta(self, dc_type: Type) -> Dict[str, Any]:
        result: Dict[str, Any] = {}
        for base in list(reversed(dc_type.mro())):
            meta = getattr(base, "__dict__", {}).get("_meta")
            if not isinstance(meta, dict):
                continue
            for key, value in meta.items():
                if isinstance(value, dict) and isinstance(result.get(key), dict):
                    merged = dict(result[key])
                    merged.update(value)
                    result[key] = merged
                else:
                    result[key] = value
        return result

    def _getVariableDisplayMap(self, meta: Any, metaKey: str) -> Dict[str, str]:
        if not isinstance(meta, dict):
            return {}
        value = meta.get(metaKey)
        if not isinstance(value, dict):
            return {}
        result: Dict[str, str] = {}
        for name, expr in value.items():
            if isinstance(name, str) and isinstance(expr, str):
                result[name] = self._evalMetaText(expr)
        return result

    def _evalMetaText(self, value: str) -> str:
        try:
            return str(eval(value))
        except Exception:
            return value

    def _getFieldDisplayName(self, name: str) -> str:
        value = self._displayNames.get(name)
        return value if isinstance(value, str) and value else name

    def _getFieldDisplayDesc(self, name: str) -> str:
        value = self._displayDescs.get(name)
        return value if isinstance(value, str) else ""

    def _initUI(self):
        layout = QtWidgets.QFormLayout(self)
        layout.setContentsMargins(10, 0, 0, 0)

        for field in StructuredFields(self.dc_type, self.data):
            val = self.data.get(field.name)

            if val is None:
                val = field.default

            if val is not None:
                self.data[field.name] = val

            widget = self._createFieldWidget(field, val)
            if field.name in self._readOnlyFields:
                self._setFieldReadOnly(widget)
            if isinstance(widget, TypedValueEditor) and val is not None:
                self.data[field.name] = widget.getValue()
            desc = self._getFieldDisplayDesc(field.name)
            label = VariableNameLabel(self._getFieldDisplayName(field.name), field.name)
            if desc:
                label.setToolTip(desc)
                widget.setToolTip(desc)
            layout.addRow(label, widget)
            self._inputs[field.name] = widget

    def _createFieldWidget(self, field, value: Any):
        ftype = self._type_hints.get(field.name, field.type)
        varType = self._getFieldVarType(field)
        if varType == "ColourVar":
            w = ColourVarEditor(value, self)
            w.VALUE_CHANGED.connect(lambda v, k=field.name: self._onFieldChanged(k, v))
            return w
        if IsVectorVarType(varType):
            w = VectorVarEditor(varType, value, self)
            w.VALUE_CHANGED.connect(lambda v, k=field.name: self._onFieldChanged(k, v))
            return w
        if varType == _PROGRESS_VAR_TYPE:
            w = ProgressVarEditor(value, self._progressVarRanges.get(field.name), self)
            w.VALUE_CHANGED.connect(lambda v, k=field.name: self._onFieldChanged(k, v))
            return w

        if IsStructuredType(ftype):
            value = value if isinstance(value, dict) else StructuredValueToDict(value)

            gb = QtWidgets.QGroupBox()
            gb.setFlat(True)
            gb.setStyleSheet(
                "QGroupBox { border: 1px solid #444; margin-top: 0.5em; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px 0 3px; }"
            )
            gb_layout = QtWidgets.QVBoxLayout(gb)
            gb_layout.setContentsMargins(0, 5, 0, 0)

            dc_widget = DataclassWidget(ftype, value, readOnlyFields=self._readOnlyFields)
            dc_widget.VALUE_CHANGED.connect(lambda v, k=field.name: self._onFieldChanged(k, v))
            gb_layout.addWidget(dc_widget)
            return gb

        w = TypedValueEditor(value, ftype, self)
        w.VALUE_CHANGED.connect(lambda v, k=field.name: self._onFieldChanged(k, v))
        return w

    def _getFieldVarType(self, field) -> str:
        value = self._metaVarTypes.get(field.name)
        if not value:
            value = getattr(field, "varType", "")
        return value if isinstance(value, str) else ""

    def _setFieldReadOnly(self, widget: QtWidgets.QWidget) -> None:
        if isinstance(widget, (ColourVarEditor, VectorVarEditor, ProgressVarEditor)):
            widget.setEditable(False)
            return
        if isinstance(widget, QtWidgets.QGroupBox):
            nested = widget.findChild(DataclassWidget)
            if nested is not None:
                nested.setReadOnly(True)
            return
        widget.setEnabled(False)
        if isinstance(widget, (QtWidgets.QLineEdit, QtWidgets.QPlainTextEdit, QtWidgets.QTextEdit)):
            widget.setReadOnly(True)

    def setReadOnly(self, readOnly: bool) -> None:
        for widget in self._inputs.values():
            if readOnly:
                self._setFieldReadOnly(widget)
            else:
                widget.setEnabled(True)

    def _onFieldChanged(self, key, value):
        self.data[key] = value
        self.VALUE_CHANGED.emit(self.data)
