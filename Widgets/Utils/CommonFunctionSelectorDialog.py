# -*- encoding: utf-8 -*-

from collections.abc import Callable
from typing import Optional

from PyQt5 import QtWidgets

from EditorGlobal import GameData
from .SearchSelectorDialog import OpenSearchSelectorDialog, SearchSelectorDialog


def OpenCommonFunctionSelector(
    parent: Optional[QtWidgets.QWidget],
    currentValue: str,
    onSelected: Callable[[str], None],
    onCancelled: Optional[Callable[[], None]] = None,
) -> SearchSelectorDialog:
    items = sorted(str(key) for key in GameData.commonFunctionsData.keys())
    return OpenSearchSelectorDialog(
        parent,
        ELOC("COMMON_FUNCTIONS"),
        items,
        currentValue,
        onSelected=onSelected,
        onCancelled=onCancelled,
    )
