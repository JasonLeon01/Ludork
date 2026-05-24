# -*- encoding: utf-8 -*-

import os
import shutil
from typing import Callable, Optional, cast
from PyQt5 import QtCore, QtGui, QtWidgets
from EditorGlobal import EditorStatus, GameData
from Utils import Panel, File
from .FilePreview import FilePreview


class FileExplorer(QtWidgets.QWidget):
    PATH_CHANGED = QtCore.pyqtSignal(str)
    FILE_CLICKED = QtCore.pyqtSignal(str)
    DATA_FILE_CHANGED = QtCore.pyqtSignal()

    def __init__(self, root_path: str, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self._root = os.path.abspath(root_path)
        self._current = self._root
        self._interactive = True

        class _Proxy(QtCore.QSortFilterProxyModel):
            def filterAcceptsRow(self, source_row: int, source_parent: QtCore.QModelIndex) -> bool:
                model = self.sourceModel()
                if model is None:
                    return True
                idx = model.index(source_row, 0, source_parent)
                if not idx.isValid():
                    return True
                info = model.fileInfo(idx)
                if info.isHidden():
                    return False
                name = info.fileName().lower()
                if name == "__pycache__":
                    return False
                if info.isDir() and name.startswith("."):
                    return False
                suf = info.suffix().lower()
                if suf in ("ini", "proj", "csproj", "vcxproj", "log", "tmp"):
                    return False
                return True

        class _View(QtWidgets.QTreeView):
            def __init__(self, owner: "FileExplorer"):
                super().__init__(owner)
                self._owner = owner
                self.setDragEnabled(False)
                self.setAcceptDrops(False)
                self.setDragDropMode(QtWidgets.QAbstractItemView.NoDragDrop)

            def startDrag(self, supportedActions: QtCore.Qt.DropActions) -> None:
                return

            def keyPressEvent(self, e: QtGui.QKeyEvent) -> None:
                if not self._owner._interactive:
                    return
                k = e.key()
                if k == QtCore.Qt.Key_Delete:
                    self._owner._deleteSelectedItems()
                    return
                if k in (QtCore.Qt.Key_Space, QtCore.Qt.Key_Return, QtCore.Qt.Key_Enter):
                    rows = self._owner._selectedSourceRows()
                    if rows:
                        i0 = rows[0]
                        p = self._owner._model.filePath(i0)
                        if k == QtCore.Qt.Key_Space:
                            if p and not self._owner._model.isDir(i0) and self._owner._isPreviewable(p):
                                self._owner._showPreview(p)
                                return
                        else:
                            if p and not self._owner._model.isDir(i0):
                                self._owner._openSystemFile(p)
                                return
                            elif p and self._owner._model.isDir(i0):
                                self._owner._setCurrentPath(p)
                                return
                super().keyPressEvent(e)

            def dragEnterEvent(self, e: QtGui.QDragEnterEvent) -> None:
                return

            def dragMoveEvent(self, e: QtGui.QDragMoveEvent) -> None:
                return

            def dropEvent(self, e: QtGui.QDropEvent) -> None:
                return

        self._model = QtWidgets.QFileSystemModel(self)
        self._model.setFilter(QtCore.QDir.AllEntries | QtCore.QDir.NoDotAndDotDot)
        self._model.setRootPath(self._root)
        self._proxy = _Proxy(self)
        self._proxy.setSourceModel(self._model)
        self._proxy.setDynamicSortFilter(True)
        self._view = _View(self)
        self._view.setModel(self._proxy)
        self._view.setRootIndex(self._proxy.mapFromSource(self._model.index(self._root)))
        self._view.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self._view.setSelectionMode(QtWidgets.QAbstractItemView.ExtendedSelection)
        self._view.setAnimated(True)
        self._view.setSortingEnabled(True)
        self._view.sortByColumn(0, QtCore.Qt.AscendingOrder)
        self._view.setColumnWidth(0, 240)
        self._view.doubleClicked.connect(self._onDoubleClicked)
        self._view.clicked.connect(self._onClicked)
        self._view.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self._view.customContextMenuRequested.connect(self._onContextMenu)

        style = cast(QtWidgets.QStyle, QtWidgets.QApplication.style())
        upIcon = style.standardIcon(QtWidgets.QStyle.SP_ArrowUp)
        self._pathEdit = QtWidgets.QLineEdit(self)
        self._pathEdit.setReadOnly(True)
        self._pathEdit.setStyleSheet("")
        self._pathEdit.setText(self._current)
        self._upButton = QtWidgets.QToolButton(self)
        self._upButton.setIcon(upIcon)
        self._upButton.clicked.connect(self._onUp)
        topBar = QtWidgets.QWidget(self)
        topLayout = QtWidgets.QHBoxLayout(topBar)
        topLayout.setContentsMargins(0, 0, 0, 0)
        topLayout.setSpacing(4)
        topLayout.addWidget(self._pathEdit, 1)
        topLayout.addWidget(self._upButton, 0, alignment=QtCore.Qt.AlignRight)
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        layout.addWidget(topBar, 0)
        layout.addWidget(self._view, 1)
        self.setMinimumHeight(160)
        Panel.ApplyDisabledOpacity(self)

    def setInteractive(self, enabled: bool) -> None:
        self._interactive = bool(enabled)
        fp = QtCore.Qt.StrongFocus if self._interactive else QtCore.Qt.NoFocus
        self._view.setFocusPolicy(fp)
        self._upButton.setEnabled(self._interactive)
        Panel.ApplyDisabledOpacity(self._upButton)

    def changeEvent(self, e: QtCore.QEvent) -> None:
        if e.type() == QtCore.QEvent.EnabledChange:
            Panel.ApplyDisabledOpacity(self)
        super().changeEvent(e)

    def _refresh(self) -> None:
        self._view.setRootIndex(self._proxy.mapFromSource(self._model.index(self._current)))

    def _isUnderRoot(self, path: str) -> bool:
        try:
            rp = os.path.normcase(os.path.abspath(path))
            rr = os.path.normcase(os.path.abspath(self._root))
            return os.path.commonpath([rp, rr]) == rr
        except Exception:
            return False

    def _setCurrentPath(self, path: str) -> None:
        if not self._isUnderRoot(path):
            return
        self._current = os.path.abspath(path)
        self._pathEdit.setText(self._current)
        self._view.setRootIndex(self._proxy.mapFromSource(self._model.index(self._current)))
        self.PATH_CHANGED.emit(self._current)

    def setCurrentPath(self, path: str) -> None:
        self._setCurrentPath(path)

    def _onDoubleClicked(self, index: QtCore.QModelIndex) -> None:
        if not self._interactive:
            return
        if not index.isValid():
            return
        src = self._proxy.mapToSource(index)
        path = self._model.filePath(src)
        if self._model.isDir(src):
            self._setCurrentPath(path)
        else:
            if not path:
                return
            ext = os.path.splitext(path)[1].lower()
            if ext == ".dat":
                self._handleDataFile(path, File.LoadData)
            else:
                if ext == ".json":
                    if self._handleDataFile(path, File.GetJSONData):
                        return
                self._openSystemFile(path)

    def _onClicked(self, index: QtCore.QModelIndex) -> None:
        if not self._interactive:
            return
        if not index.isValid():
            return
        src = self._proxy.mapToSource(index)
        try:
            path = self._model.filePath(src)
            if path and not self._model.isDir(src):
                self.FILE_CLICKED.emit(path)
        except Exception as e:
            print(f"Error while handling file click: {e}")

    def _handleDataFile(self, path: str, openFileCallable: Callable[[str], None]) -> bool:
        if not os.path.exists(path):
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("INVALID_DATA_FILE"),
                ELOC("INVALID_DATA_FILE_MESSAGE"),
            )
            return False
        try:
            data = openFileCallable(path)
        except Exception:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("INVALID_DATA_FILE"),
                ELOC("INVALID_DATA_FILE_MESSAGE"),
            )
            return False
        if not isinstance(data, dict) or "type" not in data:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("INVALID_DATA_FILE"),
                ELOC("INVALID_DATA_FILE_MESSAGE"),
            )
            return False
        dataType = data.get("type")
        baseName = os.path.splitext(os.path.basename(path))[0]
        if dataType == "map":
            items = File.mainWindow.leftList.findItems(baseName, QtCore.Qt.MatchExactly)
            if items:
                item = items[0]
                File.mainWindow.leftList.setCurrentItem(item)
                File.mainWindow._onLeftItemClicked(item)
        elif dataType == "tileset":
            File.mainWindow._onDatabaseTilesetsData()
            editor = File.mainWindow._tilesetEditor
            if editor is None:
                return False
            items = editor.listWidget.findItems(baseName, QtCore.Qt.MatchExactly)
            if items:
                item = items[0]
                editor.listWidget.setCurrentItem(item)
                editor.activateWindow()
                editor.raise_()
        elif dataType == "config":
            File.mainWindow._onDatabaseSystemConfig()
        elif dataType == "commonFunction":
            File.mainWindow._onDatabaseCommonFunctions()
            window = File.mainWindow._commonFunctionWindow
            if window is None:
                return False
            items = window._list.findItems(baseName, QtCore.Qt.MatchExactly)
            if items:
                item = items[0]
                window._list.setCurrentItem(item)
                window.activateWindow()
                window.raise_()
        elif dataType == "animation":
            animationsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Animations")
            relPath = os.path.relpath(path, animationsRoot)
            key = os.path.splitext(relPath)[0].replace("\\", "/")
            if key in GameData.animationsData:
                File.mainWindow._onDataBaseShowAnimationWindow(baseName, GameData.animationsData[key])
            else:
                QtWidgets.QMessageBox.warning(
                    self,
                    ELOC("INVALID_DATA_FILE"),
                    ELOC("INVALID_DATA_FILE_MESSAGE"),
                )
        elif dataType == "blueprint":
            blueprintsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Blueprints")
            relPath = os.path.relpath(path, blueprintsRoot)
            key = os.path.splitext(relPath)[0].replace("\\", "/")
            if key in GameData.blueprintsData:
                File.mainWindow._onDatabaseShowBlueprint(key, GameData.blueprintsData[key])
            else:
                QtWidgets.QMessageBox.warning(
                    self,
                    ELOC("INVALID_DATA_FILE"),
                    ELOC("INVALID_DATA_FILE_MESSAGE"),
                )
        else:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("INVALID_DATA_FILE"),
                ELOC("INVALID_DATA_FILE_MESSAGE"),
            )
            return False
        return True

    def _selectedSourceRows(self):
        sm = self._view.selectionModel()
        if sm is None:
            return []
        rows = sm.selectedRows()
        return [self._proxy.mapToSource(i) for i in rows]

    def _onContextMenu(self, pos: QtCore.QPoint) -> None:
        if not self._interactive:
            return
        idx = self._view.indexAt(pos)
        selectedPath = ""
        isDir = False
        if idx.isValid():
            sm = self._view.selectionModel()
            if sm is None:
                return
            if not sm.isSelected(idx):
                sm.clearSelection()
                sm.select(idx, QtCore.QItemSelectionModel.Select | QtCore.QItemSelectionModel.Rows)
                self._view.setCurrentIndex(idx)
            rows = self._selectedSourceRows()
            if rows:
                i0 = rows[0]
                selectedPath = self._model.filePath(i0)
                isDir = self._model.isDir(i0)
        menu = QtWidgets.QMenu(self)
        actNewFolder = menu.addAction(ELOC("NEW_FOLDER"))
        actOpen = menu.addAction(ELOC("OPEN_FROM_SYSTEM"))
        actDuplicate = None
        actRename = None
        actDelete = None
        if selectedPath:
            if not isDir:
                actDuplicate = menu.addAction(ELOC("DUPLICATE_FILE"))
            actRename = menu.addAction(ELOC("RENAME_FILE"))
            actDelete = menu.addAction(ELOC("DELETE"))
        viewport = self._view.viewport()
        if viewport is None:
            return
        r = menu.exec_(viewport.mapToGlobal(pos))
        if r == actNewFolder:
            self._createFolder(self._targetDirForNewFolder(selectedPath, isDir))
        elif r == actOpen:
            if selectedPath:
                self._openSystemFile(selectedPath)
        elif actDuplicate and r == actDuplicate:
            self._duplicateItem(selectedPath)
        elif actRename and r == actRename:
            self._renameItem(selectedPath)
        elif actDelete and r == actDelete:
            self._deleteSelectedItems()

    def _targetDirForNewFolder(self, selectedPath: str, isDir: bool) -> str:
        if selectedPath and isDir:
            return selectedPath
        if selectedPath and not isDir:
            return os.path.dirname(selectedPath)
        return self._current

    def _createFolder(self, targetDir: str) -> None:
        if not targetDir or not self._isUnderRoot(targetDir):
            return
        name, ok = QtWidgets.QInputDialog.getText(
            self,
            ELOC("NEW_FOLDER"),
            ELOC("NEW_FOLDER_PROMPT"),
        )
        if not ok:
            return
        name = name.strip()
        if not name:
            return
        if os.sep in name or "/" in name or "\\" in name:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("ERROR"),
                ELOC("CREATE_FOLDER_FAILED") + "\n" + name,
            )
            return
        newPath = os.path.join(targetDir, name)
        if os.path.exists(newPath):
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("ERROR"),
                ELOC("FILE_ALREADY_EXISTS").format(name=name),
            )
            return
        try:
            os.mkdir(newPath)
            self.DATA_FILE_CHANGED.emit()
        except Exception as e:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("ERROR"),
                ELOC("CREATE_FOLDER_FAILED") + "\n" + str(e),
            )

    def _duplicateItem(self, path: str) -> None:
        if not path or not os.path.isfile(path):
            return
        dirPath = os.path.dirname(path)
        base, ext = os.path.splitext(os.path.basename(path))
        candidate = os.path.join(dirPath, f"{base}_copy{ext}")
        if os.path.exists(candidate):
            i = 2
            while os.path.exists(os.path.join(dirPath, f"{base}_copy{i}{ext}")):
                i += 1
            candidate = os.path.join(dirPath, f"{base}_copy{i}{ext}")
        try:
            shutil.copy2(path, candidate)
            self.DATA_FILE_CHANGED.emit()
        except Exception as e:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("ERROR"),
                ELOC("DUPLICATE_FAILED") + "\n" + str(e),
            )

    def _renameItem(self, path: str) -> None:
        if not path:
            return
        currentName = os.path.basename(path)
        dirPath = os.path.dirname(path)
        newName, ok = QtWidgets.QInputDialog.getText(
            self,
            ELOC("RENAME_FILE"),
            ELOC("RENAME_FILE_PROMPT"),
            text=currentName,
        )
        if not ok:
            return
        newName = newName.strip()
        if not newName or newName == currentName:
            return
        if os.sep in newName or "/" in newName or "\\" in newName:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("ERROR"),
                ELOC("RENAME_FAILED") + "\n" + newName,
            )
            return
        newPath = os.path.join(dirPath, newName)
        if os.path.exists(newPath):
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("ERROR"),
                ELOC("FILE_ALREADY_EXISTS").format(name=newName),
            )
            return
        try:
            os.rename(path, newPath)
            self.DATA_FILE_CHANGED.emit()
        except Exception as e:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("ERROR"),
                ELOC("RENAME_FAILED") + "\n" + str(e),
            )

    def _deleteSelectedItems(self) -> None:
        paths = self._selectedDeletePaths()
        if not paths:
            return
        ret = QtWidgets.QMessageBox.question(
            self,
            ELOC("CONFIRM_DELETE"),
            ELOC("DELETE_CONFIRMATION"),
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
            QtWidgets.QMessageBox.No,
        )
        if ret != QtWidgets.QMessageBox.Yes:
            return

        deletedPaths = []
        failedMessages = []
        for path in paths:
            try:
                if os.path.isdir(path):
                    shutil.rmtree(path)
                else:
                    os.remove(path)
                deletedPaths.append(path)
            except Exception as e:
                failedMessages.append(f"{os.path.basename(path)}: {str(e)}")

        if deletedPaths:
            GameData.removeDataPaths(deletedPaths)
            if self._dataPathsChanged(deletedPaths):
                self.DATA_FILE_CHANGED.emit()
            if not os.path.exists(self._current):
                self._setCurrentPath(self._root)

        if failedMessages:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("DELETE_FAILED"),
                ELOC("DELETE_FAILED_MESSAGE") + "\n" + "\n".join(failedMessages),
            )

    def _selectedDeletePaths(self) -> list[str]:
        rows = self._selectedSourceRows()
        paths = []
        for row in rows:
            path = self._model.filePath(row)
            if not path or not self._isUnderRoot(path):
                continue
            if os.path.normcase(os.path.abspath(path)) == os.path.normcase(os.path.abspath(self._root)):
                continue
            if not os.path.exists(path):
                continue
            paths.append(os.path.abspath(path))
        paths.sort(key=lambda p: len(os.path.normcase(p)))
        result = []
        for path in paths:
            if any(self._isPathInside(path, parent) for parent in result):
                continue
            result.append(path)
        return result

    def _isPathInside(self, path: str, root: str) -> bool:
        try:
            p = os.path.normcase(os.path.abspath(path))
            r = os.path.normcase(os.path.abspath(root))
            return os.path.commonpath([p, r]) == r
        except Exception:
            return False

    def _dataPathsChanged(self, paths: list[str]) -> bool:
        dataRoot = os.path.join(self._root, "Data")
        for path in paths:
            if self._isPathInside(path, dataRoot) or self._isPathInside(dataRoot, path):
                return True
        return False

    def _onUp(self) -> None:
        if os.path.normcase(os.path.abspath(self._current)) == os.path.normcase(os.path.abspath(self._root)):
            return
        parent = os.path.dirname(self._current)
        if not parent or not self._isUnderRoot(parent):
            self._setCurrentPath(self._root)
        else:
            self._setCurrentPath(parent)

    def _suffix(self, p: str) -> str:
        i = p.rfind(".")
        return p[i + 1 :].lower() if i >= 0 else ""

    def _isPreviewable(self, p: str) -> bool:
        ext = self._suffix(p)
        return ext in ("png", "jpg", "jpeg", "bmp", "gif", "webp", "mp3", "wav", "ogg", "flac", "aac", "m4a")

    def _openSystemFile(self, p: str) -> None:
        if not p:
            return
        try:
            QtGui.QDesktopServices.openUrl(QtCore.QUrl.fromLocalFile(p))
        except Exception as e:
            print(f"Error while opening file {p}: {e}")

    def _showPreview(self, p: str) -> None:
        if not p:
            return
        if not os.path.exists(p):
            return
        dlg = QtWidgets.QDialog(self)
        dlg.setWindowTitle(os.path.basename(p))
        layout = QtWidgets.QVBoxLayout(dlg)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        preview = FilePreview(dlg)
        preview.setFile(p)
        layout.addWidget(preview)
        dlg.resize(640, 360)
        dlg.setModal(False)
        dlg.show()

    def setRootPath(self, root_path: str) -> None:
        self._root = os.path.abspath(root_path)
        self._model.setRootPath(self._root)
        self._setCurrentPath(self._root)
