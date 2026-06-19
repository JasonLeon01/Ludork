# -*- encoding: utf-8 -*-

import os
from typing import Optional, Union, cast
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import File, System
from . import EditorStatus


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

        self._btnNew = QtWidgets.QPushButton(ELOC("NEW_PROJECT"), self)
        self._btnOpen = QtWidgets.QPushButton(ELOC("OPEN_PROJECT"), self)
        self._btnNew.setObjectName("startNew")
        self._btnOpen.setObjectName("startOpen")
        self._btnNew.setCursor(QtCore.Qt.PointingHandCursor)
        self._btnOpen.setCursor(QtCore.Qt.PointingHandCursor)
        self._btnNew.setFixedHeight(48)
        self._btnOpen.setFixedHeight(48)
        self._btnNew.clicked.connect(self._onNewProject)
        self._btnOpen.clicked.connect(self._onOpenProject)

        System.SetStyle(self._btnNew, "starterBtn.qss")
        System.SetStyle(self._btnOpen, "starterBtn.qss")

        self.setStyleSheet("background-color: #121212;")

        layout.addWidget(title)
        layout.addStretch(1)
        layout.addWidget(self._btnNew)
        layout.addWidget(self._btnOpen)
        layout.addStretch(1)

        self.setAcceptDrops(True)
        for child in self.findChildren(QtWidgets.QWidget):
            child.setAcceptDrops(True)
            child.installEventFilter(self)

    def eventFilter(self, watched: QtCore.QObject, event: QtCore.QEvent) -> bool:
        eventType = event.type()
        if eventType == QtCore.QEvent.DragEnter:
            dragEvent = cast(QtGui.QDragEnterEvent, event)
            if self._acceptProjDrag(dragEvent):
                dragEvent.acceptProposedAction()
                return True
            return False
        if eventType == QtCore.QEvent.DragMove:
            dragEvent = cast(QtGui.QDragMoveEvent, event)
            if self._acceptProjDrag(dragEvent):
                dragEvent.acceptProposedAction()
                return True
            return False
        if eventType == QtCore.QEvent.Drop:
            dropEvent = cast(QtGui.QDropEvent, event)
            projFile = self._projFileFromDrop(dropEvent)
            if projFile:
                dropEvent.acceptProposedAction()
                File.OpenProjectFile(projFile, self)
                return True
            return False
        return super().eventFilter(watched, event)

    def dragEnterEvent(self, event: QtGui.QDragEnterEvent) -> None:
        if self._acceptProjDrag(event):
            event.acceptProposedAction()

    def dragMoveEvent(self, event: QtGui.QDragMoveEvent) -> None:
        if self._acceptProjDrag(event):
            event.acceptProposedAction()

    def dropEvent(self, event: QtGui.QDropEvent) -> None:
        projFile = self._projFileFromDrop(event)
        if projFile:
            event.acceptProposedAction()
            File.OpenProjectFile(projFile, self)

    def _acceptProjDrag(self, event: Union[QtGui.QDragEnterEvent, QtGui.QDragMoveEvent]) -> bool:
        return self._projFileFromDrop(event) is not None

    def _projFileFromDrop(self, event: QtGui.QDropEvent) -> Optional[str]:
        mimeData = event.mimeData()
        if not mimeData or not mimeData.hasUrls():
            return None
        for url in mimeData.urls():
            if not url.isLocalFile():
                continue
            path = url.toLocalFile()
            if path.lower().endswith(".proj") and os.path.isfile(path):
                return path
        return None

    def _onNewProject(self) -> None:
        File.NewProject(self)

    def _onOpenProject(self) -> None:
        File.OpenProject(self)
