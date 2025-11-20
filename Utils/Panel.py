# -*- encoding: utf-8 -*-

from PyQt5 import QtCore, QtGui, QtWidgets


def clearPanel(panel: QtWidgets.QWidget, color: QtGui.QColor = QtGui.QColor.fromRgb(0, 0, 0)) -> None:
    pal = panel.palette()
    pal.setColor(QtGui.QPalette.Window, color)
    panel.setPalette(pal)
    panel.repaint()
