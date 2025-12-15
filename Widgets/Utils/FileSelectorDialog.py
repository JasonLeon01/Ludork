import os
from PyQt5 import QtCore, QtWidgets


class FileSelectorDialog(QtWidgets.QFileDialog):
    def __init__(self, parent: QtWidgets.QWidget, root: str, filter_str: str, title: str = "Select File") -> None:
        super().__init__(parent, title, root, filter_str)
        self._root = os.path.abspath(root)
        self.setStyleSheet(
            "QFileDialog { color: white; }"
            "QLabel { color: white; }"
            "QComboBox { color: white; background-color: #3a3a3a; }"
            "QComboBox QAbstractItemView { color: white; background-color: #3a3a3a; }"
            "QLineEdit { color: white; background-color: #3a3a3a; }"
            "QToolButton { color: white; background-color: #3a3a3a; border: 1px solid #606060; border-radius: 4px; padding: 4px; }"
            "QToolButton:hover { background-color: #4a4a4a; }"
            "QToolButton:checked { background-color: #5a5a5a; }"
            "QPushButton { color: white; }"
            "QListView, QTreeView { color: white; background-color: #2b2b2b; }"
        )
        self.setOption(QtWidgets.QFileDialog.DontUseNativeDialog, True)
        self.setOption(QtWidgets.QFileDialog.ReadOnly, True)
        self.setFileMode(QtWidgets.QFileDialog.ExistingFile)
        self.setDirectory(self._root)
        self.setNameFilter(filter_str)
        try:
            self.setSidebarUrls([QtCore.QUrl.fromLocalFile(self._root)])
        except Exception as e:
            print(f"Error setting sidebar URLs: {e}")
        self.directoryEntered.connect(self._on_dir_entered)
        for b in self.findChildren(QtWidgets.QToolButton):
            b.setAutoRaise(False)
            b.setIconSize(QtCore.QSize(20, 20))

    def _on_dir_entered(self, path: str) -> None:
        rp = os.path.normcase(self._root)
        pp = os.path.normcase(os.path.abspath(path))
        if rp != pp:
            self.setDirectory(self._root)

    def execSelect(self) -> str:
        if self.exec_() != QtWidgets.QDialog.Accepted:
            return ""
        sel = self.selectedFiles()
        if not sel:
            return ""
        return sel[0]
