# -*- encoding: utf-8 -*-

from __future__ import annotations

import ast
from types import UnionType
from typing import Any, Optional, Union, get_args, get_origin

from PyQt5 import QtWidgets, QtCore

from Utils.DataValue import EvalDataExpression


CONTAINER_EDITOR_MIN_WIDTH = 180
ROW_BUTTON_WIDTH = 24


def UnwrapOptionalType(valueType: Any) -> tuple[Any, bool]:
    origin = get_origin(valueType)
    if origin not in (Union, UnionType):
        return valueType, False
    args = [arg for arg in get_args(valueType) if arg is not type(None)]
    if len(args) == 1:
        return args[0], True
    return valueType, False


def GetSimpleContainerEditorType(paramType: Any) -> Any | None:
    if paramType in (list, dict):
        return paramType
    unwrapped, _nullable = UnwrapOptionalType(paramType)
    origin = get_origin(unwrapped)
    if origin in (list, dict) or unwrapped in (list, dict):
        return paramType
    return None


def FormatParamTypeName(paramType: Any) -> str:
    if isinstance(paramType, type):
        return paramType.__name__
    text = str(paramType).replace("typing.", "")
    if text.startswith("<class "):
        return text.split(".")[-1].rstrip("'>")
    return text


def ParseParamInitialValue(initVal: Any, paramType: Any) -> Any:
    if not isinstance(initVal, str):
        return initVal
    text = initVal.strip()
    if not text:
        return None if UnwrapOptionalType(paramType)[1] else initVal
    try:
        return ast.literal_eval(text)
    except (ValueError, SyntaxError):
        return initVal


def ParseBoolText(text: str) -> Any:
    value = EvalDataExpression(text)
    if value is None or isinstance(value, bool):
        return value
    return text


