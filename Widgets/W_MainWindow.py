# -*- encoding: utf-8 -*-

import os
import sys
import subprocess
import psutil
import configparser
from typing import Optional
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import Locale, Panel
from .W_EditorPanel import EditorPanel
from .W_Toggle import ModeToggle, EditModeToggle
from .W_TileSelect import TileSelect
from .W_FileExplorer import FileExplorer
from .W_Console import ConsoleWidget
import EditorStatus


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self, title: str):
        super().__init__()
        self.setProjPath(EditorStatus.PROJ_PATH)
        self._engineProc: Optional[subprocess.Popen] = None
        self.resize(1280, 960)
        self.setWindowTitle(title)

        central = QtWidgets.QWidget(self)
        self.setCentralWidget(central)

        self.topBar = QtWidgets.QWidget()
        self.topBar.setMinimumHeight(32)
        topLayout = QtWidgets.QHBoxLayout(self.topBar)
        topLayout.setContentsMargins(0, 0, 0, 0)
        self.layerScroll = QtWidgets.QScrollArea()
        self.layerScroll.setWidgetResizable(True)
        self.layerScroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.layerScroll.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self.layerScroll.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Fixed)
        self.layerScroll.setStyleSheet("QScrollArea { border: 2px solid palette(mid); border-radius: 4px; }")
        self.layerBarContainer = QtWidgets.QWidget()
        self.layerBarLayout = QtWidgets.QHBoxLayout(self.layerBarContainer)
        self.layerBarLayout.setContentsMargins(8, 0, 8, 0)
        self.layerBarLayout.setSpacing(4)
        self.layerScroll.setWidget(self.layerBarContainer)
        self._layerButtons = {}
        self._selectedLayerName: Optional[str] = None
        panelW, panelH = 640, 480

        self.editorPanel = EditorPanel()
        self.editorPanel.setObjectName("EditorPanel")
        self.editorPanel.setAttribute(QtCore.Qt.WA_NativeWindow, True)
        self.editorPanel.setAutoFillBackground(True)
        pal = self.editorPanel.palette()
        pal.setColor(QtGui.QPalette.Window, QtGui.QColor.fromRgb(0, 0, 0))
        self.editorPanel.setPalette(pal)
        self.topBar.setMinimumHeight(32)
        self.layerScroll.setMinimumHeight(self.topBar.minimumHeight())

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

        self.editModeToggle = EditModeToggle()
        self.modeToggle = ModeToggle()
        self.addLayerButton = QtWidgets.QPushButton(Locale.getContent("ADD_LAYER"))
        self.addLayerButton.setMinimumHeight(self.topBar.minimumHeight())
        self.addLayerButton.clicked.connect(self._onAddLayer)
        topLayout.addWidget(self.layerScroll, 1)
        topLayout.addWidget(self.addLayerButton, 0, alignment=QtCore.Qt.AlignRight)
        topLayout.addWidget(self.editModeToggle, 0, alignment=QtCore.Qt.AlignRight)
        topLayout.addWidget(self.modeToggle, 0, alignment=QtCore.Qt.AlignRight)
        self.editModeToggle.selectionChanged.connect(self._onEditModeChanged)
        self.modeToggle.selectionChanged.connect(self._onModeChanged)
        self._menuBar = self.menuBar()
        self._menuBar.setNativeMenuBar(True)
        _fileMenu = self._menuBar.addMenu("File")
        self._actNewProject = QtWidgets.QAction(Locale.getContent("NEW_PROJECT"), self)
        self._actNewProject.setShortcut(QtGui.QKeySequence.StandardKey.New)
        self._actNewProject.triggered.connect(self._onNewProject)
        self._actOpenProject = QtWidgets.QAction(Locale.getContent("OPEN_PROJECT"), self)
        self._actOpenProject.setShortcut(QtGui.QKeySequence.StandardKey.Open)
        self._actOpenProject.triggered.connect(self._onOpenProject)
        self._actSave = QtWidgets.QAction(Locale.getContent("SAVE"), self)
        self._actSave.setShortcut(QtGui.QKeySequence.StandardKey.Save)
        self._actSave.triggered.connect(self._onSave)
        self._actExit = QtWidgets.QAction(Locale.getContent("EXIT"), self)
        self._actExit.setShortcut(QtGui.QKeySequence.StandardKey.Close)
        self._actExit.triggered.connect(self._onExit)
        _fileMenu.addAction(self._actNewProject)
        _fileMenu.addAction(self._actOpenProject)
        _fileMenu.addAction(self._actSave)
        _fileMenu.addAction(self._actExit)

        self.leftListIndex = -1
        self.leftList = QtWidgets.QListWidget()
        self.leftList.setMinimumWidth(320)
        self.leftList.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.refreshLeftList()
        self.leftList.itemClicked.connect(self._onLeftItemClicked)

        self.leftLabel = QtWidgets.QLabel(Locale.getContent("MAP_LIST"))
        self.leftLabel.setAlignment(QtCore.Qt.AlignCenter)
        self.leftLabel.setSizePolicy(QtWidgets.QSizePolicy.Preferred, QtWidgets.QSizePolicy.Fixed)
        self.leftLabel.setFixedHeight(32)
        _lh = 32
        _font = self.leftLabel.font()
        _font.setBold(True)
        _font.setPixelSize(max(12, int(_lh * 0.6)))
        self.leftLabel.setFont(_font)
        self.leftArea = QtWidgets.QWidget()
        leftLayout = QtWidgets.QVBoxLayout(self.leftArea)
        leftLayout.setContentsMargins(0, 0, 0, 0)
        leftLayout.setSpacing(0)
        leftLayout.addWidget(self.leftLabel, 0, alignment=QtCore.Qt.AlignHCenter)
        leftLayout.addWidget(self.leftList, 1)
        self.leftArea.setMinimumWidth(320)

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
        self.tileSelect = TileSelect(self.rightArea)
        rightLayout.addWidget(self.tileSelect, 1)
        self.tileSelect.tileSelected.connect(self._onTileSelected)
        self.tileSelect.tilesetChanged.connect(self._onTilesetChanged)
        self.editorPanel.tileNumberPicked.connect(self._onTileNumberPicked)

        self.upperSplitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        self.upperSplitter.setChildrenCollapsible(False)
        self.upperSplitter.addWidget(self.leftArea)
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
                    self._savedLeftWidth = max(320, int(ls))
                    self._savedRightWidth = max(320, int(rs))

        self.lowerArea = QtWidgets.QWidget()
        lowerLayout = QtWidgets.QVBoxLayout(self.lowerArea)
        lowerLayout.setContentsMargins(0, 0, 0, 0)
        lowerLayout.setSpacing(0)
        self.fileExplorer = FileExplorer(EditorStatus.PROJ_PATH)
        self.consoleWidget = ConsoleWidget()
        self.tabWidget = QtWidgets.QTabWidget()
        self.tabWidget.setTabPosition(QtWidgets.QTabWidget.North)
        self.tabWidget.setTabBarAutoHide(False)
        self.tabWidget.addTab(self.fileExplorer, Locale.getContent("FILE_EXPLORER"))
        self.tabWidget.addTab(self.consoleWidget, Locale.getContent("CONSOLE"))
        lowerLayout.addWidget(self.tabWidget)

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
        self._prevLeftW = self.leftArea.width()
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
        minLeft = max(320, self.leftArea.minimumWidth())
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
        Panel.applyDisabledOpacity(self.leftList)
        if hasattr(self, "fileExplorer"):
            self.fileExplorer.setInteractive(False)
        if hasattr(self, "editModeToggle"):
            self.editModeToggle.setEnabled(False)
            Panel.applyDisabledOpacity(self.editModeToggle)
        iniPath = os.path.join(EditorStatus.PROJ_PATH, "Main.ini")
        iniFile = configparser.ConfigParser()
        iniFile.read(iniPath, encoding="utf-8")
        scriptPath = iniFile["Main"]["script"]
        self._panelHandle = int(self.gamePanel.winId())
        self._engineProc = subprocess.Popen(
            self._getExec(scriptPath),
            cwd=EditorStatus.PROJ_PATH,
            shell=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            stdin=subprocess.PIPE,
            text=True,
            bufsize=1,
            env=dict(os.environ, PYTHONUNBUFFERED="1"),
        )
        self.consoleWidget.attach_process(self._engineProc)

    def endGame(self):
        if self._engineProc:
            p = psutil.Process(self._engineProc.pid)
            children = p.children(recursive=True)
            for c in children:
                try:
                    c.terminate()
                except Exception:
                    pass
            gone, alive = psutil.wait_procs(children, timeout=2)
            for c in alive:
                c.kill()
            self._engineProc = None
        Panel.clearPanel(self.gamePanel)
        self.stacked.setCurrentWidget(self.editorScroll)
        self.leftList.setEnabled(True)
        Panel.applyDisabledOpacity(self.leftList)
        if hasattr(self, "fileExplorer"):
            self.fileExplorer.setInteractive(True)
        if hasattr(self, "editModeToggle"):
            self.editModeToggle.setEnabled(True)
            Panel.applyDisabledOpacity(self.editModeToggle)
        if hasattr(self, "consoleWidget"):
            self.consoleWidget.detach_process()
            self.consoleWidget.clear()

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        try:
            self.endGame()
        except Exception:
            pass
        super().closeEvent(event)

    def _onModeChanged(self, idx: int) -> None:
        if idx == 1:
            self.startGame()
        else:
            self.endGame()
        if hasattr(self, "modeToggle"):
            self.modeToggle.setSelected(idx)
        if hasattr(self, "editModeToggle"):
            self.editModeToggle.setEnabled(idx != 1)

    def _onEditModeChanged(self, idx: int) -> None:
        if idx == 0:
            self.toTileMode()
        else:
            self.toActorMode()

    def toTileMode(self) -> None:
        self.editorPanel.setTileMode(True)
        self.tileSelect.setLayerSelected(self._selectedLayerName is not None)

    def toActorMode(self) -> None:
        self.editorPanel.setTileMode(False)
        self.tileSelect.setLayerSelected(False)

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
        self._selectedLayerName = None
        self.editorPanel.setSelectedLayer(None)
        self.tileSelect.setLayerSelected(False)
        self.tileSelect.clearSelection()
        self._refreshLayerBar()

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

    def _refreshLayerBar(self) -> None:
        for i in reversed(range(self.layerBarLayout.count())):
            item = self.layerBarLayout.takeAt(i)
            w = item.widget()
            if w:
                w.deleteLater()
        self._layerButtons.clear()
        names = self.editorPanel.getLayerNames() if hasattr(self.editorPanel, "getLayerNames") else []
        for n in names:
            btn = QtWidgets.QToolButton()
            btn.setText(n)
            btn.setCheckable(True)
            btn.setAutoRaise(True)
            btn.setMinimumHeight(self.topBar.minimumHeight())
            btn.setProperty("layerName", n)
            btn.clicked.connect(self._onLayerButtonClicked)
            btn.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
            btn.customContextMenuRequested.connect(self._onLayerContextMenu)
            self.layerBarLayout.addWidget(btn)
            self._layerButtons[n] = btn
        if self._selectedLayerName in self._layerButtons:
            self._layerButtons[self._selectedLayerName].setChecked(True)
        self.layerBarLayout.addStretch(1)

    def _onLayerButtonClicked(self, checked: bool) -> None:
        sender = self.sender()
        if not isinstance(sender, QtWidgets.QToolButton):
            return
        name = sender.property("layerName") or sender.text()
        if self._selectedLayerName == name:
            if not checked:
                self._selectedLayerName = None
                self.editorPanel.setSelectedLayer(None)
                self.tileSelect.setLayerSelected(False)
                self.tileSelect.clearSelection()
            else:
                sender.setChecked(True)
            return
        for b in self._layerButtons.values():
            if b is not sender:
                b.setChecked(False)
        sender.setChecked(True)
        self._selectedLayerName = name
        self.editorPanel.setSelectedLayer(name)
        key = self.editorPanel.getLayerTilesetKey(name)
        if key:
            self.tileSelect.setCurrentTilesetKey(key)
        self.tileSelect.setLayerSelected(True)

    def _onAddLayer(self, checked: bool = False) -> None:
        if not hasattr(self.editorPanel, "mapData") or self.editorPanel.mapData is None:
            QtWidgets.QMessageBox.warning(self, "Hint", Locale.getContent("ADD_ERROR"))
            return
        existing = set(self.editorPanel.getLayerNames())
        while True:
            name, ok = QtWidgets.QInputDialog.getText(
                self, Locale.getContent("ADD_LAYER"), Locale.getContent("ADD_MESSAGE")
            )
            if not ok:
                return
            name = name.strip()
            if not name:
                QtWidgets.QMessageBox.warning(self, "Hint", Locale.getContent("ADD_EMPTY"))
                continue
            if name in existing:
                QtWidgets.QMessageBox.warning(self, "Hint", Locale.getContent("ADD_DUPLICATE"))
                continue
            break
        self.editorPanel.addEmptyLayer(name)
        self._refreshLayerBar()
        bar = self.layerScroll.horizontalScrollBar()
        bar.setValue(bar.maximum())
        key = self.editorPanel.getLayerTilesetKey(name)
        if key:
            self.tileSelect.setCurrentTilesetKey(key)

    def _onTileSelected(self, tileNumber: int) -> None:
        self.editorPanel.setSelectedTileNumber(None if tileNumber < 0 else tileNumber)

    def _onTilesetChanged(self, key: str) -> None:
        if self._selectedLayerName:
            self.editorPanel.setLayerTilesetForSelectedLayer(key)

    def _onTileNumberPicked(self, tileNumber: int) -> None:
        if self._selectedLayerName:
            key = self.editorPanel.getLayerTilesetKey(self._selectedLayerName)
            if key:
                self.tileSelect.setCurrentTilesetKey(key)
        self.tileSelect.setSelectedTileNumber(None if tileNumber < 0 else tileNumber)

    def _onLayerContextMenu(self, pos: QtCore.QPoint) -> None:
        sender = self.sender()
        if not isinstance(sender, QtWidgets.QToolButton):
            return
        name = sender.property("layerName") or sender.text()
        menu = QtWidgets.QMenu(self)
        actDelete = menu.addAction(Locale.getContent("DELETE"))
        action = menu.exec_(sender.mapToGlobal(pos))
        if action == actDelete:
            ret = QtWidgets.QMessageBox.question(
                self,
                "Hint",
                Locale.getContent("CONFIRM_DELETE_LAYER").format(name=name),
                QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
                QtWidgets.QMessageBox.No,
            )
            if ret == QtWidgets.QMessageBox.Yes:
                if self.editorPanel.removeLayer(name):
                    if self._selectedLayerName == name:
                        self._selectedLayerName = None
                    self._refreshLayerBar()

    def _onNewProject(self, checked: bool = False) -> None:
        pass

    def _onOpenProject(self, checked: bool = False) -> None:
        pass

    def _onSave(self, checked: bool = False) -> None:
        if not hasattr(self.editorPanel, "mapData") or self.editorPanel.mapData is None:
            return
        ok, content = self.editorPanel.saveFile()
        if ok:
            QtWidgets.QMessageBox.information(self, "Hint", Locale.getContent("SAVE_SUCCESS") + content)
        else:
            QtWidgets.QMessageBox.warning(self, "Hint", Locale.getContent("SAVE_FAILED") + content)

    def _onExit(self, checked: bool = False) -> None:
        self.close()

    def _getExec(self, scriptPath):
        exePath = os.path.join(EditorStatus.PROJ_PATH, "Main.exe" if os.name == "nt" else "Main")
        if os.path.exists(exePath):
            if os.name == "nt":
                return [exePath, str(self._panelHandle)]
            return [exePath]
        if os.name == "nt":
            return [sys.executable, "-u", scriptPath, str(self._panelHandle)]
        return [sys.executable, "-u", scriptPath]
