from PyQt5 import QtCore
from PyQt5.QtWidgets import QWidget
from Utils import File

class MarkdownPreviewer(QWidget):
    def __init__(self, parent=None, filePath: str = ""):
        super().__init__(parent=parent)
        self.resize(800, 600)
        
        self.setWindowFlags(QtCore.Qt.Window)        

    def set_text(self, mdContent):
        pass
