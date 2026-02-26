# -*- encoding: utf-8 -*-

import os
import sys
import locale
import configparser
import runpy
import subprocess
from PyQt5 import QtCore, QtWidgets
from PyQt5.QtWidgets import QApplication
from qt_material import apply_stylesheet
from PyQt5.QtGui import QIcon
from W_StartWindow import StartWindow
from Utils import Locale, System, File
import EditorStatus
import traceback
import threading

START_PROJ_FILE = None


def initConfig():
    if not System.alreadyPacked():
        subprocess.run([sys.executable, "localeTransfer.py", os.path.join(".", "Locale", "locale.json")], check=True)
    Locale.init(os.path.join(File.getRootPath(), "Locale"))
    EditorStatus.editorConfig = configparser.ConfigParser()
    if not os.path.exists(os.path.join(File.getIniPath(), f"{EditorStatus.APP_NAME}.ini")):
        EditorStatus.editorConfig[EditorStatus.APP_NAME] = {}
        EditorStatus.editorConfig[EditorStatus.APP_NAME]["Width"] = "1280"
        EditorStatus.editorConfig[EditorStatus.APP_NAME]["Height"] = "720"
        EditorStatus.editorConfig[EditorStatus.APP_NAME]["UpperLeftWidth"] = "320"
        EditorStatus.editorConfig[EditorStatus.APP_NAME]["UpperRightWidth"] = "320"
        lang, _ = locale.getdefaultlocale()
        EditorStatus.editorConfig[EditorStatus.APP_NAME]["Language"] = (
            lang if lang in Locale.getLocaleKeys() else "en_GB"
        )
        EditorStatus.editorConfig[EditorStatus.APP_NAME]["Theme"] = "dark_blue.xml"
        with open(os.path.join(File.getIniPath(), f"{EditorStatus.APP_NAME}.ini"), "w") as f:
            EditorStatus.editorConfig.write(f)
    else:
        EditorStatus.editorConfig.read(os.path.join(File.getIniPath(), f"{EditorStatus.APP_NAME}.ini"))
    EditorStatus.LANGUAGE = EditorStatus.editorConfig[EditorStatus.APP_NAME]["Language"]


def _handleUnexpectedException(exc_type, exc_value, exc_tb):
    traceback.print_exception(exc_type, exc_value, exc_tb, file=sys.stderr)
    try:
        sys.stderr.flush()
    except Exception:
        pass
    QtWidgets.QMessageBox.critical(
        None,
        Locale.getContent("ERROR"),
        f"{Locale.getContent('UNEXPECTED_ERROR')}\n\n{''.join(traceback.format_exception(exc_type, exc_value, exc_tb))}",
    )
    sys.exit(1)


class App(QApplication):
    def notify(self, receiver, event):
        try:
            return super().notify(receiver, event)
        except Exception:
            _handleUnexpectedException(*sys.exc_info())
            return False


def _thread_excepthook(args):
    _handleUnexpectedException(args.exc_type, args.exc_value, args.exc_traceback)


def main():
    if System.alreadyPacked():
        app_dir = os.path.dirname(sys.executable)
        if app_dir not in sys.path:
            sys.path.insert(0, app_dir)
    icon_path = "./Resource/icon.ico"
    QApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(QtCore.Qt.AA_UseHighDpiPixmaps, True)
    app = App(sys.argv)
    sys.excepthook = _handleUnexpectedException
    threading.excepthook = _thread_excepthook
    initConfig()
    screen = app.primaryScreen()
    theme_raw = EditorStatus.editorConfig[EditorStatus.APP_NAME].get("Theme", "dark_blue.xml")
    t = theme_raw.strip().lower().replace(" ", "_").replace("-", "_")
    if t == "dark":
        theme = "dark_blue.xml"
    elif t.endswith(".xml"):
        theme = t
    else:
        theme = f"{t}.xml"
    apply_stylesheet(app, theme=theme, extra={"font_size": "12px"})
    System.applyStyle(app, "main.qss")

    app.setWindowIcon(QIcon(icon_path))
    start = StartWindow()
    screen = app.primaryScreen()
    fg = start.frameGeometry()
    cp = screen.availableGeometry().center() if screen else start.geometry().center()
    fg.moveCenter(cp)
    start.move(fg.topLeft())
    if START_PROJ_FILE and os.path.isfile(START_PROJ_FILE) and START_PROJ_FILE.lower().endswith(".proj"):
        File._openProjectPath(os.path.dirname(os.path.abspath(START_PROJ_FILE)), start)
    else:
        start.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    try:
        params = sys.argv.copy()
        if len(params) > 1:
            if not os.environ.get("WINDOWHANDLE", None) is None:
                sys.argv = sys.argv[1:]
                sys.argv[0] = os.path.abspath(params[1])
                if not os.getcwd() in sys.path:
                    sys.path.append(os.getcwd())
                runpy.run_path(sys.argv[0], run_name="__main__")
            else:
                arg1 = params[1]
                if isinstance(arg1, str) and arg1.lower().endswith(".proj") and os.path.isfile(arg1):
                    START_PROJ_FILE = os.path.abspath(arg1)
                    main()
        else:
            main()
    except Exception:
        _handleUnexpectedException(*sys.exc_info())
