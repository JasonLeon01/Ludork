# -*- encoding: utf-8 -*-

import importlib
import copy
import os
from enum import Enum
from typing import Any

from PyQt5 import QtWidgets, QtGui, QtCore

from EditorGlobal import EditorStatus, GameData
from Utils import System, File
from .FileSelectorDialog import FileSelectorDialog
from .DataclassEditDialog import DataclassEditDialog


def _getListField(data: Any, fieldName: str) -> list:
    value = getattr(data, fieldName, [])
    if isinstance(value, list):
        return list(value)
    try:
        return list(value)
    except TypeError:
        return []


def _setListField(data: Any, fieldName: str, value: list) -> None:
    setattr(data, fieldName, value)


def _resizeList(value: list, size: int, defaultValue: Any) -> None:
    if len(value) < size:
        value.extend([copy.deepcopy(defaultValue) for _ in range(size - len(value))])
    elif len(value) > size:
        del value[size:]


class TilesetMode(Enum):
    PASSABLE = 0
    MATERIAL = 1
    DIR4 = 2


class TilesetImageView(QtWidgets.QWidget):
    DATA_CHANGED = QtCore.pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._image = None
        self._data = None
        self._mode = TilesetMode.PASSABLE
        self._key = None
        Engine = System.GetModule("Engine")
        self.MaterialClass = Engine.Material
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
                    elif self._mode == TilesetMode.DIR4:
                        dir4Arr = getattr(self._data, "dir4", [])
                        dir4Val = (True, True, True, True)
                        if i < len(dir4Arr):
                            v = dir4Arr[i]
                            if isinstance(v, (list, tuple)) and len(v) == 4:
                                dir4Val = (bool(v[0]), bool(v[1]), bool(v[2]), bool(v[3]))

                        centerX = x + cellSize // 2
                        centerY = y + cellSize // 2
                        arrowLen = max(8, cellSize // 3)
                        arrowWidth = max(6, cellSize // 4)
                        headLen = max(5, arrowLen // 2)

                        painter.setPen(QtGui.QPen(QtGui.QColor(0, 0, 0, 140), 1))

                        def drawArrow(direction: str, enabled: bool) -> None:
                            color = QtGui.QColor(100, 255, 100, 200) if enabled else QtGui.QColor(255, 100, 100, 200)
                            painter.setBrush(color)
                            if direction == "up":
                                points = [
                                    QtCore.QPoint(centerX, centerY - arrowLen),
                                    QtCore.QPoint(centerX - arrowWidth // 2, centerY - arrowLen + headLen),
                                    QtCore.QPoint(centerX - arrowWidth // 4, centerY - arrowLen + headLen),
                                    QtCore.QPoint(centerX - arrowWidth // 4, centerY),
                                    QtCore.QPoint(centerX + arrowWidth // 4, centerY),
                                    QtCore.QPoint(centerX + arrowWidth // 4, centerY - arrowLen + headLen),
                                    QtCore.QPoint(centerX + arrowWidth // 2, centerY - arrowLen + headLen),
                                ]
                            elif direction == "down":
                                points = [
                                    QtCore.QPoint(centerX, centerY + arrowLen),
                                    QtCore.QPoint(centerX - arrowWidth // 2, centerY + arrowLen - headLen),
                                    QtCore.QPoint(centerX - arrowWidth // 4, centerY + arrowLen - headLen),
                                    QtCore.QPoint(centerX - arrowWidth // 4, centerY),
                                    QtCore.QPoint(centerX + arrowWidth // 4, centerY),
                                    QtCore.QPoint(centerX + arrowWidth // 4, centerY + arrowLen - headLen),
                                    QtCore.QPoint(centerX + arrowWidth // 2, centerY + arrowLen - headLen),
                                ]
                            elif direction == "left":
                                points = [
                                    QtCore.QPoint(centerX - arrowLen, centerY),
                                    QtCore.QPoint(centerX - arrowLen + headLen, centerY - arrowWidth // 2),
                                    QtCore.QPoint(centerX - arrowLen + headLen, centerY - arrowWidth // 4),
                                    QtCore.QPoint(centerX, centerY - arrowWidth // 4),
                                    QtCore.QPoint(centerX, centerY + arrowWidth // 4),
                                    QtCore.QPoint(centerX - arrowLen + headLen, centerY + arrowWidth // 4),
                                    QtCore.QPoint(centerX - arrowLen + headLen, centerY + arrowWidth // 2),
                                ]
                            else:
                                points = [
                                    QtCore.QPoint(centerX + arrowLen, centerY),
                                    QtCore.QPoint(centerX + arrowLen - headLen, centerY - arrowWidth // 2),
                                    QtCore.QPoint(centerX + arrowLen - headLen, centerY - arrowWidth // 4),
                                    QtCore.QPoint(centerX, centerY - arrowWidth // 4),
                                    QtCore.QPoint(centerX, centerY + arrowWidth // 4),
                                    QtCore.QPoint(centerX + arrowLen - headLen, centerY + arrowWidth // 4),
                                    QtCore.QPoint(centerX + arrowLen - headLen, centerY + arrowWidth // 2),
                                ]
                            painter.drawPolygon(QtGui.QPolygon(points))

                        drawArrow("down", bool(dir4Val[0]))
                        drawArrow("left", bool(dir4Val[1]))
                        drawArrow("right", bool(dir4Val[2]))
                        drawArrow("up", bool(dir4Val[3]))

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
        GameData.RecordSnapshot()
        idx = gy * cols + gx
        count = cols * rows
        if self._mode == TilesetMode.PASSABLE:
            arr = _getListField(self._data, "passable")
            _resizeList(arr, count, False)
            arr[idx] = not bool(arr[idx])
            _setListField(self._data, "passable", arr)
        elif self._mode == TilesetMode.MATERIAL:
            arr = _getListField(self._data, "materials")
            if self.MaterialClass:
                _resizeList(arr, count, self.MaterialClass())
                edit_mat = copy.deepcopy(arr[idx])
                dlg = DataclassEditDialog(self, edit_mat, ELOC("EDIT_MATERIAL"))
                if dlg.exec_():
                    GameData.RecordSnapshot()
                    arr[idx] = edit_mat
                    _setListField(self._data, "materials", arr)
        elif self._mode == TilesetMode.DIR4:
            arrDir4 = _getListField(self._data, "dir4")
            _resizeList(arrDir4, count, (True, True, True, True))

            localX = x - gx * cellSize
            localY = y - gy * cellSize
            topDist = localY
            rightDist = (cellSize - 1) - localX
            bottomDist = (cellSize - 1) - localY
            leftDist = localX
            dists = [topDist, rightDist, bottomDist, leftDist]
            dirIndex = 0
            minDist = dists[0]
            for i in range(1, 4):
                if dists[i] < minDist:
                    minDist = dists[i]
                    dirIndex = i
            dirMap = [3, 2, 0, 1]
            dirIndex = dirMap[dirIndex]

            dir4Val = arrDir4[idx]
            if not isinstance(dir4Val, (list, tuple)) or len(dir4Val) != 4:
                dir4Val = (True, True, True, True)
            newVal = [bool(dir4Val[0]), bool(dir4Val[1]), bool(dir4Val[2]), bool(dir4Val[3])]
            newVal[dirIndex] = not newVal[dirIndex]
            arrDir4[idx] = (newVal[0], newVal[1], newVal[2], newVal[3])
            _setListField(self._data, "dir4", arrDir4)
        self.DATA_CHANGED.emit()
        self.update()


class TilesetPanel(QtWidgets.QWidget):
    MODIFIED = QtCore.pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        System.SetStyle(self, "config.qss")
        Engine = System.GetModule("Engine")
        self.MaterialClass = Engine.Material
        self._data = None
        self._initUI()

    def _initUI(self):
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(5)

        file_row = QtWidgets.QHBoxLayout()
        self.nameLabel = QtWidgets.QLabel(ELOC("TILESET_NAME"))
        self.nameEdit = QtWidgets.QLineEdit()
        self.nameEdit.textEdited.connect(self._onNameChanged)

        self.fileLabel = QtWidgets.QLabel(ELOC("FILE_NAME"))
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
        self.modeList.addItem(ELOC("PASSABLE"))
        self.modeList.addItem(ELOC("MATERIAL"))
        self.modeList.addItem(ELOC("DIR4"))
        self.modeList.setCurrentRow(0)
        self.modeList.currentRowChanged.connect(self._onModeChanged)
        layout.addWidget(self.modeList)

        self.scroll = QtWidgets.QScrollArea(self)
        self.scroll.setBackgroundRole(QtGui.QPalette.Dark)
        self.scroll.setWidgetResizable(False)
        self.imageView = TilesetImageView()
        self.scroll.setWidget(self.imageView)
        self.imageView.DATA_CHANGED.connect(self.MODIFIED.emit)
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
        self.MODIFIED.emit()

    def _onNameChanged(self, text):
        if self._data:
            GameData.RecordSnapshot()
            self._data.name = text
            self.MODIFIED.emit()

    def _onModeChanged(self, row):
        self.imageView.setMode(row)

    def _onBrowseFile(self):
        if not self._data:
            return
        root = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Tilesets")
        dlg = FileSelectorDialog(self, root, FileSelectorDialog.imageFilesFilter())
        fp = dlg.execSelect()
        if fp:
            GameData.RecordSnapshot()
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
            arr_p = _getListField(self._data, "passable")
            arr_m = _getListField(self._data, "materials")
            arrDir4 = _getListField(self._data, "dir4")
            _resizeList(arr_p, new_count, True)
            _resizeList(arr_m, new_count, self.MaterialClass() if self.MaterialClass else None)
            _resizeList(arrDir4, new_count, (True, True, True, True))
            _setListField(self._data, "passable", arr_p)
            _setListField(self._data, "materials", arr_m)
            _setListField(self._data, "dir4", arrDir4)
            if self._key:
                File.mainWindow.editorPanel._renderFromMapData()
                File.mainWindow.editorPanel.update()
                ts = File.mainWindow.tileSelect
                ts.initTilesets()
            self.MODIFIED.emit()
            self.setTilesetData(self._data)
