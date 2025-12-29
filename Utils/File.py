# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
import sys
import json
import pickle
import shutil
from pathlib import Path
from typing import Dict, Any, TYPE_CHECKING
from PyQt5 import QtCore, QtGui, QtWidgets
from . import System

if TYPE_CHECKING:
    from W_MainWindow import MainWindow

mainWindow: MainWindow = None


def getRootPath() -> str:
    if System.already_packed():
        return os.path.dirname(sys.executable)
    return os.getcwd()


def getIniPath() -> str:
    import EditorStatus

    if System.already_packed() and sys.platform == "darwin":
        path = Path.home() / "Library" / "Application Support" / EditorStatus.APP_NAME
        path.mkdir(parents=True, exist_ok=True)
        return str(path)
    return os.getcwd()


def getUserPath() -> str:
    import EditorStatus

    if sys.platform == "win32":
        return os.path.join(os.getenv("APPDATA"), EditorStatus.APP_NAME)
    elif sys.platform == "darwin":
        path = Path.home() / "Library" / "Application Support" / EditorStatus.APP_NAME
    else:
        print("Unsupported platform")
        sys.exit(1)
    path.mkdir(parents=True, exist_ok=True)
    return str(path)


def getJSONData(filePath: str) -> Dict[str, Any]:
    with open(filePath, "r", encoding="utf-8") as file:
        jsonData = file.read()
    return json.loads(jsonData)


def saveJsonData(filePath: str, data: Dict[str, Any]) -> None:
    with open(filePath, "w", encoding="utf-8") as file:
        json.dump(data, file, ensure_ascii=False)


def loadData(filePath: str) -> Any:
    with open(filePath, "rb") as file:
        return pickle.load(file)


def saveData(filePath, data: Dict[str, Any]) -> None:
    with open(filePath, "wb") as file:
        pickle.dump(data, file)


def _homeDir() -> str:
    try:
        return QtCore.QStandardPaths.writableLocation(QtCore.QStandardPaths.HomeLocation)
    except Exception:
        return os.path.expanduser("~")


def _getLastPathOrHome() -> str:
    import EditorStatus

    sec = (
        EditorStatus.editorConfig[EditorStatus.APP_NAME]
        if EditorStatus.editorConfig and EditorStatus.APP_NAME in EditorStatus.editorConfig
        else None
    )
    p = sec.get("LastOpenPath") if sec else None
    if isinstance(p, str) and p.strip() and os.path.exists(p):
        return p
    return _homeDir()


def _configPath() -> str:
    import EditorStatus

    return os.path.join(getIniPath(), f"{EditorStatus.APP_NAME}.ini")


def _setLastOpenPath(path: str) -> None:
    import EditorStatus

    if not EditorStatus.editorConfig:
        return
    if EditorStatus.APP_NAME not in EditorStatus.editorConfig:
        EditorStatus.editorConfig[EditorStatus.APP_NAME] = {}
    EditorStatus.editorConfig[EditorStatus.APP_NAME]["LastOpenPath"] = os.path.abspath(path)
    with open(_configPath(), "w", encoding="utf-8") as f:
        EditorStatus.editorConfig.write(f)


def _openProjectPath(path: str, widget: QtWidgets.QWidget) -> None:
    global mainWindow

    import EditorStatus
    import Data
    from Utils import Locale, System

    EditorStatus.PROJ_PATH = os.path.abspath(path)
    if EditorStatus.PROJ_PATH not in sys.path:
        sys.path.append(EditorStatus.PROJ_PATH)
    # try:
    Data.GameData.init()
    # except Exception as e:
    #     QtWidgets.QMessageBox.critical(None, "Error", Locale.getContent("OPEN_FAILED") + "\n" + str(e))
    #     return
    from W_MainWindow import MainWindow

    mainWindow = MainWindow(System.getTitle())
    try:
        cfg_w = int(EditorStatus.editorConfig[EditorStatus.APP_NAME].get("Width", mainWindow.width()))
        cfg_h = int(EditorStatus.editorConfig[EditorStatus.APP_NAME].get("Height", mainWindow.height()))
    except Exception:
        cfg_w, cfg_h = mainWindow.width(), mainWindow.height()
    min_size = mainWindow.minimumSize()
    mainWindow.resize(max(cfg_w, min_size.width()), max(cfg_h, min_size.height()))
    icon_path = os.path.join(os.getcwd(), "Resource", "icon.ico")
    app = QtWidgets.QApplication.instance()
    if app:
        app.setWindowIcon(QtGui.QIcon(icon_path))
    mainWindow.setWindowIcon(QtGui.QIcon(icon_path))
    screen = app.primaryScreen() if app else None
    fg = mainWindow.frameGeometry()
    cp = screen.availableGeometry().center() if screen else mainWindow.geometry().center()
    fg.moveCenter(cp)
    mainWindow.move(fg.topLeft())
    if app:
        app.aboutToQuit.connect(mainWindow.endGame)
    mainWindow.show()
    widget.close()


def NewProject(parent: QtWidgets.QWidget) -> None:
    from Utils import Locale

    root = _getLastPathOrHome()
    dirPath = QtWidgets.QFileDialog.getExistingDirectory(parent, Locale.getContent("SELECT_PROJECT_DIR"), root)
    if not dirPath:
        return
    text, ok = QtWidgets.QInputDialog.getText(
        parent,
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
        QtWidgets.QMessageBox.warning(parent, "Hint", Locale.getContent("PROJECT_EXISTS"))
        return
    try:
        src = os.path.abspath(os.path.join(os.getcwd(), "Sample"))
        shutil.copytree(src, target)
        projFile = os.path.join(target, "Main.proj")
        with open(projFile, "w", encoding="utf-8") as f:
            f.write("{}")
    except Exception as e:
        QtWidgets.QMessageBox.critical(None, "Error", Locale.getContent("COPY_FAILED") + "\n" + str(e))
        return
    _setLastOpenPath(target)
    _openProjectPath(target, parent)


def OpenProject(parent: QtWidgets.QWidget) -> None:
    from Utils import Locale

    root = _getLastPathOrHome()
    fp, _ = QtWidgets.QFileDialog.getOpenFileName(
        parent,
        Locale.getContent("SELECT_PROJ_FILE"),
        root,
        "Project Files (*.proj)",
    )
    if not fp:
        return
    if not fp.lower().endswith(".proj"):
        QtWidgets.QMessageBox.warning(parent, "Hint", Locale.getContent("INVALID_PROJ_FILE"))
        return
    proj_dir = os.path.dirname(fp)
    _setLastOpenPath(proj_dir)
    _openProjectPath(proj_dir, parent)
