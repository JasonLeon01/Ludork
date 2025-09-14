import sys
from PyQt5.QtGui import QFontDatabase
from PyQt5.QtWidgets import QApplication
from qt_material import apply_stylesheet
from Widgets import MainWindow


def main():
    app = QApplication(sys.argv)

    QFontDatabase()

    extra = {
        "font_family": "Roboto",
        "density_scale": "0",
        "primaryColor": "#1976D2",
        "secondaryColor": "#03A9F4",
    }

    apply_stylesheet(app, theme="dark_teal.xml", extra=extra)

    window = MainWindow()
    window.resize(600, 400)
    window.show()

    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
