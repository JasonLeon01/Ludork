# -*- encoding: utf-8 -*-

from PyQt5 import QtWidgets, QtGui, QtCore
import os
from enum import Enum
import EditorStatus
import Data
from Utils import Locale, System, File
from .FileSelectorDialog import FileSelectorDialog
from .SingleRowDialog import SingleRowDialog


class TilesetMode(Enum):
    PASSABLE = 0
    LIGHTBLOCK = 1


class TilesetImageView(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._image = None
        self._data = None
        self._mode = TilesetMode.PASSABLE
        self._key = None
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

            cell_size = EditorStatus.CELLSIZE
            if cell_size <= 0:
                return
            cols = self._image.width() // cell_size
            rows = self._image.height() // cell_size

            pen = QtGui.QPen(QtGui.QColor(0, 0, 0, 100))
            painter.setPen(pen)
            for x in range(cols + 1):
                painter.drawLine(x * cell_size, 0, x * cell_size, self._image.height())
            for y in range(rows + 1):
                painter.drawLine(0, y * cell_size, self._image.width(), y * cell_size)

            if self._data:
                count = cols * rows
                font = painter.font()
                font.setPixelSize(10)
                painter.setFont(font)

                for i in range(count):
                    x = (i % cols) * cell_size
                    y = (i // cols) * cell_size
                    rect = QtCore.QRect(x, y, cell_size, cell_size)

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

                    elif self._mode == TilesetMode.LIGHTBLOCK:
                        lb_arr = getattr(self._data, "lightBlock", [])
                        val = 0.0
                        if i < len(lb_arr):
                            try:
                                val = float(lb_arr[i])
                            except Exception:
                                val = 0.0

                        painter.setPen(QtGui.QColor(255, 255, 255))
                        txt = ("{:.6f}".format(val)).rstrip("0").rstrip(".")
                        if not txt:
                            txt = "0"
                        painter.drawText(rect, QtCore.Qt.AlignCenter, txt)

    def mousePressEvent(self, e):
        if not self._image or not self._data:
            return
        if e.button() != QtCore.Qt.LeftButton:
            return
        cell_size = EditorStatus.CELLSIZE
        if cell_size <= 0:
            return
        x = int(e.pos().x())
        y = int(e.pos().y())
        cols = self._image.width() // cell_size
        rows = self._image.height() // cell_size
        gx = x // cell_size
        gy = y // cell_size
        if gx < 0 or gy < 0 or gx >= cols or gy >= rows:
            return
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
        else:
            arr = getattr(self._data, "lightBlock", None)
            if not isinstance(arr, list):
                arr = []
                setattr(self._data, "lightBlock", arr)
            if len(arr) < count:
                arr.extend([0.0] * (count - len(arr)))
            current = float(arr[idx]) if idx < len(arr) else 0.0
            init_text = ("{:.6f}".format(current)).rstrip("0").rstrip(".")
            if not init_text:
                init_text = "0"
            dlg = SingleRowDialog(
                self.parent(),
                Locale.getContent("LIGHT_BLOCK"),
                Locale.getContent("LIGHT_BLOCK"),
                init_text,
                input_mode="number",
                min_value=0.0,
                max_value=1.0,
            )
            ok, text = dlg.execGetText()
            if ok:
                t = text.strip()
                if not t:
                    return
                try:
                    val = float(t)
                except Exception as e:
                    QtWidgets.QMessageBox.warning(self, "Hint", str(e))
                    return
                if val < 0.0:
                    val = 0.0
                if val > 1.0:
                    val = 1.0
                arr[idx] = val
        key = getattr(self, "_key", None)
        if key:
            Data.GameData.markTilesetModified(key)
            if getattr(File, "mainWindow", None):
                File.mainWindow.setWindowTitle(System.get_title())
        self.update()


class TilesetPanel(QtWidgets.QWidget):
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
        self.modeList.addItem(Locale.getContent("LIGHT_BLOCK"))
        self.modeList.setCurrentRow(0)
        self.modeList.currentRowChanged.connect(self._onModeChanged)
        layout.addWidget(self.modeList)

        self.scroll = QtWidgets.QScrollArea(self)
        self.scroll.setBackgroundRole(QtGui.QPalette.Dark)
        self.scroll.setWidgetResizable(False)
        self.imageView = TilesetImageView()
        self.scroll.setWidget(self.imageView)
        layout.addWidget(self.scroll)

    def setTilesetData(self, tileset_data):
        self._data = tileset_data
        self._key = None
        for k, v in Data.GameData.tilesetData.items():
            if v is tileset_data or (tileset_data and v.fileName == tileset_data.fileName):
                self._key = k
                break
        if not tileset_data:
            self.nameEdit.clear()
            self.fileEdit.clear()
            self.imageView.setData(None, None)
            return
        p = None
        if tileset_data.fileName:
            p = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Tilesets", tileset_data.fileName)
            if not os.path.exists(p):
                tileset_data.fileName = ""
                p = None
                if self._key:
                    Data.GameData.markTilesetModified(self._key)
                    if getattr(File, "mainWindow", None):
                        File.mainWindow.setWindowTitle(System.get_title())

        self.nameEdit.setText(getattr(tileset_data, "name", ""))
        self.fileEdit.setText(tileset_data.fileName)
        self.imageView.setData(tileset_data, p)
        self.imageView.setTilesetKey(self._key)
        self.imageView.setMode(self.modeList.currentRow())

    def _onNameChanged(self, text):
        if self._data:
            self._data.name = text
            if self._key:
                Data.GameData.markTilesetModified(self._key)
                if getattr(File, "mainWindow", None):
                    File.mainWindow.setWindowTitle(System.get_title())

    def _onModeChanged(self, row):
        self.imageView.setMode(row)

    def _onBrowseFile(self):
        if not self._data:
            return
        root = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Tilesets")
        dlg = FileSelectorDialog(self, root, "Images (*.png *.jpg *.bmp *.jpeg)")
        fp = dlg.execSelect()
        if fp:
            filename = os.path.basename(fp)
            self._data.fileName = filename
            cell_size = EditorStatus.CELLSIZE
            new_count = 0
            if isinstance(cell_size, int) and cell_size > 0:
                img = QtGui.QImage(fp)
                if not img.isNull():
                    cols = img.width() // cell_size
                    rows = img.height() // cell_size
                    new_count = max(0, cols * rows)
            arr_p = getattr(self._data, "passable", [])
            arr_l = getattr(self._data, "lightBlock", [])
            if not isinstance(arr_p, list):
                arr_p = []
                setattr(self._data, "passable", arr_p)
            if not isinstance(arr_l, list):
                arr_l = []
                setattr(self._data, "lightBlock", arr_l)
            if len(arr_p) < new_count:
                arr_p.extend([True] * (new_count - len(arr_p)))
            elif len(arr_p) > new_count:
                del arr_p[new_count:]
            if len(arr_l) < new_count:
                arr_l.extend([0.5] * (new_count - len(arr_l)))
            elif len(arr_l) > new_count:
                del arr_l[new_count:]
            if self._key:
                Data.GameData.markTilesetModified(self._key)
                if getattr(File, "mainWindow", None):
                    File.mainWindow.setWindowTitle(System.get_title())
                    File.mainWindow.editorPanel._renderFromMapData()
                    File.mainWindow.editorPanel.update()
                    ts = File.mainWindow.tileSelect
                    ts.initTilesets()
            self.setTilesetData(self._data)
