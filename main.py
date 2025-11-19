# -*- encoding: utf-8 -*-

import sys
from PyQt5.QtWidgets import QApplication
from PyQt5.QtGui import QIcon
from Widgets import MainWindow


def main():
    icon_path = "./Resource/icon.ico"
    app = QApplication(sys.argv)
    window = MainWindow("Ludork Editor", "./Sample")
    app.setWindowIcon(QIcon(icon_path))
    window.setWindowIcon(QIcon(icon_path))
    window.resize(1280, 960)
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
