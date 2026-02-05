# -*- encoding: utf-8 -*-

import importlib
from PyQt5 import QtWidgets, QtGui, QtCore
import os
from enum import Enum
import EditorStatus
from Data import GameData
from Utils import Locale, System, File
from .WU_FileSelectorDialog import FileSelectorDialog
from .WU_DataclassEditDialog import DataclassEditDialog
import copy


class TilesetMode(Enum):
    PASSABLE = 0
    MATERIAL = 1


class TilesetImageView(QtWidgets.QWidget):
    dataChanged = QtCore.pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._image = None
        self._data = None
        self._mode = TilesetMode.PASSABLE
        self._key = None
        Engine = System.getModule("Engine")
        self.MaterialClass = Engine.Gameplay.Material
        self.setMouseTracking(True)

    def setData(self, data, image_path):
        self._data = data
        if image_path and os.path.exists(image_path):
            self._image = QtGui.QImage(image_path)
            self.resize(self._image.size())
        else:
            self._image = None
            self.resize(0, 0)
        self.update()

    def setMode(self, mode):
        if isinstance(mode, int):
            self._mode = TilesetMode(mode)
        else:
            self._mode = mode
        self.update()

    def setTilesetKey(self, key):
        self._key = key

    def paintEvent(self, event):
        painter = QtGui.QPainter(self)
        painter.fillRect(self.rect(), QtGui.QColor(30, 30, 30))
        if self._image:
            painter.drawImage(0, 0, self._image)

            cellSize = EditorStatus.CELLSIZE
            if cellSize <= 0:
                return
            cols = self._image.width() // cellSize
            rows = self._image.height() // cellSize

            pen = QtGui.QPen(QtGui.QColor(0, 0, 0, 100))
            painter.setPen(pen)
            for x in range(cols + 1):
                painter.drawLine(x * cellSize, 0, x * cellSize, self._image.height())
            for y in range(rows + 1):
                painter.drawLine(0, y * cellSize, self._image.width(), y * cellSize)

            if self._data:
                count = cols * rows
                font = painter.font()
                font.setPixelSize(10)
                painter.setFont(font)

                for i in range(count):
                    x = (i % cols) * cellSize
                    y = (i // cols) * cellSize
                    rect = QtCore.QRect(x, y, cellSize, cellSize)

                    if self._mode == TilesetMode.PASSABLE:
                        passable_arr = getattr(self._data, "passable", [])
                        val = False
                        if i < len(passable_arr):
                            val = bool(passable_arr[i])

                        painter.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255, 180), 2))

                        draw_rect = rect.adjusted(10, 10, -10, -10)

                        if val:
                            painter.drawEllipse(draw_rect)
                        else:
                            painter.drawLine(draw_rect.topLeft(), draw_rect.bottomRight())
                            painter.drawLine(draw_rect.topRight(), draw_rect.bottomLeft())

                    elif self._mode == TilesetMode.MATERIAL:
                        materials = getattr(self._data, "materials", [])
                        if i < len(materials):
                            mat = materials[i]
                            if mat and self.MaterialClass and isinstance(mat, self.MaterialClass):
                                is_default = True
                                matType = type(mat)
                                for attr in dir(mat):
                                    val = getattr(mat, attr)
                                    if not attr.startswith("_") and not callable(val):
                                        typeVal = getattr(matType, attr)
                                        if val != typeVal:
                                            is_default = False
                                            break
                                if not is_default:
                                    painter.setPen(QtGui.QPen(QtGui.QColor(100, 255, 100, 200), 2))
                                    painter.drawRect(rect.adjusted(4, 4, -4, -4))
                                    painter.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255)))
                                    painter.drawText(rect, QtCore.Qt.AlignCenter, "M")

    def mousePressEvent(self, e):
        if not self._image or not self._data:
            return
        if e.button() != QtCore.Qt.LeftButton:
            return
        cellSize = EditorStatus.CELLSIZE
        if cellSize <= 0:
            return
        x = int(e.pos().x())
        y = int(e.pos().y())
        cols = self._image.width() // cellSize
        rows = self._image.height() // cellSize
        gx = x // cellSize
        gy = y // cellSize
        if gx < 0 or gy < 0 or gx >= cols or gy >= rows:
            return
        GameData.recordSnapshot()
        idx = gy * cols + gx
        count = cols * rows
        if self._mode == TilesetMode.PASSABLE:
            arr = getattr(self._data, "passable", None)
            if not isinstance(arr, list):
                arr = []
                setattr(self._data, "passable", arr)
            if len(arr) < count:
                arr.extend([False] * (count - len(arr)))
            arr[idx] = not bool(arr[idx])
        elif self._mode == TilesetMode.MATERIAL:
            arr = getattr(self._data, "materials", None)
            if not isinstance(arr, list):
                arr = []
                setattr(self._data, "materials", arr)

            if self.MaterialClass:
                edit_mat = copy.deepcopy(arr[idx])
                dlg = DataclassEditDialog(self, edit_mat, Locale.getContent("EDIT_MATERIAL"))
                if dlg.exec_():
                    GameData.recordSnapshot()
                    arr[idx] = edit_mat
        self.dataChanged.emit()
        self.update()


