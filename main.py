# -*- encoding: utf-8 -*-

import os
import sys
import locale
import configparser
from PyQt5.QtWidgets import QApplication
from qt_material import apply_stylesheet
from PyQt5.QtGui import QIcon
from Widgets import MainWindow
from Utils import Locale


editorConfig = None


def initConfig(app):
    global editorConfig
    Locale.init("./Locale")
    editorConfig = configparser.ConfigParser()
    screen = app.primaryScreen()
    screen_size = screen.size() if screen else None
    if not screen_size or screen_size.width() <= 1920 or screen_size.height() <= 1080:
        os.environ["SCREEN_LOW_RES"] = "1"
    elif screen_size.width() <= 2560 or screen_size.height() <= 1440:
        os.environ["SCREEN_LOW_RES"] = "2"
    if not os.path.exists("./Ludork.ini"):
        editorConfig["Ludork"] = {}
        if os.environ.get("SCREEN_LOW_RES"):
            if os.environ["SCREEN_LOW_RES"] == "1":
                editorConfig["Ludork"]["Width"] = "1280"
                editorConfig["Ludork"]["Height"] = "720"
                editorConfig["Ludork"]["UpperLeftWidth"] = "320"
                editorConfig["Ludork"]["UpperRightWidth"] = "320"
            else:
                editorConfig["Ludork"]["Width"] = "1920"
                editorConfig["Ludork"]["Height"] = "1080"
                editorConfig["Ludork"]["UpperLeftWidth"] = "480"
                editorConfig["Ludork"]["UpperRightWidth"] = "480"
        else:
            editorConfig["Ludork"]["Width"] = "2560"
            editorConfig["Ludork"]["Height"] = "1440"
            editorConfig["Ludork"]["UpperLeftWidth"] = "640"
            editorConfig["Ludork"]["UpperRightWidth"] = "640"
        lang, _ = locale.getdefaultlocale()
        editorConfig["Ludork"]["Language"] = lang if lang else "en_GB"
        with open("./Ludork.ini", "w") as f:
            editorConfig.write(f)
    else:
        editorConfig.read("./Ludork.ini")
    os.environ["LANGUAGE"] = editorConfig["Ludork"]["Language"]


def main():
    global editorConfig
    icon_path = "./Resource/icon.ico"
    app = QApplication(sys.argv)
    initConfig(app)
    apply_stylesheet(app, theme="light_cyan_500.xml")
    window = MainWindow("Ludork Editor", "./Sample")
    app.setWindowIcon(QIcon(icon_path))
    window.setWindowIcon(QIcon(icon_path))
    window.resize(int(editorConfig["Ludork"]["Width"]), int(editorConfig["Ludork"]["Height"]))
    screen = app.primaryScreen()
    fg = window.frameGeometry()
    cp = screen.availableGeometry().center() if screen else window.geometry().center()
    fg.moveCenter(cp)
    window.move(fg.topLeft())
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
