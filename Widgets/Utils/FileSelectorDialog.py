# -*- encoding: utf-8 -*-

import os
from typing import Optional, cast
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import System


_THUMB_SIZE = 80
_HIDDEN_TOOLBAR_BUTTONS = frozenset({"newFolderButton", "listModeButton", "detailModeButton"})
_TEXT_SUFFIXES = {
    "txt", "json", "py", "md", "ini", "xml", "csv", "log", "yaml", "yml",
    "html", "htm", "css", "qss", "bat", "sh", "toml", "cfg", "conf", "vert", "frag",
}
_MAX_TEXT_PREVIEW_BYTES = 256 * 1024


class _ThumbnailIconProvider(QtWidgets.QFileIconProvider):
    _IMAGE_EXTS = {"png", "jpg", "jpeg", "bmp", "gif", "webp"}

    def __init__(self, thumbSize: int = _THUMB_SIZE):
        super().__init__()
        self._thumbSize = thumbSize
        self._cache: dict[str, QtGui.QIcon] = {}

    def icon(self, info: QtCore.QFileInfo) -> QtGui.QIcon:
        if not info.isDir():
            ext = info.suffix().lower()
            if ext in self._IMAGE_EXTS:
                path = info.absoluteFilePath()
                if path in self._cache:
                    return self._cache[path]
                pm = QtGui.QPixmap(path)
                if not pm.isNull():
                    scaled = pm.scaled(
                        self._thumbSize,
                        self._thumbSize,
                        QtCore.Qt.KeepAspectRatio,
                        QtCore.Qt.SmoothTransformation,
                    )
                    icon = QtGui.QIcon(scaled)
                    self._cache[path] = icon
                    return icon
        return super().icon(info)


