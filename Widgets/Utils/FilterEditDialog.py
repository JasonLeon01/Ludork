# -*- encoding: utf-8 -*-

from collections.abc import Callable, Mapping
from typing import Optional

from PyQt5 import QtCore, QtWidgets

from EditorGlobal.QmlDialogHost import QmlDialogHost

FilterData = dict[str, float | dict[str, float]]


def _toFloat(value: object, default: float) -> float:
    if not isinstance(value, (int, float, str)):
        return default
    try:
        return float(value)
    except ValueError:
        return default


class FilterEditDialog(QmlDialogHost):
    resultReady = QtCore.pyqtSignal(dict)

    def __init__(
        self,
        parent: QtWidgets.QWidget,
        filterData: Mapping[str, object],
        filterType: str,
        title: Optional[str] = None,
    ) -> None:
        self._filterData = filterData
        self._filterType = filterType
        if title is None:
            title = ELOC("EDIT_BGM_FILTER") if filterType == "bgm" else ELOC("EDIT_BGS_FILTER")
        labels = [ELOC("FILTER_OFFSET"), ELOC("FILTER_PITCH"), ELOC("FILTER_PAN"), ELOC("FILTER_VOLUME")]
        if filterType == "bgm":
            labels.append(ELOC("FILTER_LOOP_POINT"))
        dialogSize = QtCore.QSize(403, 272) if filterType == "bgm" else QtCore.QSize(320, 230)
        super().__init__(parent, title, dialogSize, QtCore.QSize(320, 0), labels)
        loopPointValue = filterData.get("loopPoint", {})
        loopPoint: Mapping[object, object] = loopPointValue if isinstance(loopPointValue, dict) else {}
        self._result: FilterData = {}
        self.loadQml(
            "Dialogs/FilterEditDialog.qml",
            {
                "filterIsBgm": filterType == "bgm",
                "filterInitialData": {
                    "offset": _toFloat(filterData.get("offset"), 0.0),
                    "pitch": _toFloat(filterData.get("pitch"), 1.0),
                    "pan": _toFloat(filterData.get("pan"), 0.0),
                    "volume": _toFloat(filterData.get("volume"), 100.0),
                    "loopStart": _toFloat(loopPoint.get("start"), 0.0),
                    "loopEnd": _toFloat(loopPoint.get("end"), 0.0),
                },
            },
        )

    def _applyResult(self, result: object) -> bool:
        if not isinstance(result, dict):
            return False
        values = {
            "offset": _toFloat(result.get("offset"), 0.0),
            "pitch": _toFloat(result.get("pitch"), 1.0),
            "pan": _toFloat(result.get("pan"), 0.0),
            "volume": _toFloat(result.get("volume"), 100.0),
            "loopStart": _toFloat(result.get("loopStart"), 0.0),
            "loopEnd": _toFloat(result.get("loopEnd"), 0.0),
        }
        self._result = self._buildResult(values)
        self.resultReady.emit(dict(self._result))
        return True

    def _buildResult(self, values: dict[str, float]) -> FilterData:
        result: FilterData = {}
        if values["offset"] > 0.0:
            result["offset"] = values["offset"]
        if values["pitch"] != 1.0:
            result["pitch"] = values["pitch"]
        if values["pan"] != 0.0:
            result["pan"] = values["pan"]
        if values["volume"] != 100.0:
            result["volume"] = values["volume"]
        if self._filterType == "bgm" and (values["loopStart"] > 0.0 or values["loopEnd"] > 0.0):
            result["loopPoint"] = {"start": values["loopStart"], "end": values["loopEnd"]}
        return result

    def getResult(self) -> FilterData:
        return dict(self._result)


def EditFilterData(
    parent: QtWidgets.QWidget,
    filterData: Mapping[str, object],
    filterType: str,
    title: Optional[str] = None,
    onAccepted: Optional[Callable[[FilterData], None]] = None,
) -> FilterEditDialog:
    dlg = FilterEditDialog(parent, filterData, filterType, title)
    if onAccepted is not None:
        dlg.resultReady.connect(onAccepted)
    dlg.open()
    return dlg
