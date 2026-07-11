# -*- encoding: utf-8 -*-

from collections.abc import Callable, Mapping, Sequence
from typing import Any, Optional

from PyQt5 import QtCore, QtWidgets

from EditorGlobal.QmlDialogHost import QmlDialogHost


class FormDialog(QmlDialogHost):
    resultReady = QtCore.pyqtSignal(dict)

    def __init__(
        self,
        parent: QtWidgets.QWidget,
        title: str,
        fields: Sequence[Mapping[str, Any]],
        size: Optional[QtCore.QSize] = None,
        minimumSize: Optional[QtCore.QSize] = None,
    ) -> None:
        labels = [str(field.get("label", "")) for field in fields]
        rowHeight = 42
        if size is None:
            size = QtCore.QSize(460, max(160, 100 + len(fields) * rowHeight))
        if minimumSize is None:
            minimumSize = QtCore.QSize(400, max(140, 80 + len(fields) * rowHeight))
        super().__init__(parent, title, size, minimumSize, labels)
        self._result: dict[str, Any] = {}
        self.loadQml("Dialogs/FormDialog.qml", {"formFields": list(fields)})

    def getResult(self) -> dict[str, Any]:
        return dict(self._result)

    def _applyResult(self, result: object) -> bool:
        if not isinstance(result, dict):
            return False
        self._result = {str(key): value for key, value in result.items()}
        self.resultReady.emit(self._result)
        return True


def OpenFormDialog(
    parent: QtWidgets.QWidget,
    title: str,
    fields: Sequence[Mapping[str, Any]],
    *,
    size: Optional[QtCore.QSize] = None,
    minimumSize: Optional[QtCore.QSize] = None,
    onAccepted: Optional[Callable[[dict[str, Any]], None]] = None,
    onRejected: Optional[Callable[[], None]] = None,
) -> FormDialog:
    dialog = FormDialog(parent, title, fields, size=size, minimumSize=minimumSize)
    if onAccepted is not None:
        dialog.resultReady.connect(onAccepted)
    if onRejected is not None:
        dialog.rejected.connect(onRejected)
    dialog.open()
    return dialog
