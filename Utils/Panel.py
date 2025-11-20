# -*- encoding: utf-8 -*-

from PyQt5 import QtCore, QtGui, QtWidgets


def clearPanel(panel: QtWidgets.QWidget, color: QtGui.QColor = QtGui.QColor.fromRgb(0, 0, 0)) -> None:
    pal = panel.palette()
    pal.setColor(QtGui.QPalette.Window, color)
    panel.setPalette(pal)
    panel.repaint()

def applyDisabledOpacity(widget: QtWidgets.QWidget, opacity: float = 0.6) -> None:
    if widget.isEnabled():
        widget.setGraphicsEffect(None)
    else:
        eff = QtWidgets.QGraphicsOpacityEffect(widget)
        eff.setOpacity(max(0.1, min(1.0, float(opacity))))
        widget.setGraphicsEffect(eff)
