# -*- encoding: utf-8 -*-

import os
from PyQt5 import QtWidgets, QtGui, QtCore
from EditorGlobal import EditorStatus, GameData
from Utils import EditorData, System, File
from .FileSelectorDialog import FileSelectorDialog
from .DataclassEditDialog import DataclassEditDialog


class _AutoTileImageView(QtWidgets.QWidget):
    DATA_CHANGED = QtCore.pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._image = None
        self._data = None
        self._mode = 0
        self.MaterialType = getattr(System.GetModule("Engine"), "Material", None)
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
        self._mode = int(mode)
        self.update()

    def paintEvent(self, event):
        painter = QtGui.QPainter(self)
        painter.fillRect(self.rect(), QtGui.QColor(30, 30, 30))
        if not self._image:
            return
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

        if not self._data:
            return

        frameRect = QtCore.QRect(0, 0, min(3, cols) * cellSize, min(4, rows) * cellSize)

        if self._mode == 0:
            val = bool(EditorData.GetField(self._data, "passable", True))
            painter.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255, 220), 3))
            margin = max(8, cellSize // 3)
            drawRect = frameRect.adjusted(margin, margin, -margin, -margin)
            if val:
                painter.drawEllipse(drawRect)
            else:
                painter.drawLine(drawRect.topLeft(), drawRect.bottomRight())
                painter.drawLine(drawRect.topRight(), drawRect.bottomLeft())
        else:
            mat = EditorData.GetField(self._data, "material", None)
            if not EditorData.IsDefaultMaterial(mat):
                painter.setPen(QtGui.QPen(QtGui.QColor(100, 255, 100, 220), 3))
                painter.drawRect(frameRect.adjusted(6, 6, -6, -6))
                painter.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255)))
                f = painter.font()
                f.setPixelSize(max(12, cellSize // 2))
                f.setBold(True)
                painter.setFont(f)
                painter.drawText(frameRect, QtCore.Qt.AlignCenter, "M")

    def mousePressEvent(self, e):
        if not self._image or not self._data:
            return
        if e.button() != QtCore.Qt.LeftButton:
            return
        if self._mode == 0:
            GameData.RecordSnapshot()
            current = bool(EditorData.GetField(self._data, "passable", True))
            EditorData.SetField(self._data, "passable", not current)
            self.DATA_CHANGED.emit()
            self.update()
        else:
            mat = EditorData.GetField(self._data, "material", None)
            edit_mat = EditorData.MaterialEditorObject(mat, self.MaterialType)
            dlg = DataclassEditDialog(self, edit_mat, ELOC("EDIT_MATERIAL"))
            if dlg.exec_():
                GameData.RecordSnapshot()
                EditorData.SetField(self._data, "material", EditorData.MaterialDataFromEditorObject(edit_mat))
                self.DATA_CHANGED.emit()
                self.update()


class AutoTilePanel(QtWidgets.QWidget):
    MODIFIED = QtCore.pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        System.SetStyle(self, "config.qss")
        self._data = None
        self._key = None
        self._initUI()

    def _initUI(self):
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(5)

        file_row = QtWidgets.QHBoxLayout()
        self.nameLabel = QtWidgets.QLabel(ELOC("AUTOTILE_NAME"))
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
        self.modeList.setCurrentRow(0)
        self.modeList.currentRowChanged.connect(self._onModeChanged)
        layout.addWidget(self.modeList)

        self.scroll = QtWidgets.QScrollArea(self)
        self.scroll.setBackgroundRole(QtGui.QPalette.Dark)
        self.scroll.setWidgetResizable(False)
        self.imageView = _AutoTileImageView()
        self.scroll.setWidget(self.imageView)
        self.imageView.DATA_CHANGED.connect(self.MODIFIED.emit)
        layout.addWidget(self.scroll)

    def setAutoTileData(self, autoTileData):
        self._data = autoTileData
        self._key = None
        for k, v in GameData.autoTileData.items():
            if v is autoTileData:
                self._key = k
                break
        if not autoTileData:
            self.nameEdit.clear()
            self.fileEdit.clear()
            self.imageView.setData(None, None)
            return

        p = None
        fileName = EditorData.AutoTileFileName(autoTileData)
        if fileName:
            p = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Autotiles", fileName)
            if not os.path.exists(p):
                EditorData.SetField(autoTileData, "fileName", "")
                fileName = ""
                p = None

        self.nameEdit.setText(str(EditorData.GetField(autoTileData, "name", "") or ""))
        self.fileEdit.setText(fileName)
        self.imageView.setData(autoTileData, p)
        self.imageView.setMode(self.modeList.currentRow())
        self.MODIFIED.emit()

    def _onNameChanged(self, text):
        if self._data:
            GameData.RecordSnapshot()
            EditorData.SetField(self._data, "name", text)
            self.MODIFIED.emit()

    def _onModeChanged(self, row):
        self.imageView.setMode(row)

    def _onBrowseFile(self):
        if not self._data:
            return
        root = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Autotiles")
        if not os.path.exists(root):
            os.makedirs(root, exist_ok=True)
        dlg = FileSelectorDialog(self, root, FileSelectorDialog.imageFilesFilter())
        fp = dlg.execSelect()
        if not fp:
            return
        if not self._validateFile(fp):
            return
        GameData.RecordSnapshot()
        filename = os.path.basename(fp)
        EditorData.SetField(self._data, "fileName", filename)
        if EditorData.GetField(self._data, "material", None) is None:
            EditorData.SetField(self._data, "material", EditorData.NormaliseMaterialData())
        File.mainWindow.tileSelect.initAutoTiles()
        self.MODIFIED.emit()
        self.setAutoTileData(self._data)

    def _validateFile(self, fp: str) -> bool:
        img = QtGui.QImage(fp)
        if img.isNull():
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("AUTOTILE_FILE_INVALID"))
            return False
        w = img.width()
        h = img.height()
        if w < 96 or h < 128 or w % 96 != 0:
            QtWidgets.QMessageBox.warning(
                self,
                ELOC("ERROR"),
                ELOC("AUTOTILE_FILE_SIZE_INVALID").format(w=w, h=h),
            )
            return False
        return True
