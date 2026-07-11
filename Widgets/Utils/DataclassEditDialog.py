# -*- encoding: utf-8 -*-

import logging
from collections.abc import Callable
from typing import Any, Optional, Set, get_type_hints

from PyQt5 import QtCore, QtWidgets

from EditorGlobal.QmlDialogHost import QmlDialogHost
from .ColourPickerDialog import ColourVarEditor
from .DataclassWidget import DataclassWidget
from .MetaVarTypes import _PROGRESS_VAR_TYPE, GetMetaVarTypes, GetProgressVarRanges
from .ProgressVarEditor import ProgressVarEditor
from .StructuredFields import StructuredFields
from .TypedValueEditor import TypedValueEditor
from .VectorVarEditor import VectorVarEditor, IsVectorVarType

log = logging.getLogger(__name__)

_FOOTER_HEIGHT = 52


def _attachHybridFooter(dialog: QmlDialogHost) -> None:
    dialog.loadQml("Dialogs/HybridDialogFooter.qml")
    dialog._quickWidget.setFixedHeight(_FOOTER_HEIGHT)


class DataclassEditDialog(QmlDialogHost):
    def __init__(self, parent: QtWidgets.QWidget, data_obj: object, title: str = "Edit Data") -> None:
        super().__init__(parent, title)
        self.data_obj = data_obj
        try:
            self._type_hints = get_type_hints(type(data_obj))
        except (NameError, TypeError, AttributeError) as e:
            log.warning("Failed to resolve type hints for %s: %s", type(data_obj), e)
            self._type_hints = {}
        meta = getattr(type(data_obj), "_meta", {})
        self._metaVarTypes = GetMetaVarTypes(meta)
        self._progressVarRanges = GetProgressVarRanges(meta)
        self._initUI()
        _attachHybridFooter(self)
        self._fitToContent()

    def _initUI(self) -> None:
        formWidget = QtWidgets.QWidget(self)
        layout = QtWidgets.QFormLayout(formWidget)
        self.inputs: dict[str, QtWidgets.QWidget] = {}

        fields = StructuredFields(type(self.data_obj), self.data_obj)
        for field in fields:
            value = getattr(self.data_obj, field.name)
            field_type = self._type_hints.get(field.name, field.type)
            varType = self._getFieldVarType(field)
            if varType == "ColourVar":
                widget = ColourVarEditor(value, formWidget)
            elif IsVectorVarType(varType):
                widget = VectorVarEditor(varType, value, formWidget)
            elif varType == _PROGRESS_VAR_TYPE:
                widget = ProgressVarEditor(value, self._progressVarRanges.get(field.name), formWidget)
            else:
                widget = TypedValueEditor(value, field_type, formWidget)

            layout.addRow(field.name, widget)
            self.inputs[field.name] = widget

        outerLayout = self.layout()
        if isinstance(outerLayout, QtWidgets.QVBoxLayout):
            outerLayout.insertWidget(0, formWidget)
        self._formWidget = formWidget

    def _fitToContent(self) -> None:
        formHeight = self._formWidget.sizeHint().height()
        minHeight = max(160, formHeight + _FOOTER_HEIGHT + 16)
        self.setMinimumSize(400, minHeight)
        self.resize(max(480, self.minimumWidth()), minHeight)

    def _applyResult(self, result: object) -> bool:
        for name, widget in self.inputs.items():
            if isinstance(widget, (TypedValueEditor, ColourVarEditor, VectorVarEditor, ProgressVarEditor)):
                setattr(self.data_obj, name, widget.getValue())
        return True

    def _getFieldVarType(self, field: object) -> str:
        value = self._metaVarTypes.get(field.name)
        if not value:
            value = getattr(field, "varType", "")
        return value if isinstance(value, str) else ""


class DataclassWidgetDialog(QmlDialogHost):
    def __init__(
        self,
        parent: QtWidgets.QWidget,
        title: str,
        dataType: type,
        data: dict[str, Any],
        readOnlyFields: Optional[Set[str]] = None,
    ) -> None:
        super().__init__(parent, title)
        self.widget = DataclassWidget(dataType, data, self, readOnlyFields=readOnlyFields)
        outerLayout = self.layout()
        if isinstance(outerLayout, QtWidgets.QVBoxLayout):
            outerLayout.insertWidget(0, self.widget)
        _attachHybridFooter(self)
        self._fitToContent()

    def _fitToContent(self) -> None:
        contentHeight = self.widget.sizeHint().height()
        minHeight = max(200, contentHeight + _FOOTER_HEIGHT + 16)
        self.setMinimumSize(420, minHeight)
        self.resize(max(520, self.minimumWidth()), min(minHeight, 640))

    def _applyResult(self, result: object) -> bool:
        return True


def OpenDataclassEditDialog(
    parent: QtWidgets.QWidget,
    data_obj: object,
    title: str = "Edit Data",
    *,
    onAccepted: Optional[Callable[[], None]] = None,
    onRejected: Optional[Callable[[], None]] = None,
) -> DataclassEditDialog:
    dialog = DataclassEditDialog(parent, data_obj, title)
    if onAccepted is not None:
        dialog.accepted.connect(onAccepted)
    if onRejected is not None:
        dialog.rejected.connect(onRejected)
    dialog.open()
    return dialog


def OpenDataclassWidgetDialog(
    parent: QtWidgets.QWidget,
    title: str,
    dataType: type,
    data: dict[str, Any],
    readOnlyFields: Optional[Set[str]] = None,
    *,
    onAccepted: Optional[Callable[[DataclassWidgetDialog], None]] = None,
    onRejected: Optional[Callable[[], None]] = None,
) -> DataclassWidgetDialog:
    dialog = DataclassWidgetDialog(parent, title, dataType, data, readOnlyFields)
    if onAccepted is not None:
        dialog.accepted.connect(lambda: onAccepted(dialog))
    if onRejected is not None:
        dialog.rejected.connect(onRejected)
    dialog.open()
    return dialog
