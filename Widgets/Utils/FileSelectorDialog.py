# -*- encoding: utf-8 -*-

import os
from typing import cast
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import System


class FileSelectorDialog(QtWidgets.QFileDialog):
    _IMAGE_SUFFIXES = {"png", "jpg", "jpeg", "bmp", "gif", "webp"}

    @staticmethod
    def allFilesFilter(star: bool = False) -> str:
        return ELOC("FILE_FILTER_ALL_STAR") if star else ELOC("FILE_FILTER_ALL")

    @staticmethod
    def audioFilesFilter() -> str:
        return ELOC("FILE_FILTER_AUDIO")

    @staticmethod
    def imageFilesFilter() -> str:
        return ELOC("FILE_FILTER_IMAGES")

    @staticmethod
    def filesFilter(patterns: list[str]) -> str:
        return ELOC("FILE_FILTER_FILES").format(patterns=" ".join(patterns))

    def __init__(self, parent: QtWidgets.QWidget, root: str, filter_str: str, title: str | None = None) -> None:
        super().__init__(parent, title or ELOC("SELECT_FILE"), root, filter_str)
        self._root = os.path.abspath(root)
        self._previewPixmap = None
        System.SetStyle(self, "fileSelector.qss")
        self.setOption(QtWidgets.QFileDialog.DontUseNativeDialog, True)
        self.setOption(QtWidgets.QFileDialog.ReadOnly, True)
        self.setFileMode(QtWidgets.QFileDialog.ExistingFile)
        self.setDirectory(self._root)
        self.setNameFilter(filter_str)
        self._localizeLabels()
        self._preview = QtWidgets.QFrame(self)
        self._preview.setObjectName("FileSelectorPreview")
        self._preview.setMinimumWidth(220)
        self._preview.setMaximumWidth(320)
        previewLayout = QtWidgets.QVBoxLayout(self._preview)
        previewLayout.setContentsMargins(8, 8, 8, 8)
        self._previewLabel = QtWidgets.QLabel(self._preview)
        self._previewLabel.setAlignment(QtCore.Qt.AlignCenter)
        self._previewLabel.setMinimumHeight(160)
        self._previewLabel.installEventFilter(self)
        previewLayout.addWidget(self._previewLabel, 1)
        self._installPreview()
        try:
            self.setSidebarUrls([QtCore.QUrl.fromLocalFile(self._root)])
        except Exception as e:
            print(f"Error setting sidebar URLs: {e}")
        self.directoryEntered.connect(self._onDirEntered)
        self.currentChanged.connect(self._onCurrentChanged)
        for b in self.findChildren(QtWidgets.QToolButton):
            b.setAutoRaise(False)
            b.setIconSize(QtCore.QSize(20, 20))

    def _localizeLabels(self) -> None:
        self.setLabelText(QtWidgets.QFileDialog.LookIn, ELOC("FILE_DIALOG_LOOK_IN"))
        self.setLabelText(QtWidgets.QFileDialog.FileName, ELOC("FILE_NAME"))
        self.setLabelText(QtWidgets.QFileDialog.FileType, ELOC("FILE_DIALOG_FILE_TYPE"))
        self.setLabelText(QtWidgets.QFileDialog.Accept, ELOC("FILE_DIALOG_OPEN"))
        self.setLabelText(QtWidgets.QFileDialog.Reject, ELOC("CANCEL"))

    def _installPreview(self) -> None:
        layout = cast(QtWidgets.QLayout, self.layout())
        if isinstance(layout, QtWidgets.QGridLayout):
            layout.addWidget(self._preview, 1, layout.columnCount(), 1, 1)
        else:
            layout.addWidget(self._preview)
        self.resize(self.width() + self._preview.minimumWidth(), self.height())

    def eventFilter(self, obj: QtCore.QObject, event: QtCore.QEvent) -> bool:
        if obj is self._previewLabel and event.type() == QtCore.QEvent.Resize:
            self._updatePreviewPixmap()
        return super().eventFilter(obj, event)

    def _onDirEntered(self, path: str) -> None:
        rp = os.path.normcase(self._root)
        pp = os.path.normcase(os.path.abspath(path))
        if pp != rp and not pp.startswith(rp + os.sep):
            self.setDirectory(self._root)

    def _onCurrentChanged(self, path: str) -> None:
        if path and os.path.isfile(path) and self._isImagePath(path):
            pixmap = QtGui.QPixmap(path)
            self._previewPixmap = pixmap if not pixmap.isNull() else None
            self._updatePreviewPixmap()
        else:
            self._clearPreview()

    def _isImagePath(self, path: str) -> bool:
        suffix = os.path.splitext(path)[1].lower().lstrip(".")
        return suffix in self._IMAGE_SUFFIXES

    def _updatePreviewPixmap(self) -> None:
        if self._previewPixmap is None:
            self._previewLabel.clear()
            return
        if self._previewLabel.width() <= 0 or self._previewLabel.height() <= 0:
            return
        scaled = self._previewPixmap.scaled(
            self._previewLabel.size(), QtCore.Qt.KeepAspectRatio, QtCore.Qt.SmoothTransformation
        )
        self._previewLabel.setPixmap(scaled)

    def _clearPreview(self) -> None:
        self._previewPixmap = None
        self._previewLabel.clear()

    def execSelect(self) -> str:
        if self.exec_() != QtWidgets.QDialog.Accepted:
            return ""
        sel = self.selectedFiles()
        if not sel:
            return ""
        return sel[0]
