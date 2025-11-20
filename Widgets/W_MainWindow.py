# -*- encoding: utf-8 -*-

import os
import sys
import subprocess
import configparser
from typing import Optional
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import Locale


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

        self.topBar = QtWidgets.QWidget()
        self.topBar.setMinimumHeight(64)
        topLayout = QtWidgets.QHBoxLayout(self.topBar)
        topLayout.setContentsMargins(0, 0, 0, 0)
        topLayout.addStretch(1)
        self.startButton = QtWidgets.QPushButton(Locale.getContent("TestGame"))
        self.startButton.setMinimumHeight(64)
        topLayout.addWidget(self.startButton)
        topLayout.addStretch(1)

        self.panel = QtWidgets.QWidget()
        if os.environ.get("SCREEN_LOW_RES"):
            if os.environ["SCREEN_LOW_RES"] == "1":
                self.panel.setFixedSize(640, 480)
            else:
                self.panel.setFixedSize(960, 720)
        else:
            self.panel.setFixedSize(1280, 960)
        self.panel.setObjectName("GamePanel")
        self.panel.setAttribute(QtCore.Qt.WA_NativeWindow, True)
        self.panel.setAutoFillBackground(True)
        pal = self.panel.palette()
        pal.setColor(QtGui.QPalette.Window, QtGui.QColor.fromRgb(0, 0, 0))
        self.panel.setPalette(pal)
        self._panelHandle = int(self.panel.winId())

        self.startButton.clicked.connect(self.startGame)

        layout = QtWidgets.QVBoxLayout(central)
        layout.setContentsMargins(8, 0, 8, 8)
        layout.setSpacing(0)
        layout.addWidget(self.topBar, 0, alignment=QtCore.Qt.AlignTop)
        layout.addWidget(self.panel, 0, alignment=QtCore.Qt.AlignHCenter)
        layout.addStretch(1)

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        cfg = configparser.ConfigParser()
        cfg_path = os.path.join(os.getcwd(), "Ludork.ini")
        assert os.path.exists(cfg_path)
        cfg.read(cfg_path)
        assert "Ludork" in cfg
        s = self.size()
        cfg["Ludork"]["Width"] = str(s.width())
        cfg["Ludork"]["Height"] = str(s.height())
        with open(cfg_path, "w") as f:
            cfg.write(f)

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
        self._engineProc = subprocess.Popen(
            [sys.executable, script_path, str(self._panelHandle)], cwd=self._projPath, shell=False
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
