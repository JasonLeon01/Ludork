# -*- encoding: utf-8 -*-

from collections.abc import Callable, Sequence
from typing import Any, Optional

from PyQt5 import QtCore, QtWidgets

from .FormDialog import FormDialog, OpenFormDialog


def _buildSingleRowField(
    message: str,
    initial_text: str,
    input_mode: Optional[str],
    min_value: Optional[float],
    max_value: Optional[float],
    combo_items: Optional[Sequence[str]],
    combo_current_index: int,
) -> dict[str, Any]:
    field_type = input_mode or "text"
    field: dict[str, Any] = {"name": "text", "label": message, "focus": True}
    if field_type == "combo":
        options = list(combo_items or ())
        safe_index = max(0, min(combo_current_index, len(options) - 1)) if options else 0
        field["type"] = "combo"
        field["options"] = options
        field["currentIndex"] = safe_index
        if options:
            field["initialValue"] = options[safe_index]
        return field
    if field_type == "int":
        field["type"] = "int"
        field["initialValue"] = initial_text
        field["minValue"] = 0 if min_value is None else min_value
        field["maxValue"] = 999999 if max_value is None else max_value
        return field
    if field_type in ("float", "number"):
        field["type"] = "float"
        field["initialValue"] = initial_text
        field["minValue"] = 0 if min_value is None else min_value
        field["maxValue"] = 999999 if max_value is None else max_value
        return field
    field["type"] = "text"
    field["initialValue"] = initial_text
    return field


SingleRowDialog = FormDialog


def OpenSingleRowDialog(
    parent: QtWidgets.QWidget,
    title: str,
    message: str,
    initial_text: str = "",
    *,
    onAccepted: Optional[Callable[[str], None]] = None,
    onRejected: Optional[Callable[[], None]] = None,
    input_mode: Optional[str] = None,
    min_value: Optional[float] = None,
    max_value: Optional[float] = None,
    combo_items: Optional[Sequence[str]] = None,
    combo_current_index: int = 0,
) -> FormDialog:
    is_combo = input_mode == "combo"
    size = QtCore.QSize(400, 150 if is_combo else 130)
    minimum_size = QtCore.QSize(320, 130 if is_combo else 0)
    field = _buildSingleRowField(
        message,
        initial_text,
        input_mode,
        min_value,
        max_value,
        combo_items,
        combo_current_index,
    )

    def handleResult(result: dict[str, Any]) -> None:
        if onAccepted is not None:
            onAccepted(str(result.get("text", "")))

    return OpenFormDialog(
        parent,
        title,
        [field],
        size=size,
        minimumSize=minimum_size,
        onAccepted=handleResult,
        onRejected=onRejected,
    )


def OpenItemSelectorDialog(
    parent: QtWidgets.QWidget,
    title: str,
    message: str,
    items: Sequence[str],
    currentIndex: int = 0,
    *,
    onAccepted: Optional[Callable[[str], None]] = None,
    onRejected: Optional[Callable[[], None]] = None,
) -> FormDialog:
    return OpenSingleRowDialog(
        parent,
        title,
        message,
        onAccepted=onAccepted,
        onRejected=onRejected,
        input_mode="combo",
        combo_items=items,
        combo_current_index=currentIndex,
    )
