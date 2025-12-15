# -*- encoding: utf-8 -*-

import os
import sys
import locale
import configparser
import subprocess
from PyQt5 import QtCore
from PyQt5.QtWidgets import QApplication
from qt_material import apply_stylesheet
from PyQt5.QtGui import QIcon
from Widgets import StartWindow
from Utils import Locale, System, File
import EditorStatus


def initConfig():
    if not System.already_packed():
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
        EditorStatus.editorConfig[EditorStatus.APP_NAME]["Language"] = lang if lang else "en_GB"
        EditorStatus.editorConfig[EditorStatus.APP_NAME]["Theme"] = "dark_blue.xml"
        with open(os.path.join(File.getIniPath(), f"{EditorStatus.APP_NAME}.ini"), "w") as f:
            EditorStatus.editorConfig.write(f)
    else:
        EditorStatus.editorConfig.read(os.path.join(File.getIniPath(), f"{EditorStatus.APP_NAME}.ini"))
    EditorStatus.LANGUAGE = EditorStatus.editorConfig[EditorStatus.APP_NAME]["Language"]


def main():
    icon_path = "./Resource/icon.ico"
    QApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(QtCore.Qt.AA_UseHighDpiPixmaps, True)
    app = QApplication(sys.argv)
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
    app.setStyleSheet(
        app.styleSheet()
        + "\n"
        + "QScrollBar:vertical { background: transparent; width: 10px; margin: 0px; }\n"
        + "QScrollBar::handle:vertical { background: #ffffff; border-radius: 5px; min-height: 24px; }\n"
        + "QScrollBar::handle:vertical:hover { background: #e6e6e6; }\n"
        + "QScrollBar:horizontal { background: transparent; height: 10px; margin: 0px; }\n"
        + "QScrollBar::handle:horizontal { background: #ffffff; border-radius: 5px; min-width: 24px; }\n"
        + "QScrollBar::handle:horizontal:hover { background: #e6e6e6; }\n"
    )
    app.setWindowIcon(QIcon(icon_path))
    start = StartWindow()
    screen = app.primaryScreen()
    fg = start.frameGeometry()
    cp = screen.availableGeometry().center() if screen else start.geometry().center()
    fg.moveCenter(cp)
    start.move(fg.topLeft())
    start.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
