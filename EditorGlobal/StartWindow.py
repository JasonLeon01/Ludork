# -*- encoding: utf-8 -*-

import os
from typing import Optional
from PyQt5 import QtCore, QtGui, QtQuickWidgets, QtWidgets
from Utils import File
from . import EditorStatus


class _StartWindowBridge(QtCore.QObject):
    newProjectRequested = QtCore.pyqtSignal()
    openProjectRequested = QtCore.pyqtSignal()
    projectDropped = QtCore.pyqtSignal(str)

    @QtCore.pyqtSlot()
    def requestNewProject(self) -> None:
        QtCore.QTimer.singleShot(0, self.newProjectRequested.emit)

    @QtCore.pyqtSlot()
    def requestOpenProject(self) -> None:
        QtCore.QTimer.singleShot(0, self.openProjectRequested.emit)

    @QtCore.pyqtSlot(str)
    def openDroppedProject(self, url: str) -> None:
        QtCore.QTimer.singleShot(0, lambda: self.projectDropped.emit(url))


class StartWindow(QtWidgets.QWidget):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self._bridge = _StartWindowBridge(self)
        self._initUi()
        self._connectBridge()

    def _initUi(self) -> None:
        self.setWindowFlags(QtCore.Qt.FramelessWindowHint | QtCore.Qt.Window)
        self.setFixedSize(480, 320)

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        self._quickWidget = QtQuickWidgets.QQuickWidget(self)
        self._quickWidget.setResizeMode(QtQuickWidgets.QQuickWidget.SizeRootObjectToView)
        self._quickWidget.setClearColor(QtGui.QColor("#121212"))
        self._quickWidget.setFocusPolicy(QtCore.Qt.StrongFocus)
        self._quickWidget.setAcceptDrops(True)

        context: QtCore.QQmlContext = self._quickWidget.rootContext()
        context.setContextProperty("startBridge", self._bridge)
        context.setContextProperty("startAppName", EditorStatus.APP_NAME)
        context.setContextProperty("startNewProjectText", ELOC("NEW_PROJECT"))
        context.setContextProperty("startOpenProjectText", ELOC("OPEN_PROJECT"))
        app = QtWidgets.QApplication.instance()
        context.setContextProperty("startFontFamily", app.font().family() if app else "")

        self._quickWidget.setSource(QtCore.QUrl.fromLocalFile(self._qmlPath()))
        if self._quickWidget.status() == QtQuickWidgets.QQuickWidget.Error:
            errors = "\n".join(error.toString() for error in self._quickWidget.errors())
            raise RuntimeError(f"Failed to load StartWindow QML:\n{errors}")
        layout.addWidget(self._quickWidget)

    def _connectBridge(self) -> None:
        self._bridge.newProjectRequested.connect(self._onNewProject)
        self._bridge.openProjectRequested.connect(self._onOpenProject)
        self._bridge.projectDropped.connect(self._onProjectDropped)

    def _qmlPath(self) -> str:
        paths = (
            os.path.join(File.GetRootPath(), "EditorGlobal", "Qml", "StartWindow.qml"),
            os.path.join(os.path.dirname(__file__), "Qml", "StartWindow.qml"),
        )
        for path in paths:
            if os.path.isfile(path):
                return os.path.abspath(path)
        return os.path.abspath(paths[0])

    def _onProjectDropped(self, urlText: str) -> None:
        url = QtCore.QUrl(urlText)
        path = url.toLocalFile() if url.isLocalFile() else urlText
        if path.lower().endswith(".proj") and os.path.isfile(path):
            File.OpenProjectFile(path, self)

    def _onNewProject(self) -> None:
        File.NewProject(self)

    def _onOpenProject(self) -> None:
        File.OpenProject(self)

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        self._quickWidget.hide()
        self._quickWidget.setParent(None)
        self._quickWidget.deleteLater()
        super().closeEvent(event)
