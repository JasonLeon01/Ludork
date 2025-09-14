# -*- encoding: utf-8 -*-

from PyQt5.QtWidgets import (
    QMainWindow,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
    QMessageBox,
    QToolBar,
    QStatusBar,
    QAction,
)


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Material PyQt5 Demo")

        menubar = self.menuBar()
        file_menu = menubar.addMenu("Files")
        help_menu = menubar.addMenu("Help")

        exit_action = QAction("Exit", self)
        exit_action.triggered.connect(self.close)
        file_menu.addAction(exit_action)

        about_action = QAction("About", self)
        about_action.triggered.connect(self.show_about_dialog)
        help_menu.addAction(about_action)

        toolbar = QToolBar("Tools")
        self.addToolBar(toolbar)
        toolbar.addAction(exit_action)
        toolbar.addAction(about_action)

        central_widget = QWidget()
        layout = QVBoxLayout()
        central_widget.setLayout(layout)
        self.setCentralWidget(central_widget)

        self.label = QLabel("Welcome to Material Style PyQt5 Interface!")
        button = QPushButton("Click me to show dialog")
        button.clicked.connect(self.show_message)

        layout.addWidget(self.label)
        layout.addWidget(button)

        self.setStatusBar(QStatusBar(self))

    def show_message(self):
        msg = QMessageBox(self)
        msg.setWindowTitle("提示")
        msg.setText("你点击了按钮！")
        msg.setStandardButtons(QMessageBox.StandardButton.Ok)
        msg.exec()

    def show_about_dialog(self):
        QMessageBox.about(self, "About", "This is a Material style PyQt5 example.")