class TypedValueEditor(QtWidgets.QWidget):
    VALUE_CHANGED = QtCore.pyqtSignal(object)

    def __init__(self, value: Any = None, valueType: Any = None, parent=None):
        super().__init__(parent)
        self.valueType = valueType
        self._valueType, self._nullable = self._unwrapOptional(valueType)
        self._kind = ""
        self._editor = None
        self._childEditors = []
        self._rowWidgets = []
        self._dictRows: list[tuple[QtWidgets.QLineEdit, TypedValueEditor]] = []
        self._initUI(value)

    def getValue(self) -> Any:
        if self._kind == "bool":
            return self._editor.isChecked()
        if self._kind == "int":
            return self._editor.value()
        if self._kind == "float":
            return self._editor.value()
        if self._kind == "tuple":
            return tuple(editor.getValue() for editor in self._childEditors)
        if self._kind == "list":
            return [editor.getValue() for editor in self._childEditors]
        if self._kind == "dict":
            return self._buildDictValue()
        return self._coerceTextValue(self._editor.text())

    def setValue(self, value: Any, emit: bool = False) -> None:
        self._clearLayout()
        self._initUI(value)
        if emit:
            self.VALUE_CHANGED.emit(self.getValue())

    def setEditable(self, editable: bool) -> None:
        self.setEnabled(editable)
        widgets = self._editableWidgets()
        for widget in widgets:
            if isinstance(widget, QtWidgets.QLineEdit):
                widget.setReadOnly(not editable)
            else:
                widget.setEnabled(editable)

    def _initUI(self, value: Any) -> None:
        if self._nullable and value is None:
            self._initTextEditor(value)
            return

        kind = self._resolveKind(value)
        if kind == "bool":
            self._initBoolEditor(value)
        elif kind == "int":
            self._initIntEditor(value)
        elif kind == "float":
            self._initFloatEditor(value)
        elif kind == "tuple":
            self._initTupleEditor(value)
        elif kind == "list":
            self._initListEditor(value)
        elif kind == "dict":
            self._initDictEditor(value)
        else:
            self._initTextEditor(value)

    def _clearLayout(self) -> None:
        self._kind = ""
        self._editor = None
        self._childEditors = []
        self._rowWidgets = []
        self._dictRows = []
        layout = self.layout()
        if layout is not None:
            while layout.count():
                item = layout.takeAt(0)
                widget = item.widget()
                if widget is not None:
                    widget.setParent(None)
                    widget.deleteLater()
            QtWidgets.QWidget().setLayout(layout)

    def _initBoolEditor(self, value: Any) -> None:
        self._kind = "bool"
        layout = self._createHBox()
        self._editor = QtWidgets.QCheckBox(self)
        self._editor.setChecked(bool(value))
        self._editor.toggled.connect(lambda _: self.VALUE_CHANGED.emit(self.getValue()))
        layout.addWidget(self._editor)
        layout.addStretch()

    def _initIntEditor(self, value: Any) -> None:
        self._kind = "int"
        layout = self._createHBox()
        self._editor = QtWidgets.QSpinBox(self)
        self._editor.setRange(-2147483648, 2147483647)
        try:
            self._editor.setValue(int(value))
        except (TypeError, ValueError):
            self._editor.setValue(0)
        self._editor.valueChanged.connect(lambda _: self.VALUE_CHANGED.emit(self.getValue()))
        layout.addWidget(self._editor)

    def _initFloatEditor(self, value: Any) -> None:
        self._kind = "float"
        layout = self._createHBox()
        self._editor = QtWidgets.QDoubleSpinBox(self)
        self._editor.setRange(-999999999.0, 999999999.0)
        self._editor.setSingleStep(0.1)
        try:
            self._editor.setValue(float(value))
        except (TypeError, ValueError):
            self._editor.setValue(0.0)
        self._editor.valueChanged.connect(lambda _: self.VALUE_CHANGED.emit(self.getValue()))
        layout.addWidget(self._editor)

    def _initTextEditor(self, value: Any) -> None:
        self._kind = "text"
        layout = self._createHBox()
        self._editor = QtWidgets.QLineEdit(self)
        self._editor.setText(self._textFromValue(value))
        self._editor.textChanged.connect(lambda _: self.VALUE_CHANGED.emit(self.getValue()))
        layout.addWidget(self._editor)

    def _initTupleEditor(self, value: Any) -> None:
        self._kind = "tuple"
        layout = self._createHBox()
        values, valueTypes = self._normaliseSequence(value, tuple)
        for item, itemType in zip(values, valueTypes):
            editor = TypedValueEditor(item, itemType, self)
            editor.VALUE_CHANGED.connect(lambda _: self.VALUE_CHANGED.emit(self.getValue()))
            self._childEditors.append(editor)
            layout.addWidget(editor)
        self._elementWidgets = self._editableWidgets()

    def _initListEditor(self, value: Any) -> None:
        self._kind = "list"
        self.setMinimumWidth(CONTAINER_EDITOR_MIN_WIDTH)
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)
        addBtn = QtWidgets.QPushButton("+", self)
        addBtn.setFixedWidth(ROW_BUTTON_WIDTH)
        addBtn.clicked.connect(lambda _: self._appendListItem(layout))
        layout.addWidget(addBtn)
        values, valueTypes = self._normaliseSequence(value, list)
        for item, itemType in zip(values, valueTypes):
            self._addListRow(layout, item, itemType)
        self._elementWidgets = self._editableWidgets()

    def _initDictEditor(self, value: Any) -> None:
        self._kind = "dict"
        self._dictRows = []
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)
        keyType, valueType = self._getDictKeyValueTypes()
        addBtn = QtWidgets.QPushButton("+", self)
        addBtn.setFixedWidth(ROW_BUTTON_WIDTH)
        addBtn.clicked.connect(lambda _: self._appendDictItem(layout, keyType, valueType))
        layout.addWidget(addBtn)
        items = value.items() if isinstance(value, dict) else []
        for key, item in items:
            self._addDictRow(layout, key, item, keyType, valueType)
        self._elementWidgets = self._editableWidgets()

    def _addDictRow(
        self,
        layout: QtWidgets.QVBoxLayout,
        key: Any,
        item: Any,
        keyType: Any,
        valueType: Any,
    ) -> None:
        row = QtWidgets.QWidget(self)
        rowLayout = QtWidgets.QHBoxLayout(row)
        rowLayout.setContentsMargins(0, 0, 0, 0)
        rowLayout.setSpacing(2)
        keyEdit = QtWidgets.QLineEdit(row)
        keyEdit.setText(self._textFromValue(key))
        keyEdit.textChanged.connect(lambda _: self.VALUE_CHANGED.emit(self.getValue()))
        valueEditor = TypedValueEditor(item, valueType, row)
        valueEditor.VALUE_CHANGED.connect(lambda _: self.VALUE_CHANGED.emit(self.getValue()))
        self._childEditors.append(valueEditor)
        self._dictRows.append((keyEdit, valueEditor))
        self._rowWidgets.append(row)
        rowLayout.addWidget(keyEdit, 1)
        rowLayout.addWidget(valueEditor, 2)
        removeBtn = QtWidgets.QPushButton("-", row)
        removeBtn.setFixedWidth(ROW_BUTTON_WIDTH)
        removeBtn.clicked.connect(lambda _: self._removeDictRow(row))
        rowLayout.addWidget(removeBtn)
        layout.insertWidget(max(0, layout.count() - 1), row)

    def _appendDictItem(self, layout: QtWidgets.QVBoxLayout, keyType: Any, valueType: Any) -> None:
        self._addDictRow(layout, self._defaultValueForType(keyType), self._defaultValueForType(valueType), keyType, valueType)
        self._elementWidgets = self._editableWidgets()
        self.VALUE_CHANGED.emit(self.getValue())

    def _removeDictRow(self, row: QtWidgets.QWidget) -> None:
        if row not in self._rowWidgets:
            return
        index = self._rowWidgets.index(row)
        self._rowWidgets.pop(index)
        self._dictRows.pop(index)
        self._childEditors.pop(index)
        row.setParent(None)
        row.deleteLater()
        self._elementWidgets = self._editableWidgets()
        self.VALUE_CHANGED.emit(self.getValue())

    def _buildDictValue(self) -> dict[Any, Any]:
        keyType, valueType = self._getDictKeyValueTypes()
        result: dict[Any, Any] = {}
        for keyEdit, valueEditor in self._dictRows:
            keyText = keyEdit.text()
            key = self._coerceTextValueForType(keyText, keyType)
            if keyText.strip() == "":
                continue
            result[key] = valueEditor.getValue()
        return result

    def _getDictKeyValueTypes(self) -> tuple[Any, Any]:
        args = list(get_args(self._valueType))
        if len(args) >= 2:
            return args[0], args[1]
        if len(args) == 1:
            return args[0], Any
        return str, Any

    def _coerceTextValueForType(self, text: str, targetType: Any) -> Any:
        targetType, nullable = UnwrapOptionalType(targetType)
        if nullable and text.strip() in ("", "None"):
            return None
        if targetType is str:
            return text
        if targetType is bool:
            return ParseBoolText(text)
        if targetType is int:
            try:
                return int(text)
            except ValueError:
                return text
        if targetType is float:
            try:
                return float(text)
            except ValueError:
                return text
        origin = get_origin(targetType)
        if origin in (list, tuple, dict) or targetType in (list, tuple, dict, Any):
            try:
                return ast.literal_eval(text)
            except (ValueError, SyntaxError):
                return text
        return text

    def _addListRow(self, layout: QtWidgets.QVBoxLayout, item: Any, itemType: Any) -> None:
        row = QtWidgets.QWidget(self)
        rowLayout = QtWidgets.QHBoxLayout(row)
        rowLayout.setContentsMargins(0, 0, 0, 0)
        rowLayout.setSpacing(2)
        editor = TypedValueEditor(item, itemType, row)
        editor.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Preferred)
        editor.VALUE_CHANGED.connect(lambda _: self.VALUE_CHANGED.emit(self.getValue()))
        self._childEditors.append(editor)
        self._rowWidgets.append(row)
        rowLayout.addWidget(editor, 1)
        removeBtn = QtWidgets.QPushButton("-", row)
        removeBtn.setFixedWidth(ROW_BUTTON_WIDTH)
        removeBtn.clicked.connect(lambda _: self._removeListRow(row))
        rowLayout.addWidget(removeBtn)
        layout.insertWidget(max(0, layout.count() - 1), row)

    def _appendListItem(self, layout: QtWidgets.QVBoxLayout) -> None:
        itemType = self._getListItemType()
        self._addListRow(layout, self._defaultValueForType(itemType), itemType)
        self._elementWidgets = self._editableWidgets()
        self.VALUE_CHANGED.emit(self.getValue())

    def _removeListRow(self, row: QtWidgets.QWidget) -> None:
        if row not in self._rowWidgets:
            return
        index = self._rowWidgets.index(row)
        self._rowWidgets.pop(index)
        self._childEditors.pop(index)
        row.setParent(None)
        row.deleteLater()
        self._elementWidgets = self._editableWidgets()
        self.VALUE_CHANGED.emit(self.getValue())

    def _createHBox(self) -> QtWidgets.QHBoxLayout:
        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)
        return layout

    def _resolveKind(self, value: Any) -> str:
        valueType = self._valueType
        origin = get_origin(valueType)
        if origin is tuple or valueType is tuple:
            return "tuple"
        if origin is list or valueType is list:
            return "list"
        if origin is dict or valueType is dict:
            return "dict"
        if valueType is bool or isinstance(value, bool):
            return "bool"
        if valueType is int or (isinstance(value, int) and not isinstance(value, bool)):
            return "int"
        if valueType is float or isinstance(value, float):
            return "float"
        if isinstance(value, tuple):
            return "tuple"
        if isinstance(value, list):
            return "list"
        if isinstance(value, dict):
            return "dict"
        return "text"

    def _normaliseSequence(self, value: Any, containerType: type) -> tuple[list, list]:
        if isinstance(value, (list, tuple)):
            values = list(value)
        else:
            values = []

        args = list(get_args(self._valueType))
        if containerType is tuple and args and args[-1] is not Ellipsis:
            valueTypes = args
            while len(values) < len(valueTypes):
                values.append(self._defaultValueForType(valueTypes[len(values)]))
            values = values[: len(valueTypes)]
        else:
            itemType = self._getListItemType()
            valueTypes = [itemType for _ in values]

        if len(valueTypes) < len(values):
            valueTypes.extend(type(item) for item in values[len(valueTypes):])
        return values, valueTypes

    def _getListItemType(self) -> Any:
        args = get_args(self._valueType)
        if not args:
            return Any
        if len(args) >= 2 and args[1] is Ellipsis:
            return self._normaliseListItemType(args[0])
        if len(args) == 1:
            return self._normaliseListItemType(args[0])
        return Any

    def _normaliseListItemType(self, itemType: Any) -> Any:
        itemType, nullable = self._unwrapOptional(itemType)
        if itemType is Any:
            itemType = str
        if nullable:
            return Optional[itemType]
        return itemType

    def _defaultValueForType(self, valueType: Any) -> Any:
        valueType, nullable = self._unwrapOptional(valueType)
        if nullable:
            return None
        origin = get_origin(valueType)
        if valueType is bool:
            return False
        if valueType is int:
            return 0
        if valueType is float:
            return 0.0
        if valueType is str:
            return ""
        if origin is tuple or valueType is tuple:
            args = list(get_args(valueType))
            if args and args[-1] is not Ellipsis:
                return tuple(self._defaultValueForType(arg) for arg in args)
            return tuple()
        if origin is list or valueType is list:
            return []
        if origin is dict or valueType is dict:
            return {}
        return ""

    def _coerceTextValue(self, text: str) -> Any:
        targetType = self._valueType
        if self._nullable and text.strip() in ("", "None"):
            return None
        if targetType is str:
            return text
        if targetType is bool:
            return ParseBoolText(text)
        if targetType is int:
            try:
                return int(text)
            except ValueError:
                return text
        if targetType is float:
            try:
                return float(text)
            except ValueError:
                return text
        origin = get_origin(targetType)
        if origin in (list, tuple, dict) or targetType in (list, tuple, dict, Any):
            try:
                return ast.literal_eval(text)
            except (ValueError, SyntaxError):
                return text
        return text

    def _textFromValue(self, value: Any) -> str:
        if self._nullable and value is None:
            return ""
        if value is None:
            return ""
        return str(value)

    def _unwrapOptional(self, valueType: Any) -> tuple[Any, bool]:
        return UnwrapOptionalType(valueType)

    def _editableWidgets(self) -> list:
        result = []
        if isinstance(self._editor, QtWidgets.QWidget):
            result.append(self._editor)
        for keyEdit, _valueEditor in self._dictRows:
            result.append(keyEdit)
        for child in self._childEditors:
            result.extend(child._editableWidgets())
        return result
