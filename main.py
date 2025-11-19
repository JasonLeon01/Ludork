# -*- encoding: utf-8 -*-

import os
import sys
import locale
import configparser
from PyQt5.QtWidgets import QApplication
from PyQt5.QtGui import QIcon
from Widgets import MainWindow
from Game import Locale


editorConfig = None


def initConfig(app):
    global editorConfig
    Locale.Locale.init("./Locale")
    editorConfig = configparser.ConfigParser()
    screen = app.primaryScreen()
    screen_size = screen.size() if screen else None
    if not screen_size or screen_size.width() < 2000 or screen_size.height() < 1500:
        os.environ["SCREEN_LOW_RES"] = "1"
    if not os.path.exists("./Ludork.ini"):
        editorConfig["Ludork"] = {}
        if os.environ.get("SCREEN_LOW_RES"):
            editorConfig["Ludork"]["Width"] = "1280"
            editorConfig["Ludork"]["Height"] = "720"
        else:
            editorConfig["Ludork"]["Width"] = "2560"
            editorConfig["Ludork"]["Height"] = "1440"
        editorConfig["Ludork"]["Maximized"] = "0"
        lang, _ = locale.getdefaultlocale()
        editorConfig["Ludork"]["Language"] = lang if lang else "en_GB"
        os.environ["LANGUAGE"] = editorConfig["Ludork"]["Language"]
        with open("./Ludork.ini", "w") as f:
            editorConfig.write(f)
    else:
        editorConfig.read("./Ludork.ini")


def main():
    global editorConfig
    icon_path = "./Resource/icon.ico"
    app = QApplication(sys.argv)
    initConfig(app)
    window = MainWindow("Ludork Editor", "./Sample")
    app.setWindowIcon(QIcon(icon_path))
    window.setWindowIcon(QIcon(icon_path))
    window.resize(int(editorConfig["Ludork"]["Width"]), int(editorConfig["Ludork"]["Height"]))
    screen = app.primaryScreen()
    fg = window.frameGeometry()
    cp = screen.availableGeometry().center() if screen else window.geometry().center()
    fg.moveCenter(cp)
    window.move(fg.topLeft())
    if editorConfig["Ludork"]["Maximized"] == "1":
        window.showMaximized()
    else:
        window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
