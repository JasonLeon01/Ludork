# -*- encoding: utf-8 -*-

from typing import Any, Dict, Optional
from PyQt5 import QtWidgets


class FilterEditDialog(QtWidgets.QDialog):
    def __init__(
        self,
        parent: QtWidgets.QWidget,
        filterData: Dict[str, Any],
        filterType: str,
        title: Optional[str] = None,
    ) -> None:
        super().__init__(parent)
        self._filterData = filterData
        self._filterType = filterType
        if title is None:
            title = ELOC("EDIT_BGM_FILTER") if filterType == "bgm" else ELOC("EDIT_BGS_FILTER")
        self.setWindowTitle(title)
        self.setMinimumWidth(320)
        form = QtWidgets.QFormLayout(self)
        form.setContentsMargins(12, 12, 12, 12)
        form.setSpacing(8)

        self._offsetSpin = QtWidgets.QDoubleSpinBox(self)
        self._offsetSpin.setRange(0.0, 999999.0)
        self._offsetSpin.setSingleStep(0.1)
        self._offsetSpin.setValue(float(filterData.get("offset", 0.0)))
        form.addRow(ELOC("FILTER_OFFSET"), self._offsetSpin)

        self._pitchSpin = QtWidgets.QDoubleSpinBox(self)
        self._pitchSpin.setRange(0.01, 4.0)
        self._pitchSpin.setSingleStep(0.05)
        self._pitchSpin.setValue(float(filterData.get("pitch", 1.0)))
        form.addRow(ELOC("FILTER_PITCH"), self._pitchSpin)

        self._panSpin = QtWidgets.QDoubleSpinBox(self)
        self._panSpin.setRange(-1.0, 1.0)
        self._panSpin.setSingleStep(0.1)
        self._panSpin.setValue(float(filterData.get("pan", 0.0)))
        form.addRow(ELOC("FILTER_PAN"), self._panSpin)

        self._volumeSpin = QtWidgets.QDoubleSpinBox(self)
        self._volumeSpin.setRange(0.0, 100.0)
        self._volumeSpin.setSingleStep(1.0)
        self._volumeSpin.setValue(float(filterData.get("volume", 100.0)))
        form.addRow(ELOC("FILTER_VOLUME"), self._volumeSpin)

        if filterType == "bgm":
            loopPoint = filterData.get("loopPoint", {})
            self._loopStartSpin = QtWidgets.QDoubleSpinBox(self)
            self._loopStartSpin.setRange(0.0, 999999.0)
            self._loopStartSpin.setSingleStep(0.1)
            self._loopStartSpin.setValue(float(loopPoint.get("start", 0.0)))
            self._loopEndSpin = QtWidgets.QDoubleSpinBox(self)
            self._loopEndSpin.setRange(0.0, 999999.0)
            self._loopEndSpin.setSingleStep(0.1)
            self._loopEndSpin.setValue(float(loopPoint.get("end", 0.0)))
            lpLayout = QtWidgets.QHBoxLayout()
            lpLayout.setContentsMargins(0, 0, 0, 0)
            lpLayout.setSpacing(6)
            lpLayout.addWidget(self._loopStartSpin)
            lpLayout.addWidget(self._loopEndSpin)
            form.addRow(ELOC("FILTER_LOOP_POINT"), lpLayout)

        btns = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel, self)
        ok_btn = btns.button(QtWidgets.QDialogButtonBox.Ok)
        cancel_btn = btns.button(QtWidgets.QDialogButtonBox.Cancel)
        if ok_btn:
            ok_btn.setText(ELOC("CONFIRM"))
        if cancel_btn:
            cancel_btn.setText(ELOC("CANCEL"))
        btns.accepted.connect(self.accept)
        btns.rejected.connect(self.reject)
        form.addRow(btns)

    def getResult(self) -> Dict[str, Any]:
        result: Dict[str, Any] = {}
        offset = self._offsetSpin.value()
        if offset > 0.0:
            result["offset"] = offset
        pitch = self._pitchSpin.value()
        if pitch != 1.0:
            result["pitch"] = pitch
        pan = self._panSpin.value()
        if pan != 0.0:
            result["pan"] = pan
        volume = self._volumeSpin.value()
        if volume != 100.0:
            result["volume"] = volume
        if self._filterType == "bgm":
            start = self._loopStartSpin.value()
            end = self._loopEndSpin.value()
            if start > 0.0 or end > 0.0:
                result["loopPoint"] = {"start": start, "end": end}
        return result


def editFilterData(
    parent: QtWidgets.QWidget,
    filterData: Dict[str, Any],
    filterType: str,
    title: Optional[str] = None,
) -> Optional[Dict[str, Any]]:
    dlg = FilterEditDialog(parent, filterData, filterType, title)
    if dlg.exec_() != QtWidgets.QDialog.Accepted:
        return None
    return dlg.getResult()
