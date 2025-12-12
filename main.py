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
from Widgets import MainWindow
from Utils import Locale, System
import EditorStatus
import Data

editorConfig = None


def initConfig():
    global editorConfig
    if not System.already_packed():
        subprocess.run([sys.executable, "localeTransfer.py", os.path.join(".", "Locale", "locale.json")], check=True)
    Locale.init("./Locale")
    editorConfig = configparser.ConfigParser()
    if not os.path.exists("./Ludork.ini"):
        editorConfig["Ludork"] = {}
        editorConfig["Ludork"]["Width"] = "1280"
        editorConfig["Ludork"]["Height"] = "720"
        editorConfig["Ludork"]["UpperLeftWidth"] = "320"
        editorConfig["Ludork"]["UpperRightWidth"] = "320"
        lang, _ = locale.getdefaultlocale()
        editorConfig["Ludork"]["Language"] = lang if lang else "en_GB"
        editorConfig["Ludork"]["Theme"] = "dark_blue.xml"
        with open("./Ludork.ini", "w") as f:
            editorConfig.write(f)
    else:
        editorConfig.read("./Ludork.ini")
    EditorStatus.LANGUAGE = editorConfig["Ludork"]["Language"]


def main():
    global editorConfig
    icon_path = "./Resource/icon.ico"
    QApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(QtCore.Qt.AA_UseHighDpiPixmaps, True)
    app = QApplication(sys.argv)
    initConfig()
    screen = app.primaryScreen()
    theme_raw = editorConfig["Ludork"].get("Theme", "dark_blue.xml")
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
    projPath = os.path.abspath("./Sample")
    EditorStatus.PROJ_PATH = projPath
    if not EditorStatus.PROJ_PATH in sys.path:
        sys.path.append(EditorStatus.PROJ_PATH)
    Data.GameData.init()
    window = MainWindow("Ludork Editor")
    window.resize(int(editorConfig["Ludork"]["Width"]), int(editorConfig["Ludork"]["Height"]))
    app.aboutToQuit.connect(window.endGame)
    app.setWindowIcon(QIcon(icon_path))
    window.setWindowIcon(QIcon(icon_path))
    screen = app.primaryScreen()
    fg = window.frameGeometry()
    cp = screen.availableGeometry().center() if screen else window.geometry().center()
    fg.moveCenter(cp)
    window.move(fg.topLeft())
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
