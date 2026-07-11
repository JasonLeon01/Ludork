# -*- encoding: utf-8 -*-

import os
import sys
import locale
import configparser
import subprocess
import traceback
import threading
from types import TracebackType

from PyQt5 import QtCore, QtWidgets
from PyQt5.QtWidgets import QApplication
from qt_material import apply_stylesheet
from PyQt5.QtGui import QIcon

from Utils import Locale, System, File

START_PROJ_FILE = None
_UNEXPECTED_EXCEPTION_HANDLED = False
_UNEXPECTED_EXCEPTION_LOCK = threading.Lock()


def InitConfig():
    from EditorGlobal import EditorStatus

    if not System.AlreadyPacked():
        subprocess.run(
            [sys.executable, "tools/localeTransfer.py", os.path.join(".", "Locale", "locale.json")], check=True
        )
    Locale.Init(os.path.join(File.GetRootPath(), "Locale"))
    EditorStatus.EDITOR_CONFIG = configparser.ConfigParser()
    if not os.path.exists(os.path.join(File.GetIniPath(), f"{EditorStatus.APP_NAME}.ini")):
        EditorStatus.EDITOR_CONFIG[EditorStatus.APP_NAME] = {}
        EditorStatus.EDITOR_CONFIG[EditorStatus.APP_NAME]["Width"] = "1280"
        EditorStatus.EDITOR_CONFIG[EditorStatus.APP_NAME]["Height"] = "720"
        EditorStatus.EDITOR_CONFIG[EditorStatus.APP_NAME]["UpperLeftWidth"] = "320"
        EditorStatus.EDITOR_CONFIG[EditorStatus.APP_NAME]["UpperRightWidth"] = "320"
        lang, _ = locale.getdefaultlocale()
        EditorStatus.EDITOR_CONFIG[EditorStatus.APP_NAME]["Language"] = (
            lang if lang in Locale.GetLocaleKeys() else "en_GB"
        )
        EditorStatus.EDITOR_CONFIG[EditorStatus.APP_NAME]["Theme"] = "dark_amber.xml"
        EditorStatus.EDITOR_CONFIG[EditorStatus.APP_NAME]["UIFont"] = "HarmonyOS_Sans_SC_Regular.ttf"
        EditorStatus.EDITOR_CONFIG[EditorStatus.APP_NAME]["UIFontSize"] = "12"
        EditorStatus.EDITOR_CONFIG[EditorStatus.APP_NAME]["Aiprovider"] = ""
        EditorStatus.EDITOR_CONFIG[EditorStatus.APP_NAME]["Aimodel"] = ""
        EditorStatus.EDITOR_CONFIG[EditorStatus.APP_NAME]["Apikey"] = ""
        with open(os.path.join(File.GetIniPath(), f"{EditorStatus.APP_NAME}.ini"), "w") as f:
            EditorStatus.EDITOR_CONFIG.write(f)
    else:
        EditorStatus.EDITOR_CONFIG.read(
            os.path.join(File.GetIniPath(), f"{EditorStatus.APP_NAME}.ini"), encoding="utf-8"
        )
    EditorStatus.LANGUAGE = EditorStatus.EDITOR_CONFIG[EditorStatus.APP_NAME]["Language"]


def _ShowUnexpectedException(excType: type[BaseException], excValue: BaseException, excTb: TracebackType | None) -> None:
    global _UNEXPECTED_EXCEPTION_HANDLED

    traceback.print_exception(excType, excValue, excTb, file=sys.stderr)
    try:
        sys.stderr.flush()
    except Exception:
        pass
    with _UNEXPECTED_EXCEPTION_LOCK:
        if _UNEXPECTED_EXCEPTION_HANDLED:
            return
        _UNEXPECTED_EXCEPTION_HANDLED = True
    QtWidgets.QMessageBox.critical(
        None,
        ELOC("ERROR"),
        f"{ELOC('UNEXPECTED_ERROR')}\n\n{''.join(traceback.format_exception(excType, excValue, excTb))}",
    )


def _ExitApplication(exitCode: int) -> None:
    app = QtWidgets.QApplication.instance()
    if app is not None:
        QtCore.QTimer.singleShot(0, lambda: app.exit(exitCode))


def _HandleUnexpectedException(
    excType: type[BaseException],
    excValue: BaseException,
    excTb: TracebackType | None,
    exitProcess: bool = True,
) -> None:
    _ShowUnexpectedException(excType, excValue, excTb)
    if exitProcess:
        sys.exit(1)
    _ExitApplication(1)


class App(QApplication):
    def notify(self, receiver, event):
        try:
            return super().notify(receiver, event)
        except Exception:
            _HandleUnexpectedException(*sys.exc_info(), exitProcess=False)
            return False


def _ThreadExcepthook(args):
    _HandleUnexpectedException(args.exc_type, args.exc_value, args.exc_traceback, exitProcess=False)


def Main():
    QApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(QtCore.Qt.AA_UseHighDpiPixmaps, True)
    QApplication.setAttribute(QtCore.Qt.AA_ShareOpenGLContexts, True)
    from EditorGlobal import StartWindow, EditorStatus

    if System.AlreadyPacked():
        app_dir = os.path.dirname(sys.executable)
        if app_dir not in sys.path:
            sys.path.insert(0, app_dir)
    icon_path = os.path.join(File.GetRootPath(), "Resource", "icon.icns" if sys.platform == "darwin" else "icon.ico")
    if not os.path.exists(icon_path):
        icon_path = os.path.join(File.GetRootPath(), "Resource", "icon.ico")
    app = App(sys.argv)
    sys.excepthook = _HandleUnexpectedException
    threading.excepthook = _ThreadExcepthook
    InitConfig()
    screen = app.primaryScreen()
    theme_raw = EditorStatus.EDITOR_CONFIG[EditorStatus.APP_NAME].get("Theme", "dark_amber.xml")
    t = theme_raw.strip().lower().replace(" ", "_").replace("-", "_")
    if t == "dark":
        theme = "dark_amber.xml"
    elif t.endswith(".xml"):
        theme = t
    else:
        theme = f"{t}.xml"
    font_size_px = System.getEditorUIFontSize()
    apply_stylesheet(app, theme=theme, extra={"font_size": f"{font_size_px}px"})
    System.ApplyStyle(app, "Main.qss")
    System.ApplyEditorFont(app)

    app.setWindowIcon(QIcon(icon_path))
    start = StartWindow()
    screen = app.primaryScreen()
    fg = start.frameGeometry()
    cp = screen.availableGeometry().center() if screen else start.geometry().center()
    fg.moveCenter(cp)
    start.move(fg.topLeft())
    if START_PROJ_FILE and os.path.isfile(START_PROJ_FILE) and START_PROJ_FILE.lower().endswith(".proj"):
        File.OpenProjectFile(START_PROJ_FILE, start)
        start = None
    else:
        start.show()
    sys.exit(app.exec_())
