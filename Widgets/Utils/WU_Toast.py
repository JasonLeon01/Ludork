# -*- encoding: utf-8 -*-

from PyQt5 import QtCore, QtGui, QtWidgets


class Toast(QtWidgets.QWidget):
    def __init__(self, parent):
        super().__init__(parent)
        self.setWindowFlags(QtCore.Qt.FramelessWindowHint | QtCore.Qt.SubWindow)
        self.setAttribute(QtCore.Qt.WA_TranslucentBackground)
        self.setAttribute(QtCore.Qt.WA_ShowWithoutActivating)

        self.layout = QtWidgets.QVBoxLayout(self)
        self.layout.setContentsMargins(0, 0, 0, 0)
        self.label = QtWidgets.QLabel()
        self.label.setStyleSheet(
            "background-color: rgba(50, 50, 50, 200); border-radius: 5px; padding: 10px; font-weight: bold;"
        )
        self.layout.addWidget(self.label)

        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self.hide)
        self.hide()

    def showMessage(self, message, duration=2000):
        if not message:
            return
        self.label.setText(message)
        self.label.adjustSize()
        self.adjustSize()
        self._updatePosition()
        self.show()
        self.raise_()
        self.timer.start(duration)

    def _updatePosition(self):
        if self.parent():
            p_rect = self.parent().rect()
            self.move(p_rect.width() - self.width() - 20, p_rect.height() - self.height() - 40)
