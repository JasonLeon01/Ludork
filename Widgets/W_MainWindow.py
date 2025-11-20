# -*- encoding: utf-8 -*-

import os
import sys
import subprocess
import configparser
from typing import Optional
from PyQt5 import QtCore, QtGui, QtWidgets
import Utils
from .W_EditorPanel import EditorPanel
from .W_Toggle import ModeToggle
from .W_FileExplorer import FileExplorer
import EditorStatus


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self, title: str):
        super().__init__()
        self.setProjPath(EditorStatus.PROJ_PATH)
        self._engineProc: Optional[subprocess.Popen] = None
        self.resize(1280, 960)
        self.setWindowTitle(title)

        QtWidgets.QApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling, True)
        QtWidgets.QApplication.setAttribute(QtCore.Qt.AA_UseHighDpiPixmaps, True)

        central = QtWidgets.QWidget(self)
        self.setCentralWidget(central)

        self.topBar = QtWidgets.QWidget()
        self.topBar.setMinimumHeight(32)
        topLayout = QtWidgets.QHBoxLayout(self.topBar)
        topLayout.setContentsMargins(0, 0, 0, 0)
        topLayout.addStretch(1)
        if EditorStatus.SCREEN_LOW_RES == 0:
            panelW, panelH = 1280, 960
        elif EditorStatus.SCREEN_LOW_RES == 1:
            panelW, panelH = 960, 720
        elif EditorStatus.SCREEN_LOW_RES == 2:
            panelW, panelH = 640, 480

        self.editorPanel = EditorPanel()
        self.editorPanel.setObjectName("EditorPanel")
        self.editorPanel.setAttribute(QtCore.Qt.WA_NativeWindow, True)
        self.editorPanel.setAutoFillBackground(True)
        pal = self.editorPanel.palette()
        pal.setColor(QtGui.QPalette.Window, QtGui.QColor.fromRgb(0, 0, 0))
        self.editorPanel.setPalette(pal)
        self._panelScale = panelW / 640.0
        self.topBar.setMinimumHeight(int(32 * self._panelScale))
        self.editorPanel.setScale(self._panelScale)

        self.editorScroll = QtWidgets.QScrollArea()
        self.editorScroll.setWidget(self.editorPanel)
        self.editorScroll.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignTop)
        self.editorScroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.editorScroll.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.editorScroll.setFixedSize(panelW, panelH)
        self.editorPanel.setObjectName("EditorPanel")
        self.editorPanel.setAttribute(QtCore.Qt.WA_NativeWindow, True)
        self.editorPanel.setAutoFillBackground(True)
        pal = self.editorPanel.palette()
        pal.setColor(QtGui.QPalette.Window, QtGui.QColor.fromRgb(0, 0, 0))
        self.editorPanel.setPalette(pal)

        self.gamePanel = QtWidgets.QWidget()
        self.gamePanel.setFixedSize(panelW, panelH)
        self.gamePanel.setObjectName("GamePanel")
        self.gamePanel.setAttribute(QtCore.Qt.WA_NativeWindow, True)
        self.gamePanel.setAutoFillBackground(True)
        pal = self.gamePanel.palette()
        pal.setColor(QtGui.QPalette.Window, QtGui.QColor.fromRgb(0, 0, 0))
        self.gamePanel.setPalette(pal)
        self._panelHandle = int(self.gamePanel.winId())

        self.modeToggle = ModeToggle(self._panelScale)
        topLayout.addWidget(self.modeToggle, 0, alignment=QtCore.Qt.AlignRight)
        self.modeToggle.selectionChanged.connect(self._onModeChanged)

        self.leftListIndex = -1
        self.leftList = QtWidgets.QListWidget()
        self.leftList.setMinimumWidth(320)
        self.leftList.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.refreshLeftList()
        self.leftList.itemClicked.connect(self._onLeftItemClicked)

        self.centerArea = QtWidgets.QWidget()
        centerLayout = QtWidgets.QVBoxLayout(self.centerArea)
        centerLayout.setContentsMargins(0, 0, 0, 0)
        centerLayout.setSpacing(0)
        centerLayout.addWidget(self.topBar, 0, alignment=QtCore.Qt.AlignTop)
        self.stacked = QtWidgets.QStackedLayout()
        self.stacked.addWidget(self.editorScroll)
        self.stacked.addWidget(self.gamePanel)
        self.stacked.setCurrentWidget(self.editorScroll)
        centerLayout.addLayout(self.stacked)
        centerLayout.addStretch(1)
        self.centerArea.setFixedWidth(self.gamePanel.width())

        self.rightArea = QtWidgets.QWidget()
        self.rightArea.setMinimumWidth(320)
        rightLayout = QtWidgets.QVBoxLayout(self.rightArea)
        rightLayout.setContentsMargins(0, 0, 0, 0)
        rightLayout.setSpacing(0)
        rightLayout.addStretch(1)

        self.upperSplitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        self.upperSplitter.setChildrenCollapsible(False)
        self.upperSplitter.addWidget(self.leftList)
        self.upperSplitter.addWidget(self.centerArea)
        self.upperSplitter.addWidget(self.rightArea)
        self.upperSplitter.setStretchFactor(0, 1)
        self.upperSplitter.setStretchFactor(1, 0)
        self.upperSplitter.setStretchFactor(2, 1)
        self.upperSplitter.setSizes([320, self.gamePanel.width(), 320])
        self.upperSplitter.splitterMoved.connect(self._onUpperSplitterMoved)
        cfg = configparser.ConfigParser()
        cfg_path = os.path.join(os.getcwd(), "Ludork.ini")
        self._savedLeftWidth = None
        self._savedRightWidth = None
        if os.path.exists(cfg_path):
            cfg.read(cfg_path)
            if "Ludork" in cfg:
                ls = cfg["Ludork"].get("UpperLeftWidth")
                rs = cfg["Ludork"].get("UpperRightWidth")
                if ls and rs:
                    try:
                        self._savedLeftWidth = max(320, int(ls))
                        self._savedRightWidth = max(320, int(rs))
                    except Exception:
                        self._savedLeftWidth = None
                        self._savedRightWidth = None

        self.lowerArea = QtWidgets.QWidget()
        lowerLayout = QtWidgets.QVBoxLayout(self.lowerArea)
        lowerLayout.setContentsMargins(0, 0, 0, 0)
        lowerLayout.setSpacing(0)
        self.fileExplorer = FileExplorer(EditorStatus.PROJ_PATH)
        lowerLayout.addWidget(self.fileExplorer)

        self.topSplitter = QtWidgets.QSplitter(QtCore.Qt.Vertical)
        self.topSplitter.setChildrenCollapsible(False)
        self.topSplitter.addWidget(self.upperSplitter)
        self.topSplitter.addWidget(self.lowerArea)
        topH = self.topBar.minimumHeight() + self.gamePanel.height()
        self.upperSplitter.setFixedHeight(topH)
        self.lowerArea.setMinimumHeight(160)
        self.topSplitter.setSizes([topH, max(self.height() - topH, 160)])

        minW = 320 + self.gamePanel.width() + 320 + self.upperSplitter.handleWidth() * 2 + 16
        minH = topH + 160 + 8
        self.setMinimumSize(minW, minH)
        self._prevFG = self.frameGeometry()
        self._prevUpperW = self.upperSplitter.width()
        self._prevLeftW = self.leftList.width()
        self._prevRightW = self.rightArea.width()
        self._sizesInitialized = False
        self._hasShown = False

        layout = QtWidgets.QVBoxLayout(central)
        layout.setContentsMargins(8, 0, 8, 8)
        layout.setSpacing(0)
        layout.addWidget(self.topSplitter)

    def showEvent(self, event: QtGui.QShowEvent) -> None:
        super().showEvent(event)
        applyLW = self._savedLeftWidth
        applyRW = self._savedRightWidth
        if applyLW is not None and applyRW is not None:
            cw = self.gamePanel.width()
            self.upperSplitter.setSizes([applyLW, cw, applyRW])
            self._prevLeftW = applyLW
            self._prevRightW = applyRW
        else:
            sizes = self.upperSplitter.sizes()
            if len(sizes) >= 3:
                self._prevLeftW = sizes[0]
                self._prevRightW = sizes[2]
        self._prevUpperW = self.upperSplitter.width()
        self._prevFG = self.frameGeometry()
        self._sizesInitialized = True
        self._hasShown = True

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        if not self._hasShown:
            self._prevUpperW = self.upperSplitter.width()
            self._prevFG = self.frameGeometry()
            topH = self.topBar.minimumHeight() + self.gamePanel.height()
            self.topSplitter.setSizes([topH, max(self.height() - topH, 160)])
            return
        if not self._sizesInitialized:
            sizes = self.upperSplitter.sizes()
            if len(sizes) >= 3 and sizes[0] > 0 and sizes[2] > 0:
                self._prevLeftW = sizes[0]
                self._prevRightW = sizes[2]
                self._prevUpperW = self.upperSplitter.width()
                self._prevFG = self.frameGeometry()
                self._sizesInitialized = True
        newFG = self.frameGeometry()
        dxL = newFG.left() - self._prevFG.left()
        dxR = newFG.right() - self._prevFG.right()
        directionLeft = abs(dxL) > abs(dxR)
        newUpperW = self.upperSplitter.width()
        deltaW = newUpperW - self._prevUpperW
        deltaEvent = event.size().width() - event.oldSize().width()
        if deltaW == 0:
            deltaW = deltaEvent
        centerW = self.gamePanel.width()
        minLeft = max(320, self.leftList.minimumWidth())
        minRight = max(320, self.rightArea.minimumWidth())
        sizesNow = (
            self.upperSplitter.sizes()
            if hasattr(self.upperSplitter, "sizes")
            else [self._prevLeftW, centerW, self._prevRightW]
        )
        leftW = sizesNow[0] if len(sizesNow) >= 3 else self._prevLeftW
        rightW = sizesNow[2] if len(sizesNow) >= 3 else self._prevRightW
        if deltaW != 0:
            if directionLeft:
                if deltaW > 0:
                    leftW += deltaW
                else:
                    need = -deltaW
                    take = min(need, max(0, leftW - minLeft))
                    leftW -= take
                    need -= take
                    if need > 0:
                        take2 = min(need, max(0, rightW - minRight))
                        rightW -= take2

            else:
                if deltaW > 0:
                    rightW += deltaW
                else:
                    need = -deltaW
                    take = min(need, max(0, rightW - minRight))
                    rightW -= take
                    need -= take
                    if need > 0:
                        take2 = min(need, max(0, leftW - minLeft))
                        leftW -= take2
            leftW = max(leftW, minLeft)
            rightW = max(rightW, minRight)
            self.upperSplitter.setSizes([leftW, centerW, rightW])
            self._prevLeftW = leftW
            self._prevRightW = rightW
            self._prevUpperW = newUpperW
        self._prevFG = newFG
        topH = self.topBar.minimumHeight() + self.gamePanel.height()
        self.topSplitter.setSizes([topH, max(self.height() - topH, 160)])
        if self._hasShown:
            cfg = configparser.ConfigParser()
            cfg_path = os.path.join(os.getcwd(), "Ludork.ini")
            assert os.path.exists(cfg_path)
            cfg.read(cfg_path)
            assert "Ludork" in cfg
            s = self.size()
            cfg["Ludork"]["Width"] = str(s.width())
            cfg["Ludork"]["Height"] = str(s.height())
            cfg["Ludork"]["UpperLeftWidth"] = str(self._prevLeftW)
            cfg["Ludork"]["UpperRightWidth"] = str(self._prevRightW)
            with open(cfg_path, "w") as f:
                cfg.write(f)

    def getPanelHandle(self) -> int:
        return int(self.gamePanel.winId())

    def setProjPath(self, projPath: str):
        self._mapFilesRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        if hasattr(self, "stacked"):
            self.stacked.setCurrentWidget(self.editorScroll)
            self.editorPanel.refreshMap()
        if hasattr(self, "fileExplorer"):
            self.fileExplorer.setRootPath(EditorStatus.PROJ_PATH)

    def startGame(self):
        self.endGame()
        self.stacked.setCurrentWidget(self.gamePanel)
        self.leftList.setEnabled(False)
        iniPath = os.path.join(EditorStatus.PROJ_PATH, "Main.ini")
        iniFile = configparser.ConfigParser()
        iniFile.read(iniPath, encoding="utf-8")
        script_path = iniFile["Main"]["script"]
        self._panelHandle = int(self.gamePanel.winId())
        self._engineProc = subprocess.Popen(
            [sys.executable, script_path, str(self._panelHandle)], cwd=EditorStatus.PROJ_PATH, shell=False
        )

    def endGame(self):
        if self._engineProc:
            self._engineProc.terminate()
            self._engineProc.wait()
            self._engineProc = None
        Utils.Panel.clearPanel(self.gamePanel)
        self.stacked.setCurrentWidget(self.editorScroll)
        self.leftList.setEnabled(True)

    def _onModeChanged(self, idx: int) -> None:
        if idx == 1:
            self.startGame()
        else:
            self.endGame()
        if hasattr(self, "modeToggle"):
            self.modeToggle.setSelected(idx)

    def _onUpperSplitterMoved(self, pos: int, index: int) -> None:
        sizes = self.upperSplitter.sizes()
        if len(sizes) >= 3:
            self._prevLeftW = sizes[0]
            self._prevRightW = sizes[2]
        self._prevUpperW = self.upperSplitter.width()
        cfg = configparser.ConfigParser()
        cfg_path = os.path.join(os.getcwd(), "Ludork.ini")
        if os.path.exists(cfg_path):
            cfg.read(cfg_path)
            if "Ludork" not in cfg:
                cfg["Ludork"] = {}
            cfg["Ludork"]["UpperLeftWidth"] = str(self._prevLeftW)
            cfg["Ludork"]["UpperRightWidth"] = str(self._prevRightW)
            with open(cfg_path, "w") as f:
                cfg.write(f)

    def _onLeftItemClicked(self, item: QtWidgets.QListWidgetItem) -> None:
        if item is None:
            return
        name = item.text()
        self.leftListIndex = self.leftList.row(item)
        self.editorPanel.refreshMap(name)

    def refreshLeftList(self):
        self.leftList.clear()
        if os.path.exists(self._mapFilesRoot):
            mapFiles = os.listdir(self._mapFilesRoot)
            mapFiles = [f for f in mapFiles if f.endswith(".dat")]
            self.leftList.addItems(mapFiles)
        if self.leftListIndex >= 0 and self.leftListIndex < self.leftList.count():
            self.leftList.setCurrentRow(self.leftListIndex)
        else:
            self.leftListIndex = -1
