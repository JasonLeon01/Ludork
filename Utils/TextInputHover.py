# -*- encoding: utf-8 -*-

from __future__ import annotations

from PyQt5 import QtCore, QtGui, QtWidgets

_TEXT_INPUT_TYPES = (
    QtWidgets.QLineEdit,
    QtWidgets.QPlainTextEdit,
    QtWidgets.QTextEdit,
)


def _cursorIndexAt(widget: QtWidgets.QWidget, pos: QtCore.QPoint) -> int:
    if isinstance(widget, QtWidgets.QLineEdit):
        return int(widget.cursorPositionAt(pos))
    if isinstance(widget, (QtWidgets.QPlainTextEdit, QtWidgets.QTextEdit)):
        return int(widget.cursorForPosition(pos).position())
    return -1


def _widgetText(widget: QtWidgets.QWidget) -> str:
    if isinstance(widget, QtWidgets.QLineEdit):
        return widget.text()
    if isinstance(widget, (QtWidgets.QPlainTextEdit, QtWidgets.QTextEdit)):
        return widget.toPlainText()
    return ""


def _mergeTooltips(base: str, extra: str) -> str:
    baseText = str(base or "").strip()
    extraText = str(extra or "").strip()
    if baseText and extraText:
        return f"{baseText}\n\n{extraText}"
    return extraText or baseText


class _TextInputHoverFilter(QtCore.QObject):
    def eventFilter(self, receiver: QtCore.QObject, event: QtCore.QEvent) -> bool:
        if event.type() != QtCore.QEvent.ToolTip:
            return super().eventFilter(receiver, event)
        widget = receiver if isinstance(receiver, _TEXT_INPUT_TYPES) else None
        if widget is None or not widget.isVisible():
            return super().eventFilter(receiver, event)
        if not isinstance(event, QtGui.QHelpEvent):
            return super().eventFilter(receiver, event)
        from . import PluginSystem

        text = _widgetText(widget)
        cursorIndex = _cursorIndexAt(widget, event.pos())
        hoverTip = PluginSystem.ResolveTextInputHoverTooltip(widget, text, cursorIndex)
        if not hoverTip:
            return super().eventFilter(receiver, event)
        fullTip = _mergeTooltips(widget.toolTip(), hoverTip)
        if fullTip:
            QtWidgets.QToolTip.showText(event.globalPos(), fullTip, widget)
            return True
        return super().eventFilter(receiver, event)


_filterInstalled = False


def InstallApplicationFilter() -> None:
    global _filterInstalled
    if _filterInstalled:
        return
    app = QtWidgets.QApplication.instance()
    if app is None:
        return
    app.installEventFilter(_TextInputHoverFilter(app))
    _filterInstalled = True
