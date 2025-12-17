# -*- encoding: utf-8 -*-

from typing import Optional
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import Locale, File
import EditorStatus


class StartWindow(QtWidgets.QWidget):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self._initUi()

    def _initUi(self) -> None:
        self.setWindowFlags(QtCore.Qt.FramelessWindowHint | QtCore.Qt.Window)
        self.setFixedSize(480, 320)
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(24, 24, 24, 24)
        layout.setSpacing(16)

        title = QtWidgets.QLabel(EditorStatus.APP_NAME, self)
        f = QtGui.QFont()
        f.setBold(True)
        f.setPointSize(28)
        title.setFont(f)
        title.setAlignment(QtCore.Qt.AlignHCenter | QtCore.Qt.AlignTop)

        self._btnNew = QtWidgets.QPushButton(Locale.getContent("NEW_PROJECT"), self)
        self._btnOpen = QtWidgets.QPushButton(Locale.getContent("OPEN_PROJECT"), self)
        self._btnNew.setObjectName("startNew")
        self._btnOpen.setObjectName("startOpen")
        self._btnNew.setCursor(QtCore.Qt.PointingHandCursor)
        self._btnOpen.setCursor(QtCore.Qt.PointingHandCursor)
        self._btnNew.setFixedHeight(48)
        self._btnOpen.setFixedHeight(48)
        self._btnNew.clicked.connect(self._onNewProject)
        self._btnOpen.clicked.connect(self._onOpenProject)

        btn_style = (
            "QPushButton { background-color: #1f1f1f; color: white; border: 1px solid #3a3a3a; border-radius: 6px; padding: 10px; font-size: 18px; }"
            + "\nQPushButton:hover { background-color: rgba(255,255,255,150); color: #111; border: 2px solid rgba(255,255,255,220); }"
            + "\nQPushButton:pressed { background-color: rgba(255,255,255,200); color: #111; border: 2px solid rgba(255,255,255,240); padding-top: 12px; padding-bottom: 8px; }"
        )
        self._btnNew.setStyleSheet(btn_style)
        self._btnOpen.setStyleSheet(btn_style)

        self.setStyleSheet("background-color: #121212; color: white;")

        layout.addWidget(title)
        layout.addStretch(1)
        layout.addWidget(self._btnNew)
        layout.addWidget(self._btnOpen)
        layout.addStretch(1)

    def _onNewProject(self) -> None:
        File.NewProject(self)

    def _onOpenProject(self) -> None:
        File.OpenProject(self)
