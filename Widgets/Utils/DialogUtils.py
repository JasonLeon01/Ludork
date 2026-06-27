# -*- encoding: utf-8 -*-

from __future__ import annotations

import sys
from typing import Optional

from PyQt5 import QtCore, QtWidgets

try:
    import PyQt5.sip as sip
except ImportError:
    sip = None


def IsWidgetValid(widget: Optional[QtWidgets.QWidget]) -> bool:
    if widget is None:
        return False
    if sip is None:
        return True
    try:
        return not sip.isdeleted(widget)
    except RuntimeError:
        return False


def GetIndependentDialogParent(source: Optional[QtWidgets.QWidget] = None) -> Optional[QtWidgets.QWidget]:
    fileModule = sys.modules.get("Utils.File")
    mainWindow = getattr(fileModule, "mainWindow", None)

    activeWindow = QtWidgets.QApplication.activeWindow()
    if (
        isinstance(activeWindow, QtWidgets.QWidget)
        and IsWidgetValid(activeWindow)
        and isinstance(mainWindow, QtWidgets.QWidget)
        and activeWindow is not mainWindow
        and activeWindow.windowModality() == QtCore.Qt.ApplicationModal
    ):
        return activeWindow

    if isinstance(mainWindow, QtWidgets.QWidget) and IsWidgetValid(mainWindow):
        return mainWindow

    if isinstance(activeWindow, QtWidgets.QWidget) and IsWidgetValid(activeWindow):
        if source is None or not _isAncestorOf(activeWindow, source):
            return activeWindow
    return None


def _isAncestorOf(candidate: QtWidgets.QWidget, widget: QtWidgets.QWidget) -> bool:
    current: Optional[QtWidgets.QWidget] = widget
    while current is not None:
        if current is candidate:
            return True
        current = current.parentWidget()
    return False