class FileSelectorDialog(QtWidgets.QFileDialog):
    _IMAGE_SUFFIXES = {"png", "jpg", "jpeg", "bmp", "gif", "webp"}
    _PREVIEW_IMAGE = 0
    _PREVIEW_TEXT = 1

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

    def __init__(
        self,
        parent: Optional[QtWidgets.QWidget],
        root: str,
        filter_str: str,
        title: str | None = None,
        *,
        save: bool = False,
    ) -> None:
        super().__init__(parent)
        self.setOption(QtWidgets.QFileDialog.DontUseNativeDialog, True)
        self._root = os.path.abspath(root)
        self._previewPixmap = None
        self.setWindowTitle(title or ELOC("SELECT_FILE"))
        System.SetStyle(self, "fileSelector.qss")
        if save:
            self.setAcceptMode(QtWidgets.QFileDialog.AcceptSave)
            self.setFileMode(QtWidgets.QFileDialog.AnyFile)
        else:
            self.setOption(QtWidgets.QFileDialog.ReadOnly, True)
            self.setFileMode(QtWidgets.QFileDialog.ExistingFile)
        self.setDirectory(self._root)
        self.setNameFilter(filter_str)
        self._localizeLabels(save)
        self._preview = QtWidgets.QFrame(self)
        self._preview.setObjectName("FileSelectorPreview")
        self._preview.setMinimumWidth(220)
        self._preview.setMaximumWidth(320)
        previewLayout = QtWidgets.QVBoxLayout(self._preview)
        previewLayout.setContentsMargins(8, 8, 8, 8)
        self._previewStack = QtWidgets.QStackedWidget(self._preview)
        self._previewLabel = QtWidgets.QLabel(self._previewStack)
        self._previewLabel.setAlignment(QtCore.Qt.AlignCenter)
        self._previewLabel.setMinimumHeight(160)
        self._previewLabel.installEventFilter(self)
        self._previewText = QtWidgets.QPlainTextEdit(self._previewStack)
        self._previewText.setObjectName("FileSelectorPreviewText")
        self._previewText.setReadOnly(True)
        self._previewText.setLineWrapMode(QtWidgets.QPlainTextEdit.WidgetWidth)
        self._previewText.setMinimumHeight(160)
        previewFont = QtGui.QFont("Consolas")
        previewFont.setStyleHint(QtGui.QFont.Monospace)
        self._previewText.setFont(previewFont)
        self._previewStack.addWidget(self._previewLabel)
        self._previewStack.addWidget(self._previewText)
        previewLayout.addWidget(self._previewStack, 1)
        self._installPreview()
        System.hideFileDialogSidebar(self)
        self.directoryEntered.connect(self._onDirEntered)
        self.currentChanged.connect(self._onCurrentChanged)
        self._setupToolbarButtons()
        self._thumbnailProvider = _ThumbnailIconProvider(_THUMB_SIZE)
        self._iconModeApplied = False

    def _setupToolbarButtons(self) -> None:
        for b in self.findChildren(QtWidgets.QToolButton):
            if b.objectName() in _HIDDEN_TOOLBAR_BUTTONS:
                b.setVisible(False)
                continue
            b.setAutoRaise(False)
            b.setIconSize(QtCore.QSize(20, 20))

    def showEvent(self, event: QtGui.QShowEvent) -> None:
        super().showEvent(event)
        if not self._iconModeApplied:
            self._iconModeApplied = True
            self.setViewMode(QtWidgets.QFileDialog.List)
            listView = self.findChild(QtWidgets.QListView, "listView")
            if listView is not None:
                listView.setViewMode(QtWidgets.QListView.IconMode)
                listView.setIconSize(QtCore.QSize(_THUMB_SIZE, _THUMB_SIZE))
                listView.setGridSize(QtCore.QSize(_THUMB_SIZE * 2, _THUMB_SIZE + 30))
                listView.setWordWrap(True)
                listView.setSpacing(4)
                listView.setUniformItemSizes(True)
                listView.setResizeMode(QtWidgets.QListView.Adjust)
                listView.setWrapping(True)
            fsModel = self.findChild(QtWidgets.QFileSystemModel)
            if fsModel is not None:
                fsModel.setIconProvider(self._thumbnailProvider)
            treeView = self.findChild(QtWidgets.QTreeView)
            if treeView is not None:
                treeView.setVisible(False)

    def _localizeLabels(self, save: bool = False) -> None:
        self.setLabelText(QtWidgets.QFileDialog.LookIn, ELOC("FILE_DIALOG_LOOK_IN"))
        self.setLabelText(QtWidgets.QFileDialog.FileName, ELOC("FILE_NAME"))
        self.setLabelText(QtWidgets.QFileDialog.FileType, ELOC("FILE_DIALOG_FILE_TYPE"))
        self.setLabelText(QtWidgets.QFileDialog.Accept, ELOC("SAVE") if save else ELOC("FILE_DIALOG_OPEN"))
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
        if not path or not os.path.isfile(path):
            self._clearPreview()
            return
        if self._isImagePath(path):
            pixmap = QtGui.QPixmap(path)
            self._previewPixmap = pixmap if not pixmap.isNull() else None
            self._previewStack.setCurrentIndex(self._PREVIEW_IMAGE)
            self._updatePreviewPixmap()
            return
        if self._isTextPath(path):
            text = self._readTextPreview(path)
            if text is not None:
                self._previewPixmap = None
                self._previewText.setPlainText(text)
                self._previewStack.setCurrentIndex(self._PREVIEW_TEXT)
                return
        self._clearPreview()

    def _isImagePath(self, path: str) -> bool:
        suffix = os.path.splitext(path)[1].lower().lstrip(".")
        return suffix in self._IMAGE_SUFFIXES

    def _isTextPath(self, path: str) -> bool:
        suffix = os.path.splitext(path)[1].lower().lstrip(".")
        return suffix in _TEXT_SUFFIXES

    def _readTextPreview(self, path: str) -> str | None:
        try:
            fileSize = os.path.getsize(path)
            readSize = min(fileSize, _MAX_TEXT_PREVIEW_BYTES)
            with open(path, "rb") as f:
                data = f.read(readSize)
            if not data or b"\x00" in data[:8192]:
                return None
            for encoding in ("utf-8", "utf-8-sig", "gbk", "latin-1"):
                try:
                    text = data.decode(encoding)
                    break
                except UnicodeDecodeError:
                    continue
            else:
                return None
            if fileSize > _MAX_TEXT_PREVIEW_BYTES:
                text += "\n\n..."
            return text
        except OSError:
            return None

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
        self._previewText.clear()

    def execSelect(self) -> str:
        if self.exec_() != QtWidgets.QDialog.Accepted:
            return ""
        sel = self.selectedFiles()
        if not sel:
            return ""
        return sel[0]