class TilesetPanel(QtWidgets.QWidget):
    modified = QtCore.pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        System.setStyle(self, "config.qss")
        self._data = None
        self._initUI()

    def _initUI(self):
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(5)

        file_row = QtWidgets.QHBoxLayout()
        self.nameLabel = QtWidgets.QLabel(Locale.getContent("TILESET_NAME"))
        self.nameEdit = QtWidgets.QLineEdit()
        self.nameEdit.textEdited.connect(self._onNameChanged)

        self.fileLabel = QtWidgets.QLabel(Locale.getContent("FILE_NAME"))
        self.fileEdit = QtWidgets.QLineEdit()
        self.fileEdit.setReadOnly(True)
        self.fileBtn = QtWidgets.QPushButton()
        self.fileBtn.setText("...")
        self.fileBtn.clicked.connect(self._onBrowseFile)

        file_row.addWidget(self.nameLabel)
        file_row.addWidget(self.nameEdit)
        file_row.addSpacing(10)
        file_row.addWidget(self.fileLabel)
        file_row.addWidget(self.fileEdit)
        file_row.addWidget(self.fileBtn)
        layout.addLayout(file_row)

        self.modeList = QtWidgets.QListWidget()
        self.modeList.setFixedHeight(64)
        self.modeList.setFlow(QtWidgets.QListView.LeftToRight)
        self.modeList.addItem(Locale.getContent("PASSABLE"))
        self.modeList.addItem(Locale.getContent("MATERIAL"))
        self.modeList.setCurrentRow(0)
        self.modeList.currentRowChanged.connect(self._onModeChanged)
        layout.addWidget(self.modeList)

        self.scroll = QtWidgets.QScrollArea(self)
        self.scroll.setBackgroundRole(QtGui.QPalette.Dark)
        self.scroll.setWidgetResizable(False)
        self.imageView = TilesetImageView()
        self.scroll.setWidget(self.imageView)
        self.imageView.dataChanged.connect(self.modified)
        layout.addWidget(self.scroll)

    def setTilesetData(self, tilesetData):
        self._data = tilesetData
        self._key = None
        for k, v in GameData.tilesetData.items():
            if v is tilesetData or (tilesetData and v.fileName == tilesetData.fileName):
                self._key = k
                break
        if not tilesetData:
            self.nameEdit.clear()
            self.fileEdit.clear()
            self.imageView.setData(None, None)
            return
        p = None
        if tilesetData.fileName:
            p = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Tilesets", tilesetData.fileName)
            if not os.path.exists(p):
                tilesetData.fileName = ""
                p = None

        self.nameEdit.setText(getattr(tilesetData, "name", ""))
        self.fileEdit.setText(tilesetData.fileName)
        self.imageView.setData(tilesetData, p)
        self.imageView.setTilesetKey(self._key)
        self.imageView.setMode(self.modeList.currentRow())
        self.modified.emit()

    def _onNameChanged(self, text):
        if self._data:
            GameData.recordSnapshot()
            self._data.name = text
            self.modified.emit()

    def _onModeChanged(self, row):
        self.imageView.setMode(row)

    def _onBrowseFile(self):
        if not self._data:
            return
        root = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Tilesets")
        dlg = FileSelectorDialog(self, root, "Images (*.png *.jpg *.bmp *.jpeg)")
        fp = dlg.execSelect()
        if fp:
            GameData.recordSnapshot()
            filename = os.path.basename(fp)
            self._data.fileName = filename
            cellSize = EditorStatus.CELLSIZE
            new_count = 0
            if isinstance(cellSize, int) and cellSize > 0:
                img = QtGui.QImage(fp)
                if not img.isNull():
                    cols = img.width() // cellSize
                    rows = img.height() // cellSize
                    new_count = max(0, cols * rows)
            arr_p = getattr(self._data, "passable", [])
            arr_m = getattr(self._data, "materials", [])
            if not isinstance(arr_p, list):
                arr_p = []
                setattr(self._data, "passable", arr_p)
            if not isinstance(arr_m, list):
                arr_m = []
                setattr(self._data, "materials", arr_m)
            if len(arr_p) < new_count:
                arr_p.extend([True] * (new_count - len(arr_p)))
            elif len(arr_p) > new_count:
                del arr_p[new_count:]
            if len(arr_m) < new_count:
                if self.MaterialClass:
                    arr_m.extend([self.MaterialClass() for _ in range(new_count - len(arr_m))])
                else:
                    arr_m.extend([None] * (new_count - len(arr_m)))
            elif len(arr_m) > new_count:
                del arr_m[new_count:]
            if self._key:
                File.mainWindow.editorPanel._renderFromMapData()
                File.mainWindow.editorPanel.update()
                ts = File.mainWindow.tileSelect
                ts.initTilesets()
            self.modified.emit()
            self.setTilesetData(self._data)
