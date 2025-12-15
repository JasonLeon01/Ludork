# -*- encoding: utf-8 -*-

import os
import sys
import shutil
from typing import Optional
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import Locale, System
import EditorStatus
import Data


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

        title = QtWidgets.QLabel("Ludork", self)
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

    def _homeDir(self) -> str:
        try:
            return QtCore.QStandardPaths.writableLocation(QtCore.QStandardPaths.HomeLocation)
        except Exception:
            return os.path.expanduser("~")

    def _configPath(self) -> str:
        return os.path.join(os.getcwd(), "Ludork.ini")

    def _getLastPathOrHome(self) -> str:
        sec = (
            EditorStatus.editorConfig["Ludork"]
            if EditorStatus.editorConfig and "Ludork" in EditorStatus.editorConfig
            else None
        )
        p = sec.get("LastOpenPath") if sec else None
        if isinstance(p, str) and p.strip() and os.path.exists(p):
            return p
        return self._homeDir()

    def _setLastOpenPath(self, path: str) -> None:
        if not EditorStatus.editorConfig:
            return
        if "Ludork" not in EditorStatus.editorConfig:
            EditorStatus.editorConfig["Ludork"] = {}
        EditorStatus.editorConfig["Ludork"]["LastOpenPath"] = os.path.abspath(path)
        with open(self._configPath(), "w", encoding="utf-8") as f:
            EditorStatus.editorConfig.write(f)

    def _onNewProject(self) -> None:
        root = self._getLastPathOrHome()
        dirPath = QtWidgets.QFileDialog.getExistingDirectory(self, Locale.getContent("SELECT_PROJECT_DIR"), root)
        if not dirPath:
            return
        text, ok = QtWidgets.QInputDialog.getText(
            self,
            Locale.getContent("ENTER_PROJECT_NAME"),
            Locale.getContent("ENTER_PROJECT_NAME"),
        )
        if not ok:
            return
        name = str(text).strip()
        if not name:
            return
        target = os.path.abspath(os.path.join(dirPath, name))
        if os.path.exists(target):
            QtWidgets.QMessageBox.warning(self, "Hint", Locale.getContent("PROJECT_EXISTS"))
            return
        try:
            src = os.path.abspath(os.path.join(os.getcwd(), "Sample"))
            shutil.copytree(src, target)
            projFile = os.path.join(target, "Main.proj")
            with open(projFile, "w", encoding="utf-8") as f:
                f.write("{}")
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", Locale.getContent("COPY_FAILED") + "\n" + str(e))
            return
        self._setLastOpenPath(target)
        self._openProjectPath(target)

    def _onOpenProject(self) -> None:
        root = self._getLastPathOrHome()
        fp, _ = QtWidgets.QFileDialog.getOpenFileName(
            self,
            Locale.getContent("SELECT_PROJ_FILE"),
            root,
            "Project Files (*.proj)",
        )
        if not fp:
            return
        if not fp.lower().endswith(".proj"):
            QtWidgets.QMessageBox.warning(self, "Hint", Locale.getContent("INVALID_PROJ_FILE"))
            return
        proj_dir = os.path.dirname(fp)
        self._setLastOpenPath(proj_dir)
        self._openProjectPath(proj_dir)

    def _openProjectPath(self, path: str) -> None:
        EditorStatus.PROJ_PATH = os.path.abspath(path)
        if EditorStatus.PROJ_PATH not in sys.path:
            sys.path.append(EditorStatus.PROJ_PATH)
        try:
            Data.GameData.init()
        except Exception as e:
            QtWidgets.QMessageBox.critical(self, "Error", Locale.getContent("OPEN_FAILED") + "\n" + str(e))
            return
        from .W_MainWindow import MainWindow

        self._mainWindow = MainWindow(System.get_title())
        try:
            cfg_w = int(EditorStatus.editorConfig["Ludork"].get("Width", self._mainWindow.width()))
            cfg_h = int(EditorStatus.editorConfig["Ludork"].get("Height", self._mainWindow.height()))
        except Exception:
            cfg_w, cfg_h = self._mainWindow.width(), self._mainWindow.height()
        min_size = self._mainWindow.minimumSize()
        self._mainWindow.resize(max(cfg_w, min_size.width()), max(cfg_h, min_size.height()))
        icon_path = os.path.join(os.getcwd(), "Resource", "icon.ico")
        app = QtWidgets.QApplication.instance()
        if app:
            app.setWindowIcon(QtGui.QIcon(icon_path))
        self._mainWindow.setWindowIcon(QtGui.QIcon(icon_path))
        screen = app.primaryScreen() if app else None
        fg = self._mainWindow.frameGeometry()
        cp = screen.availableGeometry().center() if screen else self._mainWindow.geometry().center()
        fg.moveCenter(cp)
        self._mainWindow.move(fg.topLeft())
        if app:
            app.aboutToQuit.connect(self._mainWindow.endGame)
        self._mainWindow.show()
        self.close()
