from __future__ import annotations
from PyQt5 import QtWidgets
from Widgets.Utils.WU_NodeGraphPanel import NodePanel


class NodeGraphWindow(QtWidgets.QMainWindow):
    def __init__(self, parent, graph, key: str):
        super().__init__(parent)
        self.setWindowTitle("Common Functions")
        panel = NodePanel(self, graph, key)
        self.setCentralWidget(panel)
        self.setMinimumSize(1200, 800)
