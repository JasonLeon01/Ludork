# -*- encoding: utf-8 -*-

from __future__ import annotations

import fnmatch
import os
import re
from collections.abc import Callable
from typing import Optional

from PyQt5 import QtCore, QtGui, QtWidgets

from EditorGlobal.QmlDialogHost import QmlDialogHost


_IMAGE_SUFFIXES = {"png", "jpg", "jpeg", "bmp", "gif", "webp"}
_TEXT_SUFFIXES = {
    "txt", "json", "py", "md", "ini", "xml", "csv", "log", "yaml", "yml",
    "html", "htm", "css", "qss", "bat", "sh", "toml", "cfg", "conf", "vert", "frag",
}
_MAX_TEXT_PREVIEW_BYTES = 256 * 1024


class FileEntryModel(QtCore.QAbstractListModel):
    NameRole = QtCore.Qt.UserRole + 1
    PathRole = QtCore.Qt.UserRole + 2
    IsDirectoryRole = QtCore.Qt.UserRole + 3
    IsImageRole = QtCore.Qt.UserRole + 4
    SourceRole = QtCore.Qt.UserRole + 5
    DetailRole = QtCore.Qt.UserRole + 6
    SelectedRole = QtCore.Qt.UserRole + 7

    def __init__(self, parent: QtCore.QObject | None = None) -> None:
        super().__init__(parent)
        self._entries: list[dict[str, object]] = []
        self._selectedPath = ""

    def roleNames(self) -> dict[int, bytes]:
        return {
            self.NameRole: b"entryName",
            self.PathRole: b"entryPath",
            self.IsDirectoryRole: b"isDirectory",
            self.IsImageRole: b"isImage",
            self.SourceRole: b"imageSource",
            self.DetailRole: b"entryDetail",
            self.SelectedRole: b"isSelected",
        }

    def rowCount(self, parent: QtCore.QModelIndex = QtCore.QModelIndex()) -> int:
        return 0 if parent.isValid() else len(self._entries)

    def data(self, index: QtCore.QModelIndex, role: int = QtCore.Qt.DisplayRole) -> object:
        if not index.isValid() or not 0 <= index.row() < len(self._entries):
            return None
        entry = self._entries[index.row()]
        if role in (QtCore.Qt.DisplayRole, self.NameRole):
            return entry["name"]
        if role == self.PathRole:
            return entry["path"]
        if role == self.IsDirectoryRole:
            return entry["isDirectory"]
        if role == self.IsImageRole:
            return entry["isImage"]
        if role == self.SourceRole:
            return entry["source"]
        if role == self.DetailRole:
            return entry["detail"]
        if role == self.SelectedRole:
            return entry["path"] == self._selectedPath
        return None

    def setEntries(self, entries: list[dict[str, object]]) -> None:
        self.beginResetModel()
        self._entries = entries
        self._selectedPath = ""
        self.endResetModel()

    def entry(self, row: int) -> dict[str, object] | None:
        if 0 <= row < len(self._entries):
            return self._entries[row]
        return None

    def setSelectedPath(self, path: str) -> None:
        if self._selectedPath == path:
            return
        self._selectedPath = path
        if self._entries:
            self.dataChanged.emit(
                self.index(0, 0), self.index(len(self._entries) - 1, 0), [self.SelectedRole]
            )


