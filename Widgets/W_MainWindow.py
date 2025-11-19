# -*- encoding: utf-8 -*-

import os
import subprocess
import configparser
from typing import Optional
from PyQt5 import QtCore, QtGui, QtWidgets


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self, title: str, projPath: str):
        super().__init__()
        self._projPath = projPath
        self._engineProc: Optional[subprocess.Popen] = None
        self.resize(1280, 960)
        self.setWindowTitle(title)

        QtWidgets.QApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling, True)
        QtWidgets.QApplication.setAttribute(QtCore.Qt.AA_UseHighDpiPixmaps, True)

        central = QtWidgets.QWidget(self)
        self.setCentralWidget(central)

        self.splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal, central)

        self.panel = QtWidgets.QWidget()
        self.panel.setFixedSize(640, 480)
        self.panel.setObjectName("GamePanel")
        self.panel.setAttribute(QtCore.Qt.WA_NativeWindow, True)
        self.panel.setAutoFillBackground(True)
        pal = self.panel.palette()
        pal.setColor(QtGui.QPalette.Window, QtGui.QColor.fromRgb(0, 0, 0))
        self.panel.setPalette(pal)
        self.splitter.addWidget(self.panel)
        self._panelHandle = int(self.panel.winId())

        self.startGame()

        self.splitter.setStretchFactor(0, 1)
        self.splitter.setStretchFactor(1, 3)

        layout = QtWidgets.QHBoxLayout(central)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.addWidget(self.splitter)

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        # self.endGame()

    def getPanelHandle(self) -> int:
        return int(self.panel.winId())

    def getProjPath(self) -> str:
        return self._projPath

    def setProjPath(self, projPath: str):
        self._projPath = projPath

    def startGame(self):
        self.endGame()
        iniPath = os.path.join(self._projPath, "Main.ini")
        iniFile = configparser.ConfigParser()
        iniFile.read(iniPath, encoding="utf-8")
        script_path = iniFile["Main"]["script"]
        try:
            self._engineProc = subprocess.Popen(
                ["py", "-3.10", script_path, str(self._panelHandle)], cwd=self._projPath, shell=False
            )
        except FileNotFoundError:
            local_py = os.path.join(os.environ.get("LocalAppData", ""), "Programs", "Python", "Python310", "python.exe")
            if not os.path.isfile(local_py):
                raise RuntimeError("Python 3.10 not found, please install or configure Python Launcher.")
            self._engineProc = subprocess.Popen(
                [local_py, script_path, str(self._panelHandle)], cwd=self._projPath, shell=False
            )

    def endGame(self):
        if self._engineProc:
            self._engineProc.terminate()
            self._engineProc.wait()
            self._engineProc = None
        self.clearPanel()

    def clearPanel(self, color: QtGui.QColor = QtGui.QColor.fromRgb(0, 0, 0)) -> None:
        pal = self.panel.palette()
        pal.setColor(QtGui.QPalette.Window, color)
        self.panel.setPalette(pal)
        self.panel.repaint()
