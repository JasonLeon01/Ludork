# -*- encoding: utf-8 -*-

import os
import shutil
import stat
from typing import Optional
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import Locale
from .W_FilePreview import FilePreview


class FileExplorer(QtWidgets.QWidget):
    def __init__(self, root_path: str, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self._root = os.path.abspath(root_path)
        self._current = self._root
        self._clipboard = []
        self._clipboard_cut = False
        class _Proxy(QtCore.QSortFilterProxyModel):
            def filterAcceptsRow(self, source_row: int, source_parent: QtCore.QModelIndex) -> bool:
                model = self.sourceModel()
                idx = model.index(source_row, 0, source_parent)
                if not idx.isValid():
                    return True
                info = model.fileInfo(idx)
                if info.isHidden():
                    return False
                name = info.fileName().lower()
                if name == "__pycache__":
                    return False
                if info.isDir() and name.startswith('.'):
                    return False
                suf = info.suffix().lower()
                if suf in ("ini", "proj", "csproj", "vcxproj", "log", "tmp"):
                    return False
                return True
        class _View(QtWidgets.QTreeView):
            def __init__(self, owner: 'FileExplorer'):
                super().__init__(owner)
                self._owner = owner
                self.setDragEnabled(True)
                self.setAcceptDrops(True)
                self.setDragDropMode(QtWidgets.QAbstractItemView.DragDrop)
                self.setDefaultDropAction(QtCore.Qt.MoveAction)
            def startDrag(self, supportedActions: QtCore.Qt.DropActions) -> None:
                sel = self.selectionModel().selectedRows()
                if not sel:
                    return
                urls = []
                for i in sel:
                    src = self._owner._proxy.mapToSource(i)
                    p = self._owner._model.filePath(src)
                    if p:
                        urls.append(QtCore.QUrl.fromLocalFile(p))
                mime = QtCore.QMimeData()
                mime.setUrls(urls)
                drag = QtGui.QDrag(self)
                drag.setMimeData(mime)
                drag.exec_(QtCore.Qt.MoveAction | QtCore.Qt.CopyAction)
            def keyPressEvent(self, e: QtGui.QKeyEvent) -> None:
                k = e.key()
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
                if e.mimeData().hasUrls():
                    e.acceptProposedAction()
                else:
                    super().dragEnterEvent(e)
            def dragMoveEvent(self, e: QtGui.QDragMoveEvent) -> None:
                if e.mimeData().hasUrls():
                    e.acceptProposedAction()
                else:
                    super().dragMoveEvent(e)
            def dropEvent(self, e: QtGui.QDropEvent) -> None:
                if not e.mimeData().hasUrls():
                    return super().dropEvent(e)
                idx = self.indexAt(e.pos())
                srcIdx = self._owner._proxy.mapToSource(idx) if idx.isValid() else QtCore.QModelIndex()
                targetDir = self._owner._current
                if idx.isValid():
                    if self._owner._model.isDir(srcIdx):
                        targetDir = self._owner._model.filePath(srcIdx)
                    else:
                        parent = self._owner._model.fileInfo(srcIdx).dir().path()
                        targetDir = parent
                urls = [u for u in e.mimeData().urls() if u.isLocalFile()]
                internal = all(self._owner._isUnderRoot(u.toLocalFile()) for u in urls)
                for u in urls:
                    sp = u.toLocalFile()
                    name = os.path.basename(sp)
                    dp = os.path.join(targetDir, name)
                    if not self._owner._isUnderRoot(dp):
                        continue
                    copyAction = (e.keyboardModifiers() & QtCore.Qt.ControlModifier) or (e.keyboardModifiers() & QtCore.Qt.MetaModifier)
                    if internal and not copyAction:
                        if os.path.abspath(sp) == os.path.abspath(dp):
                            continue
                        try:
                            common = os.path.commonpath([os.path.abspath(dp), os.path.abspath(sp)])
                        except Exception:
                            common = ""
                        if common == os.path.abspath(sp):
                            continue
                        dp2 = self._owner._uniquePath(dp, moved=True)
                        try:
                            shutil.move(sp, dp2)
                        except Exception:
                            pass
                    else:
                        dp2 = self._owner._uniquePath(dp, moved=False)
                        try:
                            if os.path.isdir(sp):
                                shutil.copytree(sp, dp2)
                            else:
                                shutil.copy2(sp, dp2)
                        except Exception:
                            pass
                self._owner._refresh()
                e.acceptProposedAction()
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
        self._view.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self._view.customContextMenuRequested.connect(self._onContextMenu)
        QtWidgets.QShortcut(QtGui.QKeySequence.Copy, self, self._onCopy)
        QtWidgets.QShortcut(QtGui.QKeySequence.Paste, self, self._onPaste)
        QtWidgets.QShortcut(QtGui.QKeySequence.Cut, self, self._onCut)
        QtWidgets.QShortcut(QtGui.QKeySequence.Delete, self, self._onDelete)
        style = QtWidgets.QApplication.style()
        upIcon = style.standardIcon(QtWidgets.QStyle.SP_ArrowUp)
        self._pathEdit = QtWidgets.QLineEdit(self)
        self._pathEdit.setReadOnly(True)
        self._pathEdit.setStyleSheet("color: white;")
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
    def _uniquePath(self, dst: str, moved: bool) -> str:
        if not os.path.exists(dst):
            return dst
        base = os.path.basename(dst)
        parent = os.path.dirname(dst)
        name, ext = os.path.splitext(base)
        tag = "_moved" if moved else "_copy"
        i = 1
        cand = os.path.join(parent, f"{name}{tag}{ext}")
        while os.path.exists(cand):
            cand = os.path.join(parent, f"{name}{tag}{i}{ext}")
            i += 1
        return cand
    def _refresh(self) -> None:
        self._view.setRootIndex(self._proxy.mapFromSource(self._model.index(self._current)))

    def _isUnderRoot(self, path: str) -> bool:
        try:
            rp = os.path.normcase(os.path.abspath(path))
            rr = os.path.normcase(os.path.abspath(self._root))
            return os.path.commonpath([rp, rr]) == rr
        except Exception:
            return False

    def _handleRemoveReadonly(self, func, p, excinfo):
        try:
            os.chmod(p, stat.S_IWRITE)
            func(p)
        except Exception:
            pass

    def _safeRemove(self, p: str) -> bool:
        try:
            if os.path.isdir(p):
                shutil.rmtree(p, onerror=self._handleRemoveReadonly)
            else:
                try:
                    os.remove(p)
                except PermissionError:
                    os.chmod(p, stat.S_IWRITE)
                    os.remove(p)
            return True
        except Exception:
            return False

    def _setCurrentPath(self, path: str) -> None:
        if not self._isUnderRoot(path):
            return
        self._current = os.path.abspath(path)
        self._pathEdit.setText(self._current)
        self._view.setRootIndex(self._proxy.mapFromSource(self._model.index(self._current)))

    def _onDoubleClicked(self, index: QtCore.QModelIndex) -> None:
        if not index.isValid():
            return
        src = self._proxy.mapToSource(index)
        path = self._model.filePath(src)
        if self._model.isDir(src):
            self._setCurrentPath(path)
        else:
            if path:
                self._openSystemFile(path)
    def _selectedSourceRows(self):
        rows = self._view.selectionModel().selectedRows()
        return [self._proxy.mapToSource(i) for i in rows]
    def _onCopy(self) -> None:
        srcRows = self._selectedSourceRows()
        paths = []
        for i in srcRows:
            p = self._model.filePath(i)
            if p and os.path.exists(p):
                paths.append(p)
        self._clipboard = paths
        self._clipboard_cut = False
    def _onCut(self) -> None:
        srcRows = self._selectedSourceRows()
        paths = []
        for i in srcRows:
            p = self._model.filePath(i)
            if p and os.path.exists(p):
                paths.append(p)
        self._clipboard = paths
        self._clipboard_cut = True
    def _onPaste(self) -> None:
        if not self._clipboard:
            return
        destDir = self._current
        rows = self._selectedSourceRows()
        if rows:
            i0 = rows[0]
            if self._model.isDir(i0):
                destDir = self._model.filePath(i0)
            else:
                destDir = self._model.fileInfo(i0).dir().path()
        if not self._isUnderRoot(destDir):
            destDir = self._current
        for sp in self._clipboard:
            name = os.path.basename(sp)
            dp = os.path.join(destDir, name)
            if self._clipboard_cut:
                dp2 = self._uniquePath(dp, moved=True)
                try:
                    shutil.move(sp, dp2)
                except Exception:
                    pass
            else:
                dp2 = self._uniquePath(dp, moved=False)
                try:
                    if os.path.isdir(sp):
                        shutil.copytree(sp, dp2)
                    else:
                        shutil.copy2(sp, dp2)
                except Exception:
                    pass
        self._refresh()
        self._clipboard = []
        self._clipboard_cut = False
    def _onDelete(self) -> None:
        rows = self._selectedSourceRows()
        if not rows:
            return
        msg = QtWidgets.QMessageBox(self)
        msg.setIcon(QtWidgets.QMessageBox.Question)
        msg.setWindowTitle(Locale.getContent("CONFIRM_DELETE"))
        msg.setText(Locale.getContent("DELETE_CONFIRMATION"))
        yes_btn = msg.addButton(Locale.getContent("CONFIRMATION_YES"), QtWidgets.QMessageBox.YesRole)
        no_btn = msg.addButton(Locale.getContent("CONFIRMATION_NO"), QtWidgets.QMessageBox.NoRole)
        msg.setDefaultButton(no_btn)
        msg.exec_()
        if msg.clickedButton() != yes_btn:
            return
        failed = False
        for i in rows:
            p = self._model.filePath(i)
            if not self._isUnderRoot(p):
                continue
            ok = self._safeRemove(p)
            if not ok:
                failed = True
        self._refresh()
        if failed:
            QtWidgets.QMessageBox.warning(self, Locale.getContent("DELETE_FAILED"), Locale.getContent("DELETE_FAILED_MESSAGE"))
    def _onContextMenu(self, pos: QtCore.QPoint) -> None:
        idx = self._view.indexAt(pos)
        if idx.isValid():
            sm = self._view.selectionModel()
            if not sm.isSelected(idx):
                sm.clearSelection()
                sm.select(idx, QtCore.QItemSelectionModel.Select | QtCore.QItemSelectionModel.Rows)
                self._view.setCurrentIndex(idx)
        menu = QtWidgets.QMenu(self)
        actCut = menu.addAction(Locale.getContent("CUT"))
        actCopy = menu.addAction(Locale.getContent("COPY"))
        actPaste = menu.addAction(Locale.getContent("PASTE"))
        menu.addSeparator()
        actDelete = menu.addAction(Locale.getContent("DELETE"))
        actPaste.setEnabled(bool(self._clipboard))
        r = menu.exec_(self._view.viewport().mapToGlobal(pos))
        if r == actCopy:
            self._onCopy()
        elif r == actCut:
            self._onCut()
        elif r == actPaste:
            self._onPaste()
        elif r == actDelete:
            self._onDelete()

    def _onUp(self) -> None:
        if os.path.normcase(os.path.abspath(self._current)) == os.path.normcase(os.path.abspath(self._root)):
            return
        parent = os.path.dirname(self._current)
        if not parent or not self._isUnderRoot(parent):
            self._setCurrentPath(self._root)
        else:
            self._setCurrentPath(parent)

    def _suffix(self, p: str) -> str:
        i = p.rfind('.')
        return p[i + 1 :].lower() if i >= 0 else ""

    def _isPreviewable(self, p: str) -> bool:
        ext = self._suffix(p)
        return ext in ("png", "jpg", "jpeg", "bmp", "gif", "webp", "mp3", "wav", "ogg", "flac", "aac", "m4a")

    def _openSystemFile(self, p: str) -> None:
        if not p:
            return
        try:
            QtGui.QDesktopServices.openUrl(QtCore.QUrl.fromLocalFile(p))
        except Exception:
            pass

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