class FileSelectorDialog(QmlDialogHost):
    browserStateChanged = QtCore.pyqtSignal()

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
        self._root = os.path.abspath(root)
        self._currentDirectory = self._root
        self._filterText = filter_str
        self._nameFilters = [part.strip() for part in filter_str.split(";;") if part.strip()] or [filter_str]
        self._selectedFilterIndex = 0
        self._patterns = self._parsePatterns(self._nameFilters[0])
        self._save = save
        self._selectedPath = ""
        self._fileName = ""
        self._previewType = "none"
        self._previewSource = ""
        self._previewText = ""

        super().__init__(
            parent,
            title or ELOC("SELECT_FILE"),
            QtCore.QSize(940, 620),
            QtCore.QSize(680, 440),
            [ELOC("FILE_DIALOG_LOOK_IN"), ELOC("FILE_NAME"), ELOC("FILE_DIALOG_FILE_TYPE")],
        )
        self._model = FileEntryModel(self)
        self._refreshDirectory()
        self.loadQml(
            "Dialogs/FileSelectorDialog.qml",
            {
                "fileSelectorModel": self._model,
                "fileSelectorSaveMode": save,
                "fileSelectorFilterText": filter_str,
                "fileSelectorFilters": self._nameFilters,
            },
        )

    @QtCore.pyqtProperty(str, notify=browserStateChanged)
    def currentDirectory(self) -> str:
        return self._currentDirectory

    @QtCore.pyqtProperty(bool, notify=browserStateChanged)
    def canGoUp(self) -> bool:
        return os.path.normcase(self._currentDirectory) != os.path.normcase(self._root)

    @QtCore.pyqtProperty(str, notify=browserStateChanged)
    def selectedPath(self) -> str:
        return self._selectedPath

    @QtCore.pyqtProperty(str, notify=browserStateChanged)
    def selectedName(self) -> str:
        return self._fileName

    @QtCore.pyqtProperty(str, notify=browserStateChanged)
    def previewType(self) -> str:
        return self._previewType

    @QtCore.pyqtProperty(str, notify=browserStateChanged)
    def previewSource(self) -> str:
        return self._previewSource

    @QtCore.pyqtProperty(str, notify=browserStateChanged)
    def previewText(self) -> str:
        return self._previewText

    def selectedFiles(self) -> list[str]:
        return [self._selectedPath] if self._selectedPath else []

    def selectedNameFilter(self) -> str:
        return self._nameFilters[self._selectedFilterIndex]

    @QtCore.pyqtSlot(int)
    def setNameFilterIndex(self, index: int) -> None:
        if not 0 <= index < len(self._nameFilters) or index == self._selectedFilterIndex:
            return
        self._selectedFilterIndex = index
        self._patterns = self._parsePatterns(self._nameFilters[index])
        self._refreshDirectory()

    @QtCore.pyqtSlot()
    def navigateUp(self) -> None:
        if not self.canGoUp:
            return
        parent = os.path.dirname(self._currentDirectory)
        if self._isWithinRoot(parent):
            self._currentDirectory = parent
            self._refreshDirectory()

    @QtCore.pyqtSlot(int)
    def selectRow(self, row: int) -> None:
        entry = self._model.entry(row)
        if entry is None:
            return
        path = str(entry["path"])
        if bool(entry["isDirectory"]):
            self._selectedPath = ""
            self._fileName = ""
            self._clearPreview()
        else:
            self._selectedPath = path
            self._fileName = str(entry["name"])
            self._updatePreview(path)
        self._model.setSelectedPath(self._selectedPath)
        self.browserStateChanged.emit()

    @QtCore.pyqtSlot(int)
    def activateRow(self, row: int) -> None:
        entry = self._model.entry(row)
        if entry is None:
            return
        path = str(entry["path"])
        if bool(entry["isDirectory"]):
            if self._isWithinRoot(path):
                self._currentDirectory = path
                self._refreshDirectory()
            return
        self.selectRow(row)
        if not self._save:
            self.confirm({"fileName": self._fileName, "selectedPath": self._selectedPath})

    def _applyResult(self, result: object) -> bool:
        if not isinstance(result, dict):
            return False
        fileName = str(result.get("fileName", "")).strip()
        selectedPath = str(result.get("selectedPath", "")).strip()
        if self._save:
            if not fileName:
                return False
            path = fileName if os.path.isabs(fileName) else os.path.join(self._currentDirectory, fileName)
        else:
            path = selectedPath or self._selectedPath
            if not path or not os.path.isfile(path):
                return False
        path = os.path.abspath(path)
        if not self._isWithinRoot(path):
            return False
        self._selectedPath = path
        self._fileName = os.path.basename(path)
        return True

    def openSelect(
        self,
        onSelected: Callable[[str], None],
        onCancelled: Optional[Callable[[], None]] = None,
    ) -> None:
        def onFinished(code: int) -> None:
            self.finished.disconnect(onFinished)
            if code == QtWidgets.QDialog.Accepted:
                onSelected(self._selectedPath)
            elif onCancelled is not None:
                onCancelled()

        self.finished.connect(onFinished)
        self.open()

    def _refreshDirectory(self) -> None:
        entries: list[dict[str, object]] = []
        try:
            names = os.listdir(self._currentDirectory)
        except OSError:
            names = []
        names.sort(key=lambda name: (not os.path.isdir(os.path.join(self._currentDirectory, name)), name.casefold()))
        for name in names:
            path = os.path.abspath(os.path.join(self._currentDirectory, name))
            if not self._isWithinRoot(path):
                continue
            isDirectory = os.path.isdir(path)
            if not isDirectory and not self._matchesFilter(name):
                continue
            suffix = os.path.splitext(name)[1].lower().lstrip(".")
            isImage = not isDirectory and suffix in _IMAGE_SUFFIXES
            entries.append(
                {
                    "name": name,
                    "path": path,
                    "isDirectory": isDirectory,
                    "isImage": isImage,
                    "source": QtCore.QUrl.fromLocalFile(path).toString() if isImage else "",
                    "detail": self._entryDetail(path, isDirectory),
                }
            )
        self._selectedPath = ""
        self._fileName = ""
        self._clearPreview()
        self._model.setEntries(entries)
        self.browserStateChanged.emit()

    def _parsePatterns(self, filterText: str) -> list[str]:
        groups = re.findall(r"\(([^)]*)\)", filterText)
        patterns = [part for group in groups for part in group.split() if "*" in part or "?" in part]
        return patterns or ["*"]

    def _matchesFilter(self, name: str) -> bool:
        lower = name.casefold()
        return any(pattern in ("*", "*.*") or fnmatch.fnmatch(lower, pattern.casefold()) for pattern in self._patterns)

    def _isWithinRoot(self, path: str) -> bool:
        try:
            common = os.path.commonpath((self._root, os.path.abspath(path)))
            return os.path.normcase(common) == os.path.normcase(self._root)
        except ValueError:
            return False

    def _entryDetail(self, path: str, isDirectory: bool) -> str:
        if isDirectory:
            return ""
        try:
            size = os.path.getsize(path)
        except OSError:
            return ""
        if size < 1024:
            return f"{size} B"
        if size < 1024 * 1024:
            return f"{size / 1024:.1f} KB"
        return f"{size / (1024 * 1024):.1f} MB"

    def _updatePreview(self, path: str) -> None:
        suffix = os.path.splitext(path)[1].lower().lstrip(".")
        if suffix in _IMAGE_SUFFIXES:
            self._previewType = "image"
            self._previewSource = QtCore.QUrl.fromLocalFile(path).toString()
            self._previewText = ""
            return
        if suffix in _TEXT_SUFFIXES:
            text = self._readTextPreview(path)
            if text is not None:
                self._previewType = "text"
                self._previewSource = ""
                self._previewText = text
                return
        self._clearPreview()

    def _readTextPreview(self, path: str) -> str | None:
        try:
            fileSize = os.path.getsize(path)
            readSize = min(fileSize, _MAX_TEXT_PREVIEW_BYTES)
            with open(path, "rb") as file:
                data = file.read(readSize)
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

    def _clearPreview(self) -> None:
        self._previewType = "none"
        self._previewSource = ""
        self._previewText = ""
