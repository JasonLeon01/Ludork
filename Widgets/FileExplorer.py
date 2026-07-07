# -*- encoding: utf-8 -*-

import copy
import colorsys
import os
import shutil
import subprocess
import sys
from typing import Any, Callable, Optional, cast
from PyQt5 import QtCore, QtGui, QtWidgets
from EditorGlobal import EditorStatus, GameData
from Utils import Panel, File, System
from Utils.DataConfig import DATA_FILE_SUFFIXES, DATA_FORMAT_DAT, DATA_FORMAT_EXTENSIONS, DATA_FORMAT_JSON
from .FilePreview import FilePreview


_THUMB_SIZE = 80

_EXTERNAL_EDITOR_COMMANDS = {
    "vscode": ("code.cmd", "code.exe", "code"),
    "cursor": ("cursor.cmd", "cursor.exe", "cursor"),
}


def _iconPath(name: str) -> str:
    return os.path.join(File.GetRootPath(), "Resource", "icons", f"{name}.svg")


def _resourceIcon(name: str) -> QtGui.QIcon:
    return QtGui.QIcon(_iconPath(name))


class _ThumbnailIconProvider(QtWidgets.QFileIconProvider):
    _IMAGE_EXTS = {"png", "jpg", "jpeg", "bmp", "gif", "webp"}
    _DATA_EXTS = set(DATA_FILE_SUFFIXES)

    def __init__(self, thumbSize: int = _THUMB_SIZE):
        super().__init__()
        self._thumbSize = thumbSize
        self._cache: dict[str, QtGui.QIcon] = {}
        self._blueprintCache: dict[str, tuple[tuple[Any, ...], QtGui.QIcon]] = {}

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
            if ext in self._DATA_EXTS:
                icon = self._blueprintIcon(info.absoluteFilePath())
                if icon is not None:
                    return icon
        return super().icon(info)

    def clearCache(self) -> None:
        self._cache.clear()
        self._blueprintCache.clear()

    def _blueprintKeyForPath(self, path: str) -> Optional[str]:
        blueprintsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Blueprints")
        if not os.path.isfile(path) or not self._isPathInside(path, blueprintsRoot):
            return None
        relPath = os.path.relpath(path, blueprintsRoot)
        return os.path.splitext(relPath)[0].replace("\\", "/")

    def _blueprintIcon(self, path: str) -> Optional[QtGui.QIcon]:
        key = self._blueprintKeyForPath(path)
        if not key:
            return None
        data = self._blueprintData(key, path)
        if not isinstance(data, dict):
            return None
        token = self._blueprintCacheToken(path, key, data)
        cached = self._blueprintCache.get(path)
        if cached is not None and cached[0] == token:
            return cached[1]
        pixmap = self._makeBlueprintPreview("Data.Blueprints." + key.replace("/", "."), data)
        if pixmap.isNull():
            return None
        icon = QtGui.QIcon(pixmap.scaled(
            self._thumbSize,
            self._thumbSize,
            QtCore.Qt.KeepAspectRatio,
            QtCore.Qt.SmoothTransformation,
        ))
        self._blueprintCache[path] = (token, icon)
        return icon

    def _blueprintData(self, key: str, path: str) -> Optional[dict[str, Any]]:
        data = getattr(GameData, "blueprintsData", {}).get(key)
        if isinstance(data, dict):
            return data
        try:
            ext = os.path.splitext(path)[1].lower()
            loaded = File.GetJSONData(path) if ext == DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON] else File.LoadData(path)
            if not isinstance(loaded, dict):
                return None
            fileType = loaded.get("type")
            return loaded if fileType in (None, "blueprint") else None
        except Exception:
            return None

    def _blueprintCacheToken(self, path: str, key: str, data: dict[str, Any]) -> tuple[Any, ...]:
        try:
            mtime = os.stat(path).st_mtime_ns
        except OSError:
            mtime = 0
        return (
            key,
            mtime,
            self._getBlueprintAttrFromData(data, "texturePath", ""),
            repr(self._getBlueprintAttrFromData(data, "defaultRect", None)),
            repr(self._getBlueprintAttrFromData(data, "defaultOrigin", (0.0, 0.0))),
            repr(self._getBlueprintAttrFromData(data, "defaultScale", (1.0, 1.0))),
            repr(self._getBlueprintAttrFromData(data, "hue", 0.0)),
        )

    def _getBlueprintAttrFromData(self, data: dict[str, Any], attrName: str, default: Any) -> Any:
        attrs = data.get("attrs")
        if isinstance(attrs, dict) and attrName in attrs:
            return attrs.get(attrName, default)
        parentClass = data.get("parent")
        if isinstance(parentClass, str) and parentClass.strip():
            try:
                found, value = GameData._GetBlueprintDefaultAttr(parentClass, attrName, set())
                if found:
                    return value
            except Exception:
                return default
        return default

    def _makeBlueprintPreview(self, bpRel: str, data: dict[str, Any]) -> QtGui.QPixmap:
        tileSize = max(1, int(getattr(EditorStatus, "CELLSIZE", 32)))
        texturePath = self._getBlueprintAttrFromData(data, "texturePath", "")
        image = self._resolveTextureImage(texturePath)
        rect = self._toRectTuple(self._getBlueprintAttrFromData(data, "defaultRect", None))
        origin = self._toVec2f(self._getBlueprintAttrFromData(data, "defaultOrigin", (0.0, 0.0)), 0.0, 0.0)
        scale = self._toVec2f(self._getBlueprintAttrFromData(data, "defaultScale", (1.0, 1.0)), 1.0, 1.0)
        hue = self._normaliseActorHue(self._getBlueprintAttrFromData(data, "hue", 0.0))
        if rect is None:
            if image is not None:
                rect = self._defaultImageRect(bpRel, image, tileSize)
            else:
                return self._makeBlueprintFallbackIcon()
        sx, sy, w, h = rect
        dw = max(1, int(w * scale[0]))
        dh = max(1, int(h * scale[1]))
        pixmap = QtGui.QPixmap(dw, dh)
        pixmap.fill(QtCore.Qt.transparent)
        painter = QtGui.QPainter(pixmap)
        painter.setRenderHint(QtGui.QPainter.SmoothPixmapTransform, True)
        if image is not None:
            src = QtCore.QRect(sx, sy, w, h)
            dst = QtCore.QRect(int(-origin[0] * scale[0]), int(-origin[1] * scale[1]), dw, dh)
            if self._isNeutralActorHue(hue):
                painter.drawImage(dst, image, src)
            else:
                drawImage = self._applyActorHueToImage(image.copy(src), hue)
                painter.drawImage(dst, drawImage, QtCore.QRect(0, 0, drawImage.width(), drawImage.height()))
        else:
            painter.end()
            return self._makeBlueprintFallbackIcon()
        painter.end()
        return pixmap

    def _makeBlueprintFallbackIcon(self) -> QtGui.QPixmap:
        size = max(24, self._thumbSize)
        pixmap = QtGui.QPixmap(size, size)
        pixmap.fill(QtCore.Qt.transparent)
        painter = QtGui.QPainter(pixmap)
        painter.setRenderHint(QtGui.QPainter.Antialiasing, True)
        bg = QtCore.QRectF(8, 8, size - 16, size - 16)
        painter.setPen(QtGui.QPen(QtGui.QColor(112, 160, 255, 210), 2))
        painter.setBrush(QtGui.QColor(34, 54, 96, 220))
        painter.drawRoundedRect(bg, 6, 6)
        painter.setPen(QtGui.QPen(QtGui.QColor(210, 228, 255, 230), 2))
        pad = size / 4
        left = QtCore.QRectF(pad - 4, pad, 12, 10)
        right = QtCore.QRectF(size - pad - 8, size / 2, 12, 10)
        bottom = QtCore.QRectF(pad, size - pad - 6, 12, 10)
        painter.drawRect(left)
        painter.drawRect(right)
        painter.drawRect(bottom)
        painter.drawLine(left.center(), right.center())
        painter.drawLine(left.center(), bottom.center())
        painter.end()
        return pixmap

    def _defaultImageRect(self, bpRel: str, image: QtGui.QImage, tileSize: int) -> tuple[int, int, int, int]:
        if self._isCharacterActor(bpRel, image):
            return (0, 0, max(1, image.width() // 4), max(1, image.height() // 4))
        return (0, 0, min(tileSize, image.width()), min(tileSize, image.height()))

    def _isCharacterActor(self, bpRel: str, image: QtGui.QImage) -> bool:
        if image.isNull() or image.width() < 4 or image.height() < 4:
            return False
        try:
            clsObj = GameData.classDict.get(bpRel, EditorStatus.PROJ_PATH)
            if not isinstance(clsObj, type):
                return False
            Engine = System.GetModule("Engine")
            return issubclass(clsObj, Engine.Gameplay.Actors.Character)
        except Exception:
            return False

    def _resolveTextureImage(self, texturePath: Any) -> Optional[QtGui.QImage]:
        path = self._resolveTexturePath(texturePath)
        if not path:
            return None
        image = QtGui.QImage(path)
        return image if not image.isNull() else None

    def _normaliseActorHue(self, hue: Any) -> float:
        try:
            return float(hue) % 360.0
        except (TypeError, ValueError):
            return 0.0

    def _isNeutralActorHue(self, hue: float) -> bool:
        hue = self._normaliseActorHue(hue)
        return hue <= 0.0001 or abs(hue - 360.0) <= 0.0001

    def _applyActorHueToImage(self, image: QtGui.QImage, hue: float) -> QtGui.QImage:
        if image.isNull() or self._isNeutralActorHue(hue):
            return image
        result = image.convertToFormat(QtGui.QImage.Format_ARGB32)
        hueOffset = self._normaliseActorHue(hue) / 360.0
        for y in range(result.height()):
            for x in range(result.width()):
                color = result.pixelColor(x, y)
                if color.alpha() == 0:
                    continue
                h, s, v = colorsys.rgb_to_hsv(color.redF(), color.greenF(), color.blueF())
                r, g, b = colorsys.hsv_to_rgb((h + hueOffset) % 1.0, s, v)
                color.setRgbF(r, g, b, color.alphaF())
                result.setPixelColor(x, y, color)
        return result

    def _resolveTexturePath(self, texturePath: Any) -> str:
        if isinstance(texturePath, str) and texturePath.strip():
            path = texturePath.strip()
            if os.path.isabs(path):
                return path
            if path.startswith("Assets/") or path.startswith("Assets\\"):
                return os.path.join(EditorStatus.PROJ_PATH, path)
            return os.path.join(EditorStatus.PROJ_PATH, "Assets", "Characters", path)
        return ""

    def _toVec2f(self, value: Any, defaultX: float, defaultY: float) -> tuple[float, float]:
        if isinstance(value, (list, tuple)) and len(value) >= 2:
            try:
                return (float(value[0]), float(value[1]))
            except (TypeError, ValueError):
                return (defaultX, defaultY)
        for attrX, attrY in (("x", "y"), ("X", "Y")):
            try:
                return (float(getattr(value, attrX)), float(getattr(value, attrY)))
            except (AttributeError, TypeError, ValueError):
                continue
        return (defaultX, defaultY)

    def _toRectTuple(self, value: Any) -> Optional[tuple[int, int, int, int]]:
        normalised = GameData._NormaliseBlueprintValue(copy.deepcopy(value))
        if isinstance(normalised, dict):
            pos = normalised.get("position") or normalised.get("pos")
            size = normalised.get("size")
            if isinstance(pos, (list, tuple)) and isinstance(size, (list, tuple)) and len(pos) >= 2 and len(size) >= 2:
                try:
                    return (int(pos[0]), int(pos[1]), max(1, int(size[0])), max(1, int(size[1])))
                except (TypeError, ValueError):
                    return None
        if isinstance(normalised, (list, tuple)) and len(normalised) >= 2:
            pos = normalised[0]
            size = normalised[1]
            if isinstance(pos, (list, tuple)) and isinstance(size, (list, tuple)) and len(pos) >= 2 and len(size) >= 2:
                try:
                    return (int(pos[0]), int(pos[1]), max(1, int(size[0])), max(1, int(size[1])))
                except (TypeError, ValueError):
                    return None
        return None

    def _isPathInside(self, path: str, root: str) -> bool:
        try:
            p = os.path.normcase(os.path.abspath(path))
            r = os.path.normcase(os.path.abspath(root))
            return os.path.commonpath([p, r]) == r
        except Exception:
            return False


class FileExplorer(QtWidgets.QWidget):
    PATH_CHANGED = QtCore.pyqtSignal(str)
    FILE_CLICKED = QtCore.pyqtSignal(str)
    DATA_FILE_CHANGED = QtCore.pyqtSignal()
    DATA_PATH_RENAMED = QtCore.pyqtSignal(str, str)

    def __init__(self, root_path: str, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self._root = os.path.abspath(root_path)
        self._current = self._root
        self._interactive = True
        self._referenceDialogs: list[QtWidgets.QDialog] = []

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

        class _View(QtWidgets.QListView):
            def __init__(self, owner: "FileExplorer"):
                super().__init__(owner)
                self._owner = owner
                self.setViewMode(QtWidgets.QListView.IconMode)
                self.setIconSize(QtCore.QSize(_THUMB_SIZE, _THUMB_SIZE))
                self.setGridSize(QtCore.QSize(_THUMB_SIZE * 2, _THUMB_SIZE + 30))
                self.setResizeMode(QtWidgets.QListView.Adjust)
                self.setWordWrap(True)
                self.setSpacing(4)
                self.setUniformItemSizes(True)
                self.setWrapping(True)
                self.setFlow(QtWidgets.QListView.LeftToRight)
                self.setMovement(QtWidgets.QListView.Static)
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

        self._iconProvider = _ThumbnailIconProvider()
        self._model = QtWidgets.QFileSystemModel(self)
        self._model.setFilter(QtCore.QDir.AllEntries | QtCore.QDir.NoDotAndDotDot)
        self._model.setRootPath(self._root)
        self._model.setIconProvider(self._iconProvider)
        self._proxy = _Proxy(self)
        self._proxy.setSourceModel(self._model)
        self._proxy.setDynamicSortFilter(True)
        self._proxy.sort(0, QtCore.Qt.AscendingOrder)
        self._view = _View(self)
        self._view.setModel(self._proxy)
        self._view.setRootIndex(self._proxy.mapFromSource(self._model.index(self._root)))
        self._view.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self._view.setSelectionMode(QtWidgets.QAbstractItemView.ExtendedSelection)
        self._view.doubleClicked.connect(self._onDoubleClicked)
        self._view.clicked.connect(self._onClicked)
        self._view.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self._view.customContextMenuRequested.connect(self._onContextMenu)

        style = cast(QtWidgets.QStyle, QtWidgets.QApplication.style())
        upIcon = style.standardIcon(QtWidgets.QStyle.SP_ArrowUp)
        self._listViewIcon = style.standardIcon(QtWidgets.QStyle.SP_FileDialogListView)
        self._iconViewIcon = style.standardIcon(QtWidgets.QStyle.SP_FileDialogContentsView)
        vsCodeIcon = _resourceIcon("vscode")
        cursorIcon = _resourceIcon("cursor")
        self._pathEdit = QtWidgets.QLineEdit(self)
        self._pathEdit.setReadOnly(True)
        self._pathEdit.setStyleSheet("")
        self._pathEdit.setText(self._current)
        self._viewModeButton = QtWidgets.QToolButton(self)
        self._viewModeButton.setCheckable(True)
        self._viewModeButton.setToolButtonStyle(QtCore.Qt.ToolButtonIconOnly)
        self._viewModeButton.setIcon(self._listViewIcon)
        self._viewModeButton.setToolTip(ELOC("FILE_EXPLORER_LIST_VIEW"))
        self._viewModeButton.toggled.connect(self._onViewModeToggled)
        self._upButton = QtWidgets.QToolButton(self)
        self._upButton.setIcon(upIcon)
        self._upButton.clicked.connect(self._onUp)
        self._externalEditorButton = QtWidgets.QToolButton(self)
        self._externalEditorButton.setPopupMode(QtWidgets.QToolButton.MenuButtonPopup)
        self._externalEditorButton.setToolButtonStyle(QtCore.Qt.ToolButtonIconOnly)
        self._externalEditorMenu = QtWidgets.QMenu(self._externalEditorButton)
        self._openVSCodeAction = QtWidgets.QAction(vsCodeIcon, ELOC("EXTERNAL_EDITOR_VSCODE"), self)
        self._openVSCodeAction.setToolTip(ELOC("OPEN_PROJECT_WITH_VSCODE"))
        self._openVSCodeAction.triggered.connect(lambda _checked=False: self._openProjectInExternalEditor("vscode"))
        self._openCursorAction = QtWidgets.QAction(cursorIcon, ELOC("EXTERNAL_EDITOR_CURSOR"), self)
        self._openCursorAction.setToolTip(ELOC("OPEN_PROJECT_WITH_CURSOR"))
        self._openCursorAction.triggered.connect(lambda _checked=False: self._openProjectInExternalEditor("cursor"))
        self._externalEditorMenu.addAction(self._openVSCodeAction)
        self._externalEditorMenu.addAction(self._openCursorAction)
        self._externalEditorButton.setDefaultAction(self._openVSCodeAction)
        self._externalEditorButton.setMenu(self._externalEditorMenu)
        topBar = QtWidgets.QWidget(self)
        topLayout = QtWidgets.QHBoxLayout(topBar)
        topLayout.setContentsMargins(0, 0, 0, 0)
        topLayout.setSpacing(4)
        topLayout.addWidget(self._pathEdit, 1)
        topLayout.addWidget(self._viewModeButton, 0, alignment=QtCore.Qt.AlignRight)
        topLayout.addWidget(self._upButton, 0, alignment=QtCore.Qt.AlignRight)
        topLayout.addWidget(self._externalEditorButton, 0, alignment=QtCore.Qt.AlignRight)
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
        self._viewModeButton.setEnabled(self._interactive)
        self._upButton.setEnabled(self._interactive)
        self._externalEditorButton.setEnabled(self._interactive)
        Panel.ApplyDisabledOpacity(self._viewModeButton)
        Panel.ApplyDisabledOpacity(self._upButton)
        Panel.ApplyDisabledOpacity(self._externalEditorButton)

    def changeEvent(self, e: QtCore.QEvent) -> None:
        if e.type() == QtCore.QEvent.EnabledChange:
            Panel.ApplyDisabledOpacity(self)
        super().changeEvent(e)

    def _refresh(self) -> None:
        self._iconProvider.clearCache()
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
        self._refresh()
        self.PATH_CHANGED.emit(self._current)

    def setCurrentPath(self, path: str) -> None:
        self._setCurrentPath(path)

    def locatePath(self, path: str) -> bool:
        if not isinstance(path, str) or not path.strip():
            return False
        targetPath = os.path.abspath(path.strip())
        if not self._isUnderRoot(targetPath) or not os.path.exists(targetPath):
            return False
        parentPath = os.path.dirname(targetPath)
        if not parentPath or not self._isUnderRoot(parentPath):
            return False
        self._setCurrentPath(parentPath)
        sourceIndex = self._model.index(targetPath)
        if not sourceIndex.isValid():
            return False
        proxyIndex = self._proxy.mapFromSource(sourceIndex)
        if not proxyIndex.isValid():
            return False
        selectionModel = self._view.selectionModel()
        if selectionModel is None:
            return False
        selectionModel.select(
            proxyIndex,
            QtCore.QItemSelectionModel.ClearAndSelect | QtCore.QItemSelectionModel.Rows,
        )
        self._view.setCurrentIndex(proxyIndex)
        self._view.scrollTo(proxyIndex, QtWidgets.QAbstractItemView.PositionAtCenter)
        self._view.setFocus()
        return True

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
            if ext == DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]:
                self._handleDataFile(path, File.LoadData)
            else:
                if ext == DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]:
                    if self._handleDataFile(path, File.GetJSONData):
                        return
                from Utils import PluginSystem

                pluginData = PluginSystem.LoadPluginDataFile(path)
                if pluginData is not None and PluginSystem.HandleDataFile(self, path, pluginData):
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
        if not isinstance(data, dict):
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("INVALID_DATA_FILE"),
                ELOC("INVALID_DATA_FILE_MESSAGE"),
            )
            return False
        from Utils import PluginSystem

        if PluginSystem.HandleDataFile(self, path, data):
            return True
        generalRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "General")
        if self._isPathInside(path, generalRoot):
            relPath = os.path.relpath(path, generalRoot)
            key = os.path.splitext(relPath)[0].replace("\\", "/")
            if key in GameData.generalData:
                File.mainWindow._onGeneralDataEditor()
                editor = File.mainWindow.generalDataEditor
                if editor is None:
                    return False
                editor.selectDataType(key)
            else:
                QtWidgets.QMessageBox.warning(
                    self,
                    ELOC("INVALID_DATA_FILE"),
                    ELOC("INVALID_DATA_FILE_MESSAGE"),
                )
            return True
        if "type" not in data:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("INVALID_DATA_FILE"),
                ELOC("INVALID_DATA_FILE_MESSAGE"),
            )
            return False
        dataType = data.get("type")
        baseName = os.path.splitext(os.path.basename(path))[0]
        if dataType == "map":
            mapsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
            relPath = os.path.relpath(path, mapsRoot)
            key = os.path.splitext(relPath)[0].replace("\\", "/")
            if key in GameData.mapData:
                item = File.mainWindow._findItemByKey(key)
                if item:
                    File.mainWindow.leftList.setCurrentItem(item)
                    File.mainWindow._onLeftItemClicked(item)
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
        elif dataType == "tileset":
            tilesetsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Tilesets")
            relPath = os.path.relpath(path, tilesetsRoot)
            key = os.path.splitext(relPath)[0].replace("\\", "/")
            if key in GameData.tilesetData:
                File.mainWindow._onDatabaseTilesetsData()
                editor = File.mainWindow._tilesetEditor
                if editor is None:
                    return False
                editor.selectTileset(key)
            else:
                QtWidgets.QMessageBox.warning(
                    self,
                    ELOC("INVALID_DATA_FILE"),
                    ELOC("INVALID_DATA_FILE_MESSAGE"),
                )
        elif dataType == "autoTile":
            autoTilesRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "AutoTiles")
            relPath = os.path.relpath(path, autoTilesRoot)
            key = os.path.splitext(relPath)[0].replace("\\", "/")
            if key in GameData.autoTileData:
                File.mainWindow._onDatabaseTilesetsData()
                editor = File.mainWindow._tilesetEditor
                if editor is None:
                    return False
                editor.selectAutoTile(key)
            else:
                QtWidgets.QMessageBox.warning(
                    self,
                    ELOC("INVALID_DATA_FILE"),
                    ELOC("INVALID_DATA_FILE_MESSAGE"),
                )
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
        if not rows:
            rows = [i for i in sm.selectedIndexes() if i.column() == 0]
        result = []
        seen = set()
        for idx in rows:
            if not idx.isValid():
                continue
            if idx.column() != 0:
                idx = idx.sibling(idx.row(), 0)
            src = self._proxy.mapToSource(idx)
            if not src.isValid():
                continue
            path = self._model.filePath(src)
            key = os.path.normcase(os.path.abspath(path)) if path else (src.row(), src.column())
            if key in seen:
                continue
            seen.add(key)
            result.append(src)
        return result

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
        actReferenceTree = None
        actDeriveBlueprint = None
        actCopyBlueprintClassName = None
        actConvertDataFormat = None
        blueprintClassName = ""
        if selectedPath:
            if not isDir:
                blueprintClassName = self._blueprintClassNameForPath(selectedPath) or ""
            if blueprintClassName:
                actCopyBlueprintClassName = menu.addAction(ELOC("COPY_BLUEPRINT_CLASS_NAME"))
                actDeriveBlueprint = menu.addAction(ELOC("DERIVE_FROM_THIS_BLUEPRINT"))
            if not isDir and GameData.GetReferenceNodeForPath(selectedPath):
                actReferenceTree = menu.addAction(ELOC("SHOW_REFERENCE_TREE"))
            if not isDir:
                currentFormat = GameData.GetConvertibleDataFormatForPath(selectedPath)
                if currentFormat == "dat":
                    actConvertDataFormat = menu.addAction(ELOC("CONVERT_TO_JSON"))
                elif currentFormat == "json":
                    actConvertDataFormat = menu.addAction(ELOC("CONVERT_TO_DAT"))
            if not isDir:
                actDuplicate = menu.addAction(ELOC("DUPLICATE_FILE"))
            actRename = menu.addAction(ELOC("RENAME_FILE"))
            actDelete = menu.addAction(ELOC("DELETE"))
        viewport = self._view.viewport()
        if viewport is None:
            return
        from Utils import PluginSystem

        PluginSystem.AddRightClickActions(
            menu,
            self,
            "fileExplorer",
            "hit" if selectedPath else "empty",
            selectedPath or None,
        )
        r = menu.exec_(viewport.mapToGlobal(pos))
        if r == actNewFolder:
            self._createFolder(self._targetDirForNewFolder(selectedPath, isDir))
        elif actCopyBlueprintClassName and r == actCopyBlueprintClassName:
            self._copyBlueprintClassName(blueprintClassName)
        elif actDeriveBlueprint and r == actDeriveBlueprint:
            self._deriveBlueprintFrom(selectedPath)
        elif actReferenceTree and r == actReferenceTree:
            self._showReferenceTree(selectedPath)
        elif actConvertDataFormat and r == actConvertDataFormat:
            self._convertDataFileFormat(selectedPath)
        elif r == actOpen:
            if selectedPath:
                self._openSystemFile(selectedPath)
        elif actDuplicate and r == actDuplicate:
            self._duplicateItem(selectedPath)
        elif actRename and r == actRename:
            self._renameItem(selectedPath)
        elif actDelete and r == actDelete:
            self._deleteSelectedItems()

    def _convertDataFileFormat(self, path: str) -> None:
        if not path or not os.path.isfile(path):
            return
        try:
            newPath = GameData.ConvertDataFormatForPath(path)
            self._refresh()
            self.locatePath(newPath)
            mainWindow = File.mainWindow
            mainWindow.refreshLeftList()
            mainWindow.actorQueuePanel.purgeStale()
            mainWindow._refreshInfo()
        except Exception as e:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("ERROR"),
                ELOC("CONVERT_DATA_FORMAT_FAILED") + "\n" + str(e),
            )

    def _targetDirForNewFolder(self, selectedPath: str, isDir: bool) -> str:
        if selectedPath and isDir:
            return selectedPath
        if selectedPath and not isDir:
            return os.path.dirname(selectedPath)
        return self._current

    def _blueprintKeyForPath(self, path: str) -> Optional[str]:
        if not path or not os.path.isfile(path):
            return None
        blueprintsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Blueprints")
        if not self._isPathInside(path, blueprintsRoot):
            return None
        relPath = os.path.relpath(path, blueprintsRoot)
        key = os.path.splitext(relPath)[0].replace("\\", "/")
        if key in GameData.blueprintsData:
            return key
        return None

    def _blueprintClassNameForPath(self, path: str) -> Optional[str]:
        key = self._blueprintKeyForPath(path)
        if not key:
            return None
        return "Data.Blueprints." + key.replace("/", ".")

    def _copyBlueprintClassName(self, className: str) -> None:
        if not className:
            return
        QtWidgets.QApplication.clipboard().setText(className)

    def _deriveBlueprintFrom(self, path: str) -> None:
        className = self._blueprintClassNameForPath(path)
        if not className:
            return
        File.mainWindow._onNewBlueprint(parentClass=className)

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
            if self._dataPathsChanged([path, newPath]):
                self.DATA_PATH_RENAMED.emit(path, newPath)
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
            GameData.RemoveDataPaths(deletedPaths)
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

    def _onViewModeToggled(self, listMode: bool) -> None:
        if listMode:
            appStyle = cast(QtWidgets.QStyle, QtWidgets.QApplication.style())
            listIconSize = appStyle.pixelMetric(QtWidgets.QStyle.PM_ListViewIconSize)
            self._view.setViewMode(QtWidgets.QListView.ListMode)
            self._view.setIconSize(QtCore.QSize(listIconSize, listIconSize))
            self._view.setGridSize(QtCore.QSize())
            self._view.setWordWrap(False)
            self._view.setWrapping(False)
            self._view.setFlow(QtWidgets.QListView.TopToBottom)
            self._viewModeButton.setIcon(self._iconViewIcon)
            self._viewModeButton.setToolTip(ELOC("FILE_EXPLORER_ICON_VIEW"))
        else:
            self._view.setViewMode(QtWidgets.QListView.IconMode)
            self._view.setIconSize(QtCore.QSize(_THUMB_SIZE, _THUMB_SIZE))
            self._view.setGridSize(QtCore.QSize(_THUMB_SIZE * 2, _THUMB_SIZE + 30))
            self._view.setWordWrap(True)
            self._view.setWrapping(True)
            self._view.setFlow(QtWidgets.QListView.LeftToRight)
            self._viewModeButton.setIcon(self._listViewIcon)
            self._viewModeButton.setToolTip(ELOC("FILE_EXPLORER_LIST_VIEW"))

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

    def _externalEditorLabel(self, editorId: str) -> str:
        if editorId == "cursor":
            return ELOC("EXTERNAL_EDITOR_CURSOR")
        return ELOC("EXTERNAL_EDITOR_VSCODE")

    def _externalEditorWindowsCandidates(self, editorId: str) -> list[str]:
        localAppData = os.environ.get("LOCALAPPDATA", "")
        programFiles = os.environ.get("ProgramFiles", "")
        programFilesX86 = os.environ.get("ProgramFiles(x86)", "")
        if editorId == "cursor":
            candidates = [
                (localAppData, "Programs", "Cursor", "Cursor.exe"),
                (programFiles, "Cursor", "Cursor.exe"),
                (programFilesX86, "Cursor", "Cursor.exe"),
            ]
        else:
            candidates = [
                (localAppData, "Programs", "Microsoft VS Code", "Code.exe"),
                (programFiles, "Microsoft VS Code", "Code.exe"),
                (programFilesX86, "Microsoft VS Code", "Code.exe"),
            ]
        return [os.path.join(*parts) for parts in candidates if parts[0]]

    def _externalEditorCommandLine(self, editorId: str, projectPath: str) -> Optional[list[str]]:
        if sys.platform == "win32":
            for candidate in self._externalEditorWindowsCandidates(editorId):
                if candidate and os.path.isfile(candidate):
                    return [candidate, projectPath]

        for command in _EXTERNAL_EDITOR_COMMANDS.get(editorId, ()):
            executable = shutil.which(command)
            if not executable:
                continue
            if sys.platform == "win32":
                ext = os.path.splitext(executable)[1].lower()
                if ext in (".cmd", ".bat") or ext != ".exe":
                    return [os.environ.get("COMSPEC", "cmd.exe"), "/c", executable, projectPath]
            return [executable, projectPath]
        return None

    def _openProjectInExternalEditor(self, editorId: str) -> None:
        projectPath = os.path.abspath(self._root)
        if not projectPath or not os.path.isdir(projectPath):
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("PACK_NO_PROJECT"))
            return
        appName = self._externalEditorLabel(editorId)
        commandLine = self._externalEditorCommandLine(editorId, projectPath)
        if not commandLine:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("ERROR"),
                ELOC("EXTERNAL_EDITOR_NOT_FOUND").format(app=appName),
            )
            return
        try:
            popenArgs = {
                "cwd": projectPath,
                "stdin": subprocess.DEVNULL,
                "stdout": subprocess.DEVNULL,
                "stderr": subprocess.DEVNULL,
            }
            if sys.platform == "win32":
                popenArgs["creationflags"] = subprocess.CREATE_NO_WINDOW
            subprocess.Popen(
                commandLine,
                **popenArgs,
            )
        except Exception as e:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("ERROR"),
                ELOC("OPEN_EXTERNAL_EDITOR_FAILED").format(app=appName) + "\n" + str(e),
            )

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

    def _showReferenceTree(self, p: str) -> None:
        from .ReferenceTreeDialog import ReferenceTreeDialog

        nodeId = GameData.GetReferenceNodeForPath(p)
        if not nodeId:
            return
        dlg = ReferenceTreeDialog(self, nodeId, self)
        dlg.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        dlg.setModal(False)
        self._referenceDialogs.append(dlg)

        def _removeDialog(_obj=None, dialog=dlg) -> None:
            if dialog in self._referenceDialogs:
                self._referenceDialogs.remove(dialog)

        dlg.destroyed.connect(_removeDialog)
        dlg.show()

    def setRootPath(self, root_path: str) -> None:
        self._root = os.path.abspath(root_path)
        self._model.setRootPath(self._root)
        self._setCurrentPath(self._root)
