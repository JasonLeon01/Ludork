# -*- encoding: utf-8 -*-

import os
import sys
import subprocess
import psutil
import configparser
import json
import copy
from typing import Optional
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtCore import QSize
from Utils import Locale, Panel, System, File
from Widgets import (
    EditorPanel,
    ModeToggle,
    EditModeToggle,
    TileSelect,
    FileExplorer,
    ConsoleWidget,
    ConfigWindow,
    TilesetEditor,
    SettingsWindow,
)
from Widgets.Utils import MapEditDialog, SingleRowDialog, Toast
import EditorStatus
import Data


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self, title: str):
        super().__init__()
        self.toast = Toast(self)
        self._engineProc: Optional[subprocess.Popen] = None
        self.setWindowTitle(title)

        self.topBar = QtWidgets.QWidget()
        self.layerList = QtWidgets.QListWidget()
        self._selectedLayerName: Optional[str] = None
        self.editorPanel = EditorPanel()
        self.editorPanel.dataChanged.connect(self._refreshUndoRedo)
        self.editorScroll = QtWidgets.QScrollArea()
        self.gamePanel = QtWidgets.QWidget()
        self._panelHandle = int(self.gamePanel.winId())
        self.editModeToggle = EditModeToggle()
        self.modeToggle = ModeToggle()
        self._menuBar = self.menuBar()
        self.leftListIndex = -1
        self.leftList = QtWidgets.QListWidget()
        self.leftLabel = QtWidgets.QLabel(Locale.getContent("MAP_LIST"))
        self.leftArea = QtWidgets.QWidget()
        self.centerArea = QtWidgets.QWidget()
        self.stacked = QtWidgets.QStackedLayout()
        self.rightArea = QtWidgets.QWidget()
        self.tileSelect = TileSelect(self.rightArea)
        self.upperSplitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        self._savedLeftWidth: Optional[int] = None
        self._savedRightWidth: Optional[int] = None
        self.lowerArea = QtWidgets.QWidget()
        self.fileExplorer = FileExplorer(EditorStatus.PROJ_PATH)
        self.consoleWidget = ConsoleWidget()
        self.tabWidget = QtWidgets.QTabWidget()
        self.topSplitter = QtWidgets.QSplitter(QtCore.Qt.Vertical)
        self._sizesInitialized = False
        self._hasShown = False

        self._actNewProject = QtWidgets.QAction(Locale.getContent("NEW_PROJECT"), self)
        self._actNewProject.setIcon(self.style().standardIcon(QtWidgets.QStyle.SP_FileIcon))
        self._actOpenProject = QtWidgets.QAction(Locale.getContent("OPEN_PROJECT"), self)
        self._actOpenProject.setIcon(self.style().standardIcon(QtWidgets.QStyle.SP_DialogOpenButton))
        self._actSave = QtWidgets.QAction(Locale.getContent("SAVE"), self)
        self._actSave.setIcon(self.style().standardIcon(QtWidgets.QStyle.SP_DialogSaveButton))
        self._actExit = QtWidgets.QAction(Locale.getContent("EXIT"), self)
        self._actUndo = QtWidgets.QAction(Locale.getContent("UNDO"), self)
        self._actUndo.setIcon(self.style().standardIcon(QtWidgets.QStyle.SP_ArrowBack))
        self._actRedo = QtWidgets.QAction(Locale.getContent("REDO"), self)
        self._actRedo.setIcon(self.style().standardIcon(QtWidgets.QStyle.SP_ArrowForward))
        self._actGameSettings = QtWidgets.QAction(Locale.getContent("GAME_SETTINGS"), self)
        self._actGameSettings.setIcon(self.style().standardIcon(QtWidgets.QStyle.SP_FileDialogDetailedView))
        self._actDatabaseSystemConfig = QtWidgets.QAction(Locale.getContent("SYSTEM_CONFIG"), self)
        self._actDatabaseTilesetsData = QtWidgets.QAction(Locale.getContent("TILESETS_DATA"), self)
        self._actDatabaseCommonFunctions = QtWidgets.QAction(Locale.getContent("COMMON_FUNCTIONS"), self)
        self._actDatabaseScripts = QtWidgets.QAction(Locale.getContent("SCRIPTS"), self)
        self._actHelpExplanation = QtWidgets.QAction(Locale.getContent("HELP_EXPLANATION"), self)

        self._mapClipboard = None
        self._actCopyMap = QtWidgets.QAction(Locale.getContent("COPY"), self)
        self._actCopyMap.setShortcut(QtGui.QKeySequence.Copy)
        self._actCopyMap.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actCopyMap.triggered.connect(self._onCopyMap)

        self._actPasteMap = QtWidgets.QAction(Locale.getContent("PASTE"), self)
        self._actPasteMap.setShortcut(QtGui.QKeySequence.Paste)
        self._actPasteMap.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actPasteMap.triggered.connect(self._onPasteMap)
        self._actPasteMap.setEnabled(False)

        self._actDeleteMap = QtWidgets.QAction(Locale.getContent("DELETE"), self)
        self._actDeleteMap.setShortcut(QtGui.QKeySequence.Delete)
        self._actDeleteMap.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actDeleteMap.triggered.connect(self._onDeleteMapAction)

        self.setProjPath(EditorStatus.PROJ_PATH)
        self._setStyle()
        self._initProjConfigAndSelection()
        self._engineMonitorTimer: Optional[QtCore.QTimer] = None

    def showEvent(self, event: QtGui.QShowEvent) -> None:
        self._refreshUndoRedo()
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
        topH = self.topBar.minimumHeight() + self.gamePanel.height()
        self.topSplitter.setSizes([topH, max(self.height() - topH, 160)])

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        self.toast._updatePosition()
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
        sizesNow = self.upperSplitter.sizes()
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
            cfg_path = os.path.join(File.getIniPath(), f"{EditorStatus.APP_NAME}.ini")
            assert os.path.exists(cfg_path)
            cfg.read(cfg_path)
            assert EditorStatus.APP_NAME in cfg
            s = self.size()
            cfg[EditorStatus.APP_NAME]["Width"] = str(s.width())
            cfg[EditorStatus.APP_NAME]["Height"] = str(s.height())
            cfg[EditorStatus.APP_NAME]["UpperLeftWidth"] = str(self._prevLeftW)
            cfg[EditorStatus.APP_NAME]["UpperRightWidth"] = str(self._prevRightW)
            with open(cfg_path, "w") as f:
                cfg.write(f)

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        if not self._checkUnsavedChanges():
            event.ignore()
            return
        self._saveProjLastMap()
        self.endGame()
        super().closeEvent(event)

    def _checkUnsavedChanges(self) -> bool:
        if not Data.GameData.checkModified():
            return True

        msgBox = QtWidgets.QMessageBox(self)
        msgBox.setWindowTitle(Locale.getContent("EXIT"))
        msgBox.setText(Locale.getContent("CONFIRM_EXIT_WITH_UNSAVED_CHANGES"))
        msgBox.setIcon(QtWidgets.QMessageBox.Question)

        btnSave = msgBox.addButton(Locale.getContent("SAVE_AND_EXIT"), QtWidgets.QMessageBox.AcceptRole)
        btnDiscard = msgBox.addButton(Locale.getContent("DISCARD_AND_EXIT"), QtWidgets.QMessageBox.DestructiveRole)
        btnCancel = msgBox.addButton(Locale.getContent("CANCEL"), QtWidgets.QMessageBox.RejectRole)

        msgBox.exec_()

        if msgBox.clickedButton() == btnSave:
            Data.GameData.saveAllModified()
            return True
        elif msgBox.clickedButton() == btnDiscard:
            return True

        return False

    def getPanelHandle(self) -> int:
        return int(self.gamePanel.winId())

    def setProjPath(self, projPath: str):
        self._mapFilesRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        self.stacked.setCurrentWidget(self.editorScroll)
        self.editorPanel.refreshMap()
        self.fileExplorer.setRootPath(EditorStatus.PROJ_PATH)

    def startGame(self):
        self.endGame()
        self.stacked.setCurrentWidget(self.gamePanel)
        self.leftList.setEnabled(False)
        Panel.applyDisabledOpacity(self.leftList)
        self.tileSelect.setEnabled(False)
        Panel.applyDisabledOpacity(self.tileSelect)
        self.fileExplorer.setInteractive(False)
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
        self.tabWidget.setCurrentWidget(self.consoleWidget)
        if self._engineMonitorTimer is None:
            self._engineMonitorTimer = QtCore.QTimer(self)
            self._engineMonitorTimer.setInterval(500)
            self._engineMonitorTimer.timeout.connect(self._onEngineProcCheck)
        self._engineMonitorTimer.start()

    def endGame(self):
        if self._engineMonitorTimer is not None:
            self._engineMonitorTimer.stop()
        if self._engineProc:
            try:
                pid = self._engineProc.pid
                if psutil.pid_exists(pid):
                    p = psutil.Process(pid)
                    children = p.children(recursive=True)
                    for c in children:
                        try:
                            c.terminate()
                        except Exception as e:
                            print(f"Error while terminating child process {c.pid}: {e}")
                    gone, alive = psutil.wait_procs(children, timeout=2)
                    for c in alive:
                        c.kill()
                    p.terminate()
                    try:
                        p.wait(timeout=2)
                    except psutil.TimeoutExpired:
                        p.kill()
            except Exception as e:
                print(f"Error while terminating engine process: {e}")
            finally:
                self._engineProc = None
        Panel.clearPanel(self.gamePanel)
        self.stacked.setCurrentWidget(self.editorScroll)
        self.leftList.setEnabled(True)
        Panel.applyDisabledOpacity(self.leftList)
        self.fileExplorer.setInteractive(True)
        self.editModeToggle.setEnabled(True)
        Panel.applyDisabledOpacity(self.editModeToggle)
        self.tileSelect.setEnabled(True)
        Panel.applyDisabledOpacity(self.tileSelect)
        self.consoleWidget.detach_process()
        self.consoleWidget.clear()
        self.tabWidget.setCurrentWidget(self.fileExplorer)

    def toTileMode(self) -> None:
        self.editorPanel.setTileMode(True)
        self.tileSelect.setLayerSelected(self._selectedLayerName is not None)

    def toActorMode(self) -> None:
        self.editorPanel.setTileMode(False)
        self.tileSelect.setLayerSelected(False)

    def refreshLeftList(self):
        self.leftList.clear()
        mapFiles = [k for k in Data.GameData.mapData.keys() if k.endswith(".dat")]
        mapFiles.sort()
        self.leftList.addItems(mapFiles)

        if self.leftListIndex >= 0 and self.leftListIndex < self.leftList.count():
            self.leftList.setCurrentRow(self.leftListIndex)
        else:
            self.leftListIndex = -1

    def _onEngineProcCheck(self) -> None:
        if self._engineProc is None:
            if self._engineMonitorTimer is not None:
                self._engineMonitorTimer.stop()
            return
        try:
            if self._engineProc.poll() is not None:
                if self._engineMonitorTimer is not None:
                    self._engineMonitorTimer.stop()
                self.endGame()
                self.modeToggle.setSelected(0)
        except Exception as e:
            print(f"Error checking engine process state: {e}")

    def _onModeChanged(self, idx: int) -> None:
        if idx == 1:
            self.startGame()
        else:
            self.endGame()
        self.modeToggle.setSelected(idx)
        self.editModeToggle.setEnabled(idx != 1)

    def _onEditModeChanged(self, idx: int) -> None:
        if idx == 0:
            self.toTileMode()
        else:
            self.toActorMode()

    def _onUpperSplitterMoved(self, pos: int, index: int) -> None:
        sizes = self.upperSplitter.sizes()
        if len(sizes) >= 3:
            self._prevLeftW = sizes[0]
            self._prevRightW = sizes[2]
        self._prevUpperW = self.upperSplitter.width()
        cfg = configparser.ConfigParser()
        cfg_path = os.path.join(File.getIniPath(), f"{EditorStatus.APP_NAME}.ini")
        if os.path.exists(cfg_path):
            cfg.read(cfg_path)
            if EditorStatus.APP_NAME not in cfg:
                cfg[EditorStatus.APP_NAME] = {}
            cfg[EditorStatus.APP_NAME]["UpperLeftWidth"] = str(self._prevLeftW)
            cfg[EditorStatus.APP_NAME]["UpperRightWidth"] = str(self._prevRightW)
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

    def _onLeftListContextMenu(self, pos: QtCore.QPoint) -> None:
        item = self.leftList.itemAt(pos)
        menu = QtWidgets.QMenu(self)
        if item is None:
            actNew = menu.addAction(Locale.getContent("NEW_MAP"))
            if self._mapClipboard:
                menu.addAction(self._actPasteMap)
            else:
                self._actPasteMap.setEnabled(False)
                menu.addAction(self._actPasteMap)
                self._actPasteMap.setEnabled(True)

            action = menu.exec_(self.leftList.mapToGlobal(pos))
            if action == actNew:
                self._onNewMap()
            return

        if self.leftList.currentItem() != item:
            self.leftList.setCurrentItem(item)
            self._onLeftItemClicked(item)

        actLabel = Locale.getContent("MAPLIST_EDIT")
        actEdit = menu.addAction(actLabel)
        menu.addAction(self._actCopyMap)
        menu.addAction(self._actDeleteMap)
        action = menu.exec_(self.leftList.mapToGlobal(pos))
        if action == actEdit:
            self._onEditMap(item.text())

    def _getNewMapFileName(self) -> str:
        existing = set(Data.GameData.mapData.keys())
        i = 1
        while True:
            name = f"Map_{i:02d}.dat"
            if name not in existing:
                return name
            i += 1

    def _onCopyMap(self) -> None:
        item = self.leftList.currentItem()
        if not item:
            return
        mapName = item.text()
        if mapName in Data.GameData.mapData:
            self._mapClipboard = copy.deepcopy(Data.GameData.mapData[mapName])
            self._mapClipboard["__source_file__"] = mapName
            self._actPasteMap.setEnabled(True)

    def _onPasteMap(self) -> None:
        if not self._mapClipboard:
            return

        newMapData = copy.deepcopy(self._mapClipboard)
        sourceFile = newMapData.pop("__source_file__", None)

        if "mapName" in newMapData:
            newMapData["mapName"] += " (copy)"

        if sourceFile:
            base, ext = os.path.splitext(sourceFile)
            newFileName = f"{base} (copy){ext}"
            if newFileName in Data.GameData.mapData:
                i = 1
                while True:
                    testName = f"{base} (copy) ({i}){ext}"
                    if testName not in Data.GameData.mapData:
                        newFileName = testName
                        break
                    i += 1
        else:
            newFileName = self._getNewMapFileName()

        Data.GameData.recordSnapshot()
        Data.GameData.mapData[newFileName] = newMapData

        self.refreshLeftList()
        self.setWindowTitle(System.getTitle())
        self._refreshUndoRedo()

        items = self.leftList.findItems(newFileName, QtCore.Qt.MatchExactly)
        if items:
            self.leftList.setCurrentItem(items[0])
            self._onLeftItemClicked(items[0])

    def _onDeleteMapAction(self) -> None:
        item = self.leftList.currentItem()
        if item:
            self._onDeleteMap(item.text())

    def _onDeleteMap(self, mapName: str) -> None:
        if mapName not in Data.GameData.mapData:
            return

        Data.GameData.recordSnapshot()
        del Data.GameData.mapData[mapName]

        self.refreshLeftList()
        self.setWindowTitle(System.getTitle())
        self._refreshUndoRedo()

        item = self.leftList.currentItem()
        if item:
            self._onLeftItemClicked(item)
        else:
            self.editorPanel.refreshMap(None)
            self._selectedLayerName = None
            self.editorPanel.setSelectedLayer(None)
            self.tileSelect.setLayerSelected(False)
            self.tileSelect.clearSelection()
            self._refreshLayerBar()

    def _onNewMap(self) -> None:
        default_data = {
            "mapName": Locale.getContent("NEW_MAP_DEFAULT_NAME"),
            "width": 20,
            "height": 15,
            "ambientLight": [255, 255, 255, 255],
            "layers": {},
        }
        suggested_name = self._getNewMapFileName()
        dlg = MapEditDialog(self, default_data, suggested_name, Locale.getContent("NEW_MAP"))
        if not dlg.execApply():
            return

        filename = dlg.getFileName()
        Data.GameData.mapData[filename] = default_data
        self.refreshLeftList()

        items = self.leftList.findItems(filename, QtCore.Qt.MatchExactly)
        if items:
            self.leftList.setCurrentItem(items[0])
            self._onLeftItemClicked(items[0])

        self.setWindowTitle(System.getTitle())
        self._refreshUndoRedo()

    def _setStyle(self) -> None:
        central = QtWidgets.QWidget(self)
        self.setCentralWidget(central)

        self.toolbar = self.addToolBar("MainToolbar")
        self.toolbar.setIconSize(QtCore.QSize(16, 16))
        self.toolbar.setMovable(False)
        self.toolbar.addAction(self._actNewProject)
        self.toolbar.addAction(self._actOpenProject)
        self.toolbar.addAction(self._actSave)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self._actUndo)
        self.toolbar.addAction(self._actRedo)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self._actGameSettings)

        self.topBar.setMinimumHeight(32)
        topLayout = QtWidgets.QHBoxLayout(self.topBar)
        topLayout.setContentsMargins(0, 0, 0, 0)
        self.layerList.setFlow(QtWidgets.QListView.LeftToRight)
        self.layerList.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Fixed)
        self.layerList.setFixedHeight(32)
        self.layerList.setGridSize(QSize(72, 32))
        self.layerList.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self.layerList.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.layerList.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.layerList.customContextMenuRequested.connect(self._onLayerContextMenu)
        self.layerList.itemClicked.connect(self._onLayerButtonClicked)
        self.layerList.setDragDropMode(QtWidgets.QAbstractItemView.InternalMove)
        self.layerList.model().rowsMoved.connect(self._onLayerMoved)
        panelW, panelH = 640, 480

        self.editorPanel.setObjectName("EditorPanel")
        self.editorPanel.setAttribute(QtCore.Qt.WA_NativeWindow, True)
        self.editorPanel.setAutoFillBackground(True)
        pal = self.editorPanel.palette()
        pal.setColor(QtGui.QPalette.Window, QtGui.QColor.fromRgb(0, 0, 0))
        self.editorPanel.setPalette(pal)
        self.topBar.setMinimumHeight(32)

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

        self.gamePanel.setFixedSize(panelW, panelH)
        self.gamePanel.setObjectName("GamePanel")
        self.gamePanel.setAttribute(QtCore.Qt.WA_NativeWindow, True)
        self.gamePanel.setAutoFillBackground(True)
        pal = self.gamePanel.palette()
        pal.setColor(QtGui.QPalette.Window, QtGui.QColor.fromRgb(0, 0, 0))
        self.gamePanel.setPalette(pal)

        topLayout.addWidget(self.layerList, 1)
        topLayout.addWidget(self.editModeToggle, 0, alignment=QtCore.Qt.AlignRight)
        topLayout.addWidget(self.modeToggle, 0, alignment=QtCore.Qt.AlignRight)
        self.editModeToggle.selectionChanged.connect(self._onEditModeChanged)
        self.modeToggle.selectionChanged.connect(self._onModeChanged)
        self._menuBar.setNativeMenuBar(True)

        self.leftList.setMinimumWidth(320)
        self.leftList.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.refreshLeftList()
        self.leftList.itemClicked.connect(self._onLeftItemClicked)
        self.leftList.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.leftList.customContextMenuRequested.connect(self._onLeftListContextMenu)
        self.leftList.addAction(self._actCopyMap)
        self.leftList.addAction(self._actPasteMap)
        self.leftList.addAction(self._actDeleteMap)

        self.leftLabel.setAlignment(QtCore.Qt.AlignCenter)
        self.leftLabel.setSizePolicy(QtWidgets.QSizePolicy.Preferred, QtWidgets.QSizePolicy.Fixed)
        self.leftLabel.setFixedHeight(32)
        _lh = 32
        _font = self.leftLabel.font()
        _font.setBold(True)
        _font.setPixelSize(max(12, int(_lh * 0.6)))
        self.leftLabel.setFont(_font)
        leftLayout = QtWidgets.QVBoxLayout(self.leftArea)
        leftLayout.setContentsMargins(0, 0, 0, 0)
        leftLayout.setSpacing(0)
        leftLayout.addWidget(self.leftLabel, 0, alignment=QtCore.Qt.AlignHCenter)
        leftLayout.addWidget(self.leftList, 1)
        self.leftArea.setMinimumWidth(320)

        centerLayout = QtWidgets.QVBoxLayout(self.centerArea)
        centerLayout.setContentsMargins(0, 0, 0, 0)
        centerLayout.setSpacing(0)
        centerLayout.addWidget(self.topBar, 0, alignment=QtCore.Qt.AlignTop)
        self.stacked.addWidget(self.editorScroll)
        self.stacked.addWidget(self.gamePanel)
        self.stacked.setCurrentWidget(self.editorScroll)
        centerLayout.addLayout(self.stacked)
        centerLayout.addStretch(1)
        self.centerArea.setFixedWidth(self.gamePanel.width())

        self.rightArea.setMinimumWidth(320)
        rightLayout = QtWidgets.QVBoxLayout(self.rightArea)
        rightLayout.setContentsMargins(0, 0, 0, 0)
        rightLayout.setSpacing(0)
        rightLayout.addWidget(self.tileSelect, 1)
        self.tileSelect.tileSelected.connect(self._onTileSelected)
        self.tileSelect.tilesetChanged.connect(self._onTilesetChanged)
        self.editorPanel.tileNumberPicked.connect(self._onTileNumberPicked)

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
        cfg_path = os.path.join(File.getIniPath(), f"{EditorStatus.APP_NAME}.ini")
        if os.path.exists(cfg_path):
            cfg.read(cfg_path)
            if EditorStatus.APP_NAME in cfg:
                ls = cfg[EditorStatus.APP_NAME].get("UpperLeftWidth")
                rs = cfg[EditorStatus.APP_NAME].get("UpperRightWidth")
                if ls and rs:
                    self._savedLeftWidth = max(320, int(ls))
                    self._savedRightWidth = max(320, int(rs))

        lowerLayout = QtWidgets.QVBoxLayout(self.lowerArea)
        lowerLayout.setContentsMargins(0, 0, 0, 0)
        lowerLayout.setSpacing(0)
        self.tabWidget.setTabPosition(QtWidgets.QTabWidget.North)
        self.tabWidget.setTabBarAutoHide(False)
        self.tabWidget.addTab(self.fileExplorer, Locale.getContent("FILE_EXPLORER"))
        self.tabWidget.addTab(self.consoleWidget, Locale.getContent("CONSOLE"))
        lowerLayout.addWidget(self.tabWidget)

        self.topSplitter.setChildrenCollapsible(False)
        self.topSplitter.addWidget(self.upperSplitter)
        self.topSplitter.addWidget(self.lowerArea)
        topH = self.topBar.minimumHeight() + self.gamePanel.height()
        self.upperSplitter.setFixedHeight(topH)
        self.lowerArea.setMinimumHeight(160)

        minW = 320 + self.gamePanel.width() + 320 + self.upperSplitter.handleWidth() * 2 + 16
        minH = topH + 160 + 8
        self.setMinimumSize(minW, minH)

        layout = QtWidgets.QVBoxLayout(central)
        layout.setContentsMargins(8, 0, 8, 8)
        layout.setSpacing(0)
        layout.addWidget(self.topSplitter)

        self._setTopMenu()

    def _setTopMenu(self) -> None:
        _fileMenu = self._menuBar.addMenu(Locale.getContent("FILE"))
        self._actNewProject.setShortcut(QtGui.QKeySequence.StandardKey.New)
        self._actNewProject.triggered.connect(self._onNewProject)
        self._actOpenProject.setShortcut(QtGui.QKeySequence.StandardKey.Open)
        self._actOpenProject.triggered.connect(self._onOpenProject)
        self._actSave.setShortcut(QtGui.QKeySequence.StandardKey.Save)
        self._actSave.triggered.connect(self._onSave)
        self._actExit.setShortcut(QtGui.QKeySequence.StandardKey.Close)
        self._actExit.triggered.connect(self._onExit)
        _fileMenu.addAction(self._actNewProject)
        _fileMenu.addAction(self._actOpenProject)
        _fileMenu.addAction(self._actSave)
        _fileMenu.addAction(self._actExit)

        _editMenu = self._menuBar.addMenu(Locale.getContent("EDIT"))
        self._actUndo.setShortcut(QtGui.QKeySequence.StandardKey.Undo)
        self._actUndo.triggered.connect(self._onUndo)
        self._actRedo.setShortcut(QtGui.QKeySequence.StandardKey.Redo)
        self._actRedo.triggered.connect(self._onRedo)
        _editMenu.addAction(self._actUndo)
        _editMenu.addAction(self._actRedo)

        _gameMenu = self._menuBar.addMenu(Locale.getContent("GAME"))
        self._actGameSettings.triggered.connect(self._onGameSettings)
        self._actGameSettings.setShortcut(QtGui.QKeySequence("F4"))
        _gameMenu.addAction(self._actGameSettings)

        _dbMenu = self._menuBar.addMenu(Locale.getContent("DATABASE"))
        self._actDatabaseSystemConfig.triggered.connect(self._onDatabaseSystemConfig)
        self._actDatabaseSystemConfig.setShortcut(QtGui.QKeySequence("F8"))
        self._actDatabaseTilesetsData.triggered.connect(self._onDatabaseTilesetsData)
        self._actDatabaseTilesetsData.setShortcut(QtGui.QKeySequence("F9"))
        self._actDatabaseCommonFunctions.triggered.connect(self._onDatabaseCommonFunctions)
        self._actDatabaseCommonFunctions.setShortcut(QtGui.QKeySequence("F10"))
        self._actDatabaseScripts.triggered.connect(self._onDatabaseScripts)
        self._actDatabaseScripts.setShortcut(QtGui.QKeySequence("F11"))
        _dbMenu.addAction(self._actDatabaseSystemConfig)
        _dbMenu.addAction(self._actDatabaseTilesetsData)
        _dbMenu.addAction(self._actDatabaseCommonFunctions)
        _dbMenu.addAction(self._actDatabaseScripts)

        _helpMenu = self._menuBar.addMenu(Locale.getContent("HELP"))
        self._actHelpExplanation.triggered.connect(self._onHelpExplanation)
        self._actHelpExplanation.setShortcut(QtGui.QKeySequence("F1"))
        _helpMenu.addAction(self._actHelpExplanation)

    def _refreshUndoRedo(self) -> None:
        self._actUndo.setEnabled(bool(Data.GameData.undoStack))
        self._actRedo.setEnabled(bool(Data.GameData.redoStack))

    def _onEditMap(self, mapKey: str) -> None:
        data = Data.GameData.mapData.get(mapKey)
        if data is None:
            fp = os.path.join(self._mapFilesRoot, mapKey)
            if os.path.exists(fp):
                data = File.loadData(fp)
                Data.GameData.mapData[mapKey] = data
        if not isinstance(data, dict):
            return

        was_active = self.leftList.currentItem() and self.leftList.currentItem().text() == mapKey

        dlg = MapEditDialog(self, data, mapKey)
        if not dlg.execApply():
            return

        newKey = dlg.getFileName()
        self.setWindowTitle(System.getTitle())

        if newKey != mapKey:
            self.refreshLeftList()

        if was_active:
            items = self.leftList.findItems(newKey, QtCore.Qt.MatchExactly)
            if items:
                self.leftList.setCurrentItem(items[0])
                self.editorPanel.refreshMap(newKey)
                self._refreshLayerBar()

    def _refreshLayerBar(self) -> None:
        self.layerList.clear()
        names = self.editorPanel.getLayerNames()
        for n in names:
            item = QtWidgets.QListWidgetItem(n)
            self.layerList.addItem(item)
            if n == self._selectedLayerName:
                self.layerList.setCurrentItem(item)

    def _clearLayerSelection(self) -> None:
        if self._selectedLayerName is None:
            return
        self.layerList.clearSelection()
        self._selectedLayerName = None
        self.editorPanel.setSelectedLayer(None)
        self.tileSelect.setLayerSelected(False)
        self.tileSelect.clearSelection()

    def _onLayerButtonClicked(self, item: QtWidgets.QListWidgetItem, force_select: bool = False) -> None:
        name = item.text()
        if not force_select and name == self._selectedLayerName:
            self._clearLayerSelection()
            return
        self._selectedLayerName = name
        self.editorPanel.setSelectedLayer(name)
        key = self.editorPanel.getLayerTilesetKey(name)
        if key:
            self.tileSelect.setCurrentTilesetKey(key)
        self.tileSelect.setLayerSelected(True)

    def _onLayerMoved(self, parent, start, end, destination, row) -> None:
        new_order = [self.layerList.item(i).text() for i in range(self.layerList.count())]
        self.editorPanel.reorderLayers(new_order)
        selected_items = self.layerList.selectedItems()
        if selected_items:
            self._onLayerButtonClicked(selected_items[0], force_select=True)

    def _onAddLayer(self, checked: bool = False) -> None:
        if self.editorPanel.mapData is None:
            QtWidgets.QMessageBox.warning(self, "Hint", Locale.getContent("ADD_ERROR"))
            return
        existing = set(self.editorPanel.getLayerNames())
        while True:
            dlg = SingleRowDialog(self, Locale.getContent("ADD_LAYER"), Locale.getContent("ADD_MESSAGE"))
            ok, name = dlg.execGetText()
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
        bar = self.layerList.horizontalScrollBar()
        bar.setValue(bar.maximum())

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
        item = self.layerList.itemAt(pos)
        if item is None:
            menu = QtWidgets.QMenu(self)
            actAdd = menu.addAction(Locale.getContent("ADD_LAYER"))
            action = menu.exec_(self.layerList.mapToGlobal(pos))
            if action == actAdd:
                self._onAddLayer()
            return
        self.layerList.setCurrentItem(item)
        self._onLayerButtonClicked(item, force_select=True)

        name = item.text()
        menu = QtWidgets.QMenu(self)
        actRename = menu.addAction(Locale.getContent("RENAME_LAYER"))
        actDelete = menu.addAction(Locale.getContent("DELETE"))
        action = menu.exec_(self.layerList.mapToGlobal(pos))
        if action == actRename:
            existing = set(self.editorPanel.getLayerNames())
            if name in existing:
                existing.remove(name)
            while True:
                dlg = SingleRowDialog(
                    self, Locale.getContent("RENAME_LAYER"), Locale.getContent("RENAME_MESSAGE"), str(name)
                )
                ok, newName = dlg.execGetText()
                if not ok:
                    return
                newName = newName.strip()
                if not newName:
                    QtWidgets.QMessageBox.warning(self, "Hint", Locale.getContent("ADD_EMPTY"))
                    continue
                if newName in existing:
                    QtWidgets.QMessageBox.warning(self, "Hint", Locale.getContent("ADD_DUPLICATE"))
                    continue
                break
            if self.editorPanel.renameLayer(name, newName):
                if self._selectedLayerName == name:
                    self._selectedLayerName = newName
                self._refreshLayerBar()
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

    def _getExec(self, scriptPath):
        individual = str(self._projConfig.get("IndividualWindow", False))
        if System.already_packed():
            return [sys.argv[0], scriptPath, str(self._panelHandle), individual]
        return [sys.executable, "-u", scriptPath, str(self._panelHandle), individual]

    def _initProjConfigAndSelection(self) -> None:
        root = EditorStatus.PROJ_PATH
        chosen = None
        try:
            if os.path.exists(os.path.join(root, "Main.proj")):
                chosen = os.path.join(root, "Main.proj")
            else:
                chosen = os.path.join(root, "Main.proj")
                with open(chosen, "w", encoding="utf-8") as f:
                    f.write("{}")
        except Exception as e:
            print(f"Error while initializing project config {chosen}: {e}")
        self._projConfigPath = chosen
        self._projConfig = {}
        if chosen and os.path.exists(chosen):
            try:
                with open(chosen, "r", encoding="utf-8") as f:
                    content = f.read().strip()
                    if content:
                        self._projConfig = json.loads(content)
            except Exception as e:
                print(f"Error while loading project config {chosen}: {e}")
                self._projConfig = {}

        modified = False
        if "IndividualWindow" not in self._projConfig:
            self._projConfig["IndividualWindow"] = False
            modified = True

        if sys.platform == "darwin":
            if not self._projConfig["IndividualWindow"]:
                self._projConfig["IndividualWindow"] = True
                modified = True

        if modified and chosen:
            try:
                with open(chosen, "w", encoding="utf-8") as f:
                    json.dump(self._projConfig, f, ensure_ascii=False, indent=4)
            except Exception as e:
                print(f"Error saving project config: {e}")

        last = None
        if isinstance(self._projConfig, dict):
            last = self._projConfig.get("lastMap")
        targetRow = 0 if self.leftList.count() > 0 else -1
        if last:
            for i in range(self.leftList.count()):
                it = self.leftList.item(i)
                if it and it.text() == last:
                    targetRow = i
                    break
        if targetRow >= 0:
            self.leftList.setCurrentRow(targetRow)
            item = self.leftList.item(targetRow)
            if item:
                self._onLeftItemClicked(item)

    def _saveProjLastMap(self) -> None:
        if not self._projConfigPath:
            return
        name = None
        item = self.leftList.currentItem()
        if item:
            name = item.text()
        if name:
            data = {}
            if isinstance(getattr(self, "_projConfig", {}), dict):
                data.update(self._projConfig)
            data["lastMap"] = name
            with open(self._projConfigPath, "w", encoding="utf-8") as f:
                json.dump(data, f, ensure_ascii=False)

    def _onNewProject(self, checked: bool = False) -> None:
        File.NewProject(self)

    def _onOpenProject(self, checked: bool = False) -> None:
        File.OpenProject(self)

    def _onSave(self, checked: bool = False) -> None:
        ok, content = Data.GameData.saveAllModified()
        if ok:
            QtWidgets.QMessageBox.information(
                self, "Hint", Locale.getContent("SAVE_SUCCESS") + Locale.getContent("SAVE_PATH").format(content)
            )
        else:
            QtWidgets.QMessageBox.warning(
                self, "Hint", Locale.getContent("SAVE_FAILED") + Locale.getContent("SAVE_PATH").format(content)
            )
        self.setWindowTitle(System.getTitle())

    def _onExit(self, checked: bool = False) -> None:
        self.close()

    def _onUndo(self, checked: bool = False) -> None:
        diffs = Data.GameData.undo()
        self._refreshCurrentView()
        if diffs:
            self.toast.showMessage("Undo:\n" + "\n".join(diffs))

    def _onRedo(self, checked: bool = False) -> None:
        diffs = Data.GameData.redo()
        self._refreshCurrentView()
        if diffs:
            self.toast.showMessage("Redo:\n" + "\n".join(diffs))

    def _refreshCurrentView(self):
        self.refreshLeftList()
        self.tileSelect.initTilesets()
        self.setWindowTitle(System.getTitle())
        self._refreshUndoRedo()
        if self.stacked.currentWidget() == self.editorScroll:
            item = self.leftList.currentItem()
            if item:
                self.editorPanel.refreshMap(item.text())
                self._refreshLayerBar()

    def _onDatabaseSystemConfig(self, checked: bool = False) -> None:
        self._configWindow = ConfigWindow(self)
        self._configWindow.modified.connect(lambda: (self.setWindowTitle(System.getTitle()), self._refreshUndoRedo()))
        self._configWindow.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self._configWindow.setWindowModality(QtCore.Qt.ApplicationModal)
        self._configWindow.activateWindow()
        self._configWindow.raise_()
        self._configWindow.show()

    def _onDatabaseTilesetsData(self, checked: bool = False) -> None:
        self._tilesetEditor = TilesetEditor(self)
        self._tilesetEditor.modified.connect(lambda: (self.setWindowTitle(System.getTitle()), self._refreshUndoRedo()))
        self._tilesetEditor.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self._tilesetEditor.setWindowModality(QtCore.Qt.ApplicationModal)
        self._tilesetEditor.show()

    def _onDatabaseCommonFunctions(self, checked: bool = False) -> None:
        pass

    def _onDatabaseScripts(self, checked: bool = False) -> None:
        pass

    def _onHelpExplanation(self, checked: bool = False) -> None:
        pass

    def _onGameSettings(self, checked: bool = False) -> None:
        self._settingsWindow = SettingsWindow(self, self._projConfig)
        self._settingsWindow.modified.connect(lambda: self.setWindowTitle(System.getTitle()))
        self._settingsWindow.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self._settingsWindow.setWindowModality(QtCore.Qt.ApplicationModal)
        self._settingsWindow.activateWindow()
        self._settingsWindow.raise_()
        self._settingsWindow.show()
