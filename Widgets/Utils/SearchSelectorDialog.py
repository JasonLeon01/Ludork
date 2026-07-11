# -*- encoding: utf-8 -*-

from collections.abc import Callable, Sequence
from typing import Optional

from PyQt5 import QtCore, QtWidgets

from EditorGlobal.QmlDialogHost import QmlDialogHost


class SearchSelectorDialog(QmlDialogHost):
    resultReady = QtCore.pyqtSignal(str)

    def __init__(
        self,
        parent: Optional[QtWidgets.QWidget],
        title: str,
        items: Sequence[str],
        currentValue: str = "",
        size: Optional[QtCore.QSize] = None,
        minimumSize: Optional[QtCore.QSize] = None,
    ) -> None:
        if size is None:
            size = QtCore.QSize(360, 480)
        if minimumSize is None:
            minimumSize = QtCore.QSize(300, 280)
        super().__init__(parent, title, size, minimumSize)
        self._selected = currentValue
        self.loadQml(
            "Dialogs/SearchSelectorDialog.qml",
            {
                "searchSelectorItems": list(items),
                "searchSelectorInitial": currentValue,
            },
        )

    def _applyResult(self, result: object) -> bool:
        if isinstance(result, dict):
            self._selected = str(result.get("selected", ""))
            if self._selected:
                self.resultReady.emit(self._selected)
        return True

    def getSelected(self) -> str:
        return self._selected


def OpenSearchSelectorDialog(
    parent: Optional[QtWidgets.QWidget],
    title: str,
    items: Sequence[str],
    currentValue: str = "",
    *,
    onSelected: Optional[Callable[[str], None]] = None,
    onCancelled: Optional[Callable[[], None]] = None,
    size: Optional[QtCore.QSize] = None,
    minimumSize: Optional[QtCore.QSize] = None,
) -> SearchSelectorDialog:
    dialog = SearchSelectorDialog(parent, title, items, currentValue, size=size, minimumSize=minimumSize)
    if onSelected is not None:
        dialog.resultReady.connect(onSelected)
    if onCancelled is not None:
        dialog.rejected.connect(onCancelled)
    dialog.open()
    return dialog
