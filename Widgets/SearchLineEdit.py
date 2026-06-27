# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
from typing import Dict
from PyQt5 import QtGui, QtWidgets
from Utils import File


_ICON_CACHE: Dict[str, QtGui.QIcon] = {}


def AddSearchIcon(lineEdit: QtWidgets.QLineEdit) -> None:
    lineEdit.addAction(_searchIcon(), QtWidgets.QLineEdit.LeadingPosition)


def _searchIcon() -> QtGui.QIcon:
    icon = _ICON_CACHE.get("search")
    if icon is None:
        path = os.path.join(File.GetRootPath(), "Resource", "icons", "search.svg")
        icon = QtGui.QIcon(path)
        _ICON_CACHE["search"] = icon
    return icon
