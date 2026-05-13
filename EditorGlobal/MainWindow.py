# -*- encoding: utf-8 -*-

import os
import configparser
import subprocess
from typing import Any, Dict, Optional, cast
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtCore import QSize
from Utils import File
from Widgets import (
    GamePanel,
    EditorPanel,
    ModeToggle,
    EditModeToggle,
    TileSelect,
    FileExplorer,
    ConsoleWidget,
    LightPanel,
    ActorInfoPanel,
    GeneralDataEditor,
    ActorQueuePanel,
)
from Widgets.Utils import Toast
from . import EditorStatus, GameData
from .MainUtils import (
    GameRunnerMixin,
    MapListOpsMixin,
    LayerBarMixin,
    LightActorMixin,
    MenuBuilderMixin,
    DatabaseMenuMixin,
    ProjectConfigMixin,
    LayoutMixin,
)


class MainWindow(
    LayoutMixin,
    MenuBuilderMixin,
    DatabaseMenuMixin,
    ProjectConfigMixin,
    MapListOpsMixin,
    LayerBarMixin,
    LightActorMixin,
    GameRunnerMixin,
    QtWidgets.QMainWindow,
):
    DEFAULT_LEFT_PANEL_MIN_WIDTH = 160
    DEFAULT_RIGHT_PANEL_MIN_WIDTH = 320
    DEFAULT_LOWER_AREA_MIN_HEIGHT = 160

    def __init__(self, title: str):
        super().__init__()
        self.toast = Toast(self)
        self._engineProc: Optional[subprocess.Popen] = None
        self.setWindowTitle(title)
        wstyle = cast(QtWidgets.QStyle, self.style())

        self.topBar = QtWidgets.QWidget()
        self.layerList = QtWidgets.QTabWidget()
        self._selectedLayerName: Optional[str] = None
        self.editorPanel = EditorPanel()
        self.editorPanel.DATA_CHANGED.connect(self._refreshUndoRedo)
        self.editorScroll = QtWidgets.QScrollArea()
        self.gamePanel = GamePanel()
        self.gamePanel.setFocusPolicy(QtCore.Qt.StrongFocus)
        self._panelHandle = int(self.gamePanel.winId())
        self.editModeToggle = EditModeToggle()
        self.modeToggle = ModeToggle()
        self._menuBar = cast(QtWidgets.QMenuBar, self.menuBar())
        self.leftListIndex = -1
        self.leftList = QtWidgets.QListWidget()
        self.leftLabel = QtWidgets.QLabel(ELOC("MAP_LIST"))
        self.leftArea = QtWidgets.QWidget()
        self.centerArea = QtWidgets.QWidget()
        self.stacked = QtWidgets.QStackedLayout()
        self.rightArea = QtWidgets.QWidget()
        self.tileSelect = TileSelect(self.rightArea)
        self.lightPanel = LightPanel(self.rightArea)
        self.actorInfo = ActorInfoPanel(self.rightArea)
        self._selectedLightMapKey = ""
        self._selectedLightIndex: Optional[int] = None
        self.upperSplitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        self._savedLeftWidth: Optional[int] = None
        self._savedRightWidth: Optional[int] = None
        self.lowerArea = QtWidgets.QWidget()
        self.fileExplorer = FileExplorer(EditorStatus.PROJ_PATH)
        self.actorQueuePanel = ActorQueuePanel(dockMode="vertical")
        self.consoleWidget = ConsoleWidget()
        self.tabWidget = QtWidgets.QTabWidget()
        self.lowerSplitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        self.topSplitter = QtWidgets.QSplitter(QtCore.Qt.Vertical)
        self._sizesInitialized = False
        self._hasShown = False
        self.generalDataEditor: Optional[GeneralDataEditor] = None
        self._actNewProject = QtWidgets.QAction(ELOC("NEW_PROJECT"), self)
        self._actNewProject.setIcon(wstyle.standardIcon(QtWidgets.QStyle.SP_FileIcon))
        self._actOpenProject = QtWidgets.QAction(ELOC("OPEN_PROJECT"), self)
        self._actOpenProject.setIcon(wstyle.standardIcon(QtWidgets.QStyle.SP_DialogOpenButton))
        self._actSave = QtWidgets.QAction(ELOC("SAVE"), self)
        self._actSave.setIcon(wstyle.standardIcon(QtWidgets.QStyle.SP_DialogSaveButton))
        self.packAction = QtWidgets.QAction(ELOC("PACK_PROJECT"), self)
        self._actExit = QtWidgets.QAction(ELOC("EXIT"), self)
        self._actUndo = QtWidgets.QAction(ELOC("UNDO"), self)
        self._actUndo.setIcon(wstyle.standardIcon(QtWidgets.QStyle.SP_ArrowBack))
        self._actRedo = QtWidgets.QAction(ELOC("REDO"), self)
        self._actRedo.setIcon(wstyle.standardIcon(QtWidgets.QStyle.SP_ArrowForward))
        self._actReloadModule = QtWidgets.QAction(ELOC("RELOAD_MODULE"), self)
        self._actGameSettings = QtWidgets.QAction(ELOC("GAME_SETTINGS"), self)
        self._actGameSettings.setIcon(wstyle.standardIcon(QtWidgets.QStyle.SP_FileDialogDetailedView))
        self._actGameConfig = QtWidgets.QAction(ELOC("GAME_CONFIG"), self)
        self._actGameConfig.setIcon(wstyle.standardIcon(QtWidgets.QStyle.SP_FileDialogInfoView))
        self._actNewBlueprint = QtWidgets.QAction(ELOC("NEW_BLUEPRINT"), self)
        self._actNewAnimation = QtWidgets.QAction(ELOC("NEW_ANIMATION"), self)
        self._actDatabaseSystemConfig = QtWidgets.QAction(ELOC("SYSTEM_CONFIG"), self)
        self._actDatabaseTilesetsData = QtWidgets.QAction(ELOC("TILESETS_DATA"), self)
        self._actDatabaseCommonFunctions = QtWidgets.QAction(ELOC("COMMON_FUNCTIONS"), self)
        self._actDatabaseGeneralData = QtWidgets.QAction(ELOC("GENERAL_DATA"), self)
        self._actDatabaseGeneralData.setShortcut(QtGui.QKeySequence("F11"))
        self._actDatabaseGeneralData.triggered.connect(self._onGeneralDataEditor)
        self._actHelpExplanation = QtWidgets.QAction(ELOC("HELP_EXPLANATION"), self)
        self._languageActionGroup = QtWidgets.QActionGroup(self)
        self._actDatabaseExportLocale = QtWidgets.QAction(ELOC("EXPORT_LOCALE"), self)
        self._actDatabaseExportLocale.setShortcut(QtGui.QKeySequence("F12"))
        self._actDatabaseExportLocale.triggered.connect(self._onDatabaseExportLocale)

        self._mapClipboard = None
        self._actCopyMap = QtWidgets.QAction(ELOC("COPY"), self)
        self._actCopyMap.setShortcut(QtGui.QKeySequence.Copy)
        self._actCopyMap.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actCopyMap.triggered.connect(self._onCopyMap)

        self._actPasteMap = QtWidgets.QAction(ELOC("PASTE"), self)
        self._actPasteMap.setShortcut(QtGui.QKeySequence.Paste)
        self._actPasteMap.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actPasteMap.triggered.connect(self._onPasteMap)
        self._actPasteMap.setEnabled(False)

        self._actDeleteMap = QtWidgets.QAction(ELOC("DELETE"), self)
        self._actDeleteMap.setShortcut(QtGui.QKeySequence.Delete)
        self._actDeleteMap.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actDeleteMap.triggered.connect(self._onDeleteMapAction)

        self._actEditMap = QtWidgets.QAction(ELOC("MAPLIST_EDIT"), self)
        self._actEditMap.setShortcuts([QtGui.QKeySequence("Return"), QtGui.QKeySequence("Enter")])
        self._actEditMap.setShortcutContext(QtCore.Qt.WidgetShortcut)

        self._actEditMap.triggered.connect(self._onEditCurrentMap)

        self._editModeIdx = 0
        self._lastEditorPanelContextPos: Optional[QtCore.QPoint] = None
        self._actNewLightSource = QtWidgets.QAction(ELOC("NEW_LIGHT_SOURCE"), self)
        self._actNewLightSource.triggered.connect(self._onNewLightSource)
        self._actNewLightSource.setEnabled(False)

        self._actPasteLightSource = QtWidgets.QAction(ELOC("PASTE"), self)
        self._actPasteLightSource.setShortcut(QtGui.QKeySequence.Paste)
        self._actPasteLightSource.setShortcutContext(QtCore.Qt.WidgetShortcut)
        self._actPasteLightSource.triggered.connect(self._onPasteLightSource)
        self._actPasteLightSource.setEnabled(False)

        self.gameSize = QSize(640, 480)
        self.panelAspectRatio = 4.0 / 3.0
        self.refreshGameSize()
        self.setProjPath(EditorStatus.PROJ_PATH)
        self._setStyle()
        self._initProjConfigAndSelection()
        self._engineMonitorTimer: Optional[QtCore.QTimer] = None
        self._gameLockActive: bool = False
        self._lockedViewportSize: Optional[QSize] = None
        self._gameConfigModified: bool = False
        self._pendingGameConfig: Optional[Dict[str, Any]] = None

    def _layerTabBar(self) -> QtWidgets.QTabBar:
        tabBar = self.layerList.tabBar()
        if tabBar:
            return tabBar
        tabBar = QtWidgets.QTabBar(self.layerList)
        self.layerList.setTabBar(tabBar)
        return tabBar

    def refreshGameSize(self) -> None:
        panelW, panelH = 640, 480
        cfg = GameData.systemConfigData.get("System")
        if isinstance(cfg, dict):
            gs = cfg.get("gameSize")
            val = gs.get("value") if isinstance(gs, dict) else gs
            if isinstance(val, (list, tuple)) and len(val) >= 2:

                def toPositiveInt(v: Any) -> Optional[int]:
                    if isinstance(v, bool):
                        return None
                    if isinstance(v, int):
                        return v if v > 0 else None
                    if isinstance(v, float):
                        i = int(v)
                        return i if i > 0 else None
                    if isinstance(v, str):
                        s = v.strip()
                        if s.isdigit():
                            i = int(s)
                            return i if i > 0 else None
                    return None

                w = toPositiveInt(val[0])
                h = toPositiveInt(val[1])
                if isinstance(w, int) and isinstance(h, int):
                    panelW, panelH = w, h
        self.gameSize = QSize(panelW, panelH)
        self.panelAspectRatio = (float(panelW) / float(panelH)) if panelH > 0 else (4.0 / 3.0)
        if hasattr(self, "editorViewport"):
            self.editorViewport.setAspectRatio(self.panelAspectRatio)
            self.editorViewport.setMinimumSize(self.gameSize)
        if hasattr(self, "gameViewport"):
            self.gameViewport.setAspectRatio(self.panelAspectRatio)
            self.gameViewport.setMinimumSize(self.gameSize)
        if hasattr(self, "editorScroll"):
            self.editorScroll.setMinimumSize(self.gameSize)
        if hasattr(self, "gamePanel"):
            self.gamePanel.setMinimumSize(self.gameSize)
        if hasattr(self, "centerArea"):
            self.centerArea.setMinimumWidth(self.gameSize.width())
        if hasattr(self, "upperSplitter") and hasattr(self, "topBar") and hasattr(self, "lowerArea"):
            minLeft = self.DEFAULT_LEFT_PANEL_MIN_WIDTH
            minRight = self.DEFAULT_RIGHT_PANEL_MIN_WIDTH
            if hasattr(self, "leftArea"):
                minLeft = max(minLeft, int(self.leftArea.minimumWidth()))
            if hasattr(self, "rightArea"):
                minRight = max(minRight, int(self.rightArea.minimumWidth()))
            lowerMinH = max(self.DEFAULT_LOWER_AREA_MIN_HEIGHT, int(self.lowerArea.minimumHeight()))
            topHMin = int(self.topBar.minimumHeight()) + int(self.gameSize.height())
            handleW = int(self.upperSplitter.handleWidth())
            minW = minLeft + int(self.gameSize.width()) + minRight + handleW * 2 + 16
            minH = topHMin + lowerMinH + 8
            self.setMinimumSize(minW, minH)

    def showEvent(self, event: QtGui.QShowEvent) -> None:
        self._refreshUndoRedo()
        super().showEvent(event)
        applyLW = self._savedLeftWidth
        applyRW = self._savedRightWidth
        if applyLW is not None and applyRW is not None:
            totalW = self.upperSplitter.width()
            minCenterW = max(self.gameSize.width(), self.centerArea.minimumWidth())
            cw = max(minCenterW, totalW - applyLW - applyRW)
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
        self.toast._updatePosition()
        if self._hasShown:
            cfg = configparser.ConfigParser()
            cfg_path = os.path.join(File.GetIniPath(), f"{EditorStatus.APP_NAME}.ini")
            if os.path.exists(cfg_path):
                cfg.read(cfg_path)
            if EditorStatus.APP_NAME not in cfg:
                cfg[EditorStatus.APP_NAME] = {}
            s = self.size()
            cfg[EditorStatus.APP_NAME]["Width"] = str(s.width())
            cfg[EditorStatus.APP_NAME]["Height"] = str(s.height())
            sizes = self.upperSplitter.sizes()
            if len(sizes) >= 3:
                self._prevLeftW = sizes[0]
                self._prevRightW = sizes[2]
            cfg[EditorStatus.APP_NAME]["UpperLeftWidth"] = str(self._prevLeftW or self.DEFAULT_LEFT_PANEL_MIN_WIDTH)
            cfg[EditorStatus.APP_NAME]["UpperRightWidth"] = str(self._prevRightW or self.DEFAULT_RIGHT_PANEL_MIN_WIDTH)
            with open(cfg_path, "w") as f:
                cfg.write(f)

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        if not self._checkUnsavedChanges():
            event.ignore()
            return
        self._saveProjLastMap()
        self.endGame(showFPS=False)
        super().closeEvent(event)

    def getPanelHandle(self) -> int:
        return int(self.gamePanel.winId())

    def setProjPath(self, projPath: str):
        self._mapFilesRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        self.refreshGameSize()
        if hasattr(self, "editorViewport"):
            self.stacked.setCurrentWidget(self.editorViewport)
        else:
            self.stacked.setCurrentWidget(self.editorScroll)
        self.editorPanel.refreshMap()
        self.fileExplorer.setRootPath(EditorStatus.PROJ_PATH)

    def _setLayerListInteractive(self, enabled: bool) -> None:
        from Utils import Panel

        self.layerList.setEnabled(bool(enabled))
        Panel.ApplyDisabledOpacity(self.layerList)
