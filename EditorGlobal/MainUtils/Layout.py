# -*- encoding: utf-8 -*-

import os
import configparser
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import File
from Widgets import AspectRatioContainer
from .. import EditorStatus


class LayoutMixin:
    def _setStyle(self) -> None:
        central = QtWidgets.QWidget(self)
        self.setCentralWidget(central)

        self.topBar.setMinimumHeight(32)
        topLayout = QtWidgets.QHBoxLayout(self.topBar)
        topLayout.setContentsMargins(0, 0, 0, 0)
        self.layerList.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Fixed)
        self.layerList.setFixedHeight(32)
        self.layerList.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.layerList.customContextMenuRequested.connect(self._onLayerContextMenu)
        self.layerList.currentChanged.connect(self._onLayerTabChanged)
        tabBar = self._layerTabBar()
        self.layerList.setMovable(False)
        tabBar.tabMoved.connect(self._onLayerTabMoved)
        self.layerList.setStyleSheet("QTabWidget::pane { border: 0; }")
        panelW, panelH = self.gameSize.width(), self.gameSize.height()

        self.editorPanel.setObjectName("EditorPanel")
        self.editorPanel.setAttribute(QtCore.Qt.WA_NativeWindow, True)
        self.editorPanel.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        self.editorPanel.setAutoFillBackground(False)
        self.topBar.setMinimumHeight(32)

        self.editorScroll.setWidget(self.editorPanel)
        self.editorScroll.setWidgetResizable(True)
        self.editorScroll.setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignTop)
        self.editorScroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.editorScroll.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.editorScroll.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.editorScroll.setMinimumSize(self.gameSize)
        self.editorPanel.setObjectName("EditorPanel")
        self.editorPanel.setAttribute(QtCore.Qt.WA_NativeWindow, True)
        self.editorPanel.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        self.editorPanel.setAutoFillBackground(False)

        self.gamePanel.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.gamePanel.setMinimumSize(self.gameSize)
        self.gamePanel.setObjectName("GamePanel")
        self.gamePanel.setAttribute(QtCore.Qt.WA_NativeWindow, True)
        self.gamePanel.setAutoFillBackground(True)
        pal = self.gamePanel.palette()
        pal.setColor(QtGui.QPalette.Window, QtGui.QColor.fromRgb(0, 0, 0))
        self.gamePanel.setPalette(pal)

        self.editorViewport = self.editorScroll
        self.gameViewport = AspectRatioContainer(self.gamePanel, self.panelAspectRatio)
        self.gameViewport.setMinimumSize(self.gameSize)

        topLayout.addWidget(self.layerList, 1)
        topLayout.addWidget(self.editModeToggle, 0, alignment=QtCore.Qt.AlignRight)
        topLayout.addWidget(self.modeToggle, 0, alignment=QtCore.Qt.AlignRight)
        self.editModeToggle.SELECTION_CHANGED.connect(self._onEditModeChanged)
        self.modeToggle.SELECTION_CHANGED.connect(self._onModeChanged)
        self._menuBar.setNativeMenuBar(True)

        self.leftList.setMinimumWidth(self.DEFAULT_LEFT_PANEL_MIN_WIDTH)
        self.leftList.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.refreshLeftList()
        self.leftList.itemClicked.connect(self._onLeftItemClicked)
        self.leftList.itemActivated.connect(lambda item: self._onEditMap(item.data(QtCore.Qt.UserRole)))
        self.leftList.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.leftList.customContextMenuRequested.connect(self._onLeftListContextMenu)
        self.leftList.addAction(self._actCopyMap)
        self.leftList.addAction(self._actPasteMap)
        self.leftList.addAction(self._actDeleteMap)
        self.leftList.addAction(self._actEditMap)

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
        self.leftArea.setMinimumWidth(self.DEFAULT_LEFT_PANEL_MIN_WIDTH)

        centerLayout = QtWidgets.QVBoxLayout(self.centerArea)
        centerLayout.setContentsMargins(0, 0, 0, 0)
        centerLayout.setSpacing(0)
        centerLayout.addWidget(self.topBar, 0, alignment=QtCore.Qt.AlignTop)
        self.stacked.addWidget(self.editorViewport)
        self.stacked.addWidget(self.gameViewport)
        self.stacked.setCurrentWidget(self.editorViewport)
        centerLayout.addLayout(self.stacked, 1)
        self.centerArea.setMinimumWidth(panelW)

        self.rightArea.setMinimumWidth(self.DEFAULT_RIGHT_PANEL_MIN_WIDTH)
        self.rightStack = QtWidgets.QStackedLayout(self.rightArea)
        self.rightStack.setContentsMargins(0, 0, 0, 0)
        self.rightStack.setSpacing(0)
        self.rightStack.addWidget(self.tileSelect)
        self.rightStack.addWidget(self.lightPanel)
        self.rightStack.addWidget(self.actorInfo)
        self.rightStack.setCurrentWidget(self.tileSelect)
        self.tileSelect.TILE_SELECTED.connect(self._onTileSelected)
        self.tileSelect.TILE_PATTERN_SELECTED.connect(self._onTilePatternSelected)
        self.tileSelect.TILESET_CHANGED.connect(self._onTilesetChanged)
        self.tileSelect.AUTOTILE_SELECTED.connect(self._onAutoTileSelected)
        self.editorPanel.TILE_NUMBER_PICKED.connect(self._onTileNumberPicked)
        self.editorPanel.AUTOTILE_PICKED.connect(self._onAutoTilePicked)
        self.editorPanel.LIGHT_SELECTION_CHANGED.connect(self._onLightSelectionChanged)
        self.editorPanel.LIGHT_DATA_CHANGED.connect(self._onLightDataChanged)
        self.editorPanel.ACTOR_SELECTION_CHANGED.connect(self._onActorSelectionChanged)
        self.lightPanel.LIGHT_EDITED.connect(self._onLightEdited)
        self.editorPanel.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.editorPanel.customContextMenuRequested.connect(self._onEditorPanelContextMenu)
        self.editorPanel.addAction(self._actNewLightSource)
        self.editorPanel.addAction(self._actPasteLightSource)
        self.editorPanel.addAction(self._actDeleteLightSource)

        self.upperSplitter.setChildrenCollapsible(False)
        self.upperSplitter.addWidget(self.leftArea)
        self.upperSplitter.addWidget(self.centerArea)
        self.upperSplitter.addWidget(self.rightArea)
        self.upperSplitter.setStretchFactor(0, 0)
        self.upperSplitter.setStretchFactor(1, 1)
        self.upperSplitter.setStretchFactor(2, 0)
        self.upperSplitter.setSizes([self.DEFAULT_LEFT_PANEL_MIN_WIDTH, panelW, self.DEFAULT_RIGHT_PANEL_MIN_WIDTH])
        self.upperSplitter.splitterMoved.connect(self._onUpperSplitterMoved)
        cfg = configparser.ConfigParser()
        cfg_path = os.path.join(File.GetIniPath(), f"{EditorStatus.APP_NAME}.ini")
        if os.path.exists(cfg_path):
            cfg.read(cfg_path)
            if EditorStatus.APP_NAME in cfg:
                ls = cfg[EditorStatus.APP_NAME].get("UpperLeftWidth")
                rs = cfg[EditorStatus.APP_NAME].get("UpperRightWidth")
                if ls and rs:
                    self._savedLeftWidth = max(self.DEFAULT_LEFT_PANEL_MIN_WIDTH, int(ls))
                    self._savedRightWidth = max(self.DEFAULT_RIGHT_PANEL_MIN_WIDTH, int(rs))

        lowerLayout = QtWidgets.QVBoxLayout(self.lowerArea)
        lowerLayout.setContentsMargins(0, 0, 0, 0)
        lowerLayout.setSpacing(0)
        self.tabWidget.setTabPosition(QtWidgets.QTabWidget.North)
        self.tabWidget.setTabBarAutoHide(False)
        self.tabWidget.addTab(self.fileExplorer, ELOC("FILE_EXPLORER"))
        self.tabWidget.addTab(self.consoleWidget, ELOC("CONSOLE"))
        self.lowerSplitter.setChildrenCollapsible(False)
        self.lowerSplitter.addWidget(self.actorQueuePanel)
        self.lowerSplitter.addWidget(self.tabWidget)
        self.lowerSplitter.setStretchFactor(0, 0)
        self.lowerSplitter.setStretchFactor(1, 1)
        self.lowerSplitter.setSizes([int(self.actorQueuePanel.minimumWidth()), 1])
        lowerLayout.addWidget(self.lowerSplitter)

        self.topSplitter.setChildrenCollapsible(False)
        self.topSplitter.addWidget(self.upperSplitter)
        self.topSplitter.addWidget(self.lowerArea)
        self.topSplitter.setStretchFactor(0, 1)
        self.topSplitter.setStretchFactor(1, 0)
        self.lowerArea.setMinimumHeight(self.DEFAULT_LOWER_AREA_MIN_HEIGHT)

        topHMin = self.topBar.minimumHeight() + panelH
        minW = (
            self.DEFAULT_LEFT_PANEL_MIN_WIDTH
            + panelW
            + self.DEFAULT_RIGHT_PANEL_MIN_WIDTH
            + self.upperSplitter.handleWidth() * 2
            + 16
        )
        minH = topHMin + self.DEFAULT_LOWER_AREA_MIN_HEIGHT + 8
        self.setMinimumSize(minW, minH)

        layout = QtWidgets.QVBoxLayout(central)
        layout.setContentsMargins(8, 0, 8, 8)
        layout.setSpacing(0)
        layout.addWidget(self.topSplitter)

        self._setTopMenu()

        self.fileExplorer.FILE_CLICKED.connect(self._onFileExplorerFileClicked)
        self.fileExplorer.DATA_FILE_CHANGED.connect(self._onDataFileChanged)
        self.actorQueuePanel.SELECTION_CHANGED.connect(
            lambda bpRel: self.editorPanel.setPendingActor(bpRel if isinstance(bpRel, str) else None)
        )
        self.actorQueuePanel.BLUEPRINT_OPEN_REQUESTED.connect(self._onActorQueueBlueprintOpen)
        self.actorQueuePanel.BLUEPRINT_LOCATE_REQUESTED.connect(self._onActorQueueBlueprintLocate)

    def _onUpperSplitterMoved(self, pos: int, index: int) -> None:
        sizes = self.upperSplitter.sizes()
        if len(sizes) >= 3:
            self._prevLeftW = sizes[0]
            self._prevRightW = sizes[2]
        self._prevUpperW = self.upperSplitter.width()
        cfg = configparser.ConfigParser()
        cfg_path = os.path.join(File.GetIniPath(), f"{EditorStatus.APP_NAME}.ini")
        if os.path.exists(cfg_path):
            cfg.read(cfg_path)
            if EditorStatus.APP_NAME not in cfg:
                cfg[EditorStatus.APP_NAME] = {}
            cfg[EditorStatus.APP_NAME]["UpperLeftWidth"] = str(self._prevLeftW)
            cfg[EditorStatus.APP_NAME]["UpperRightWidth"] = str(self._prevRightW)
            with open(cfg_path, "w") as f:
                cfg.write(f)

    def _clampUpperSplitterHandlePosition(self, index: int, pos: int) -> int:
        sizes = self.upperSplitter.sizes()
        if len(sizes) < 3 or index not in (1, 2):
            return int(pos)
        totalW = sum(sizes)
        minLeftW, minCenterW, minRightW = self._upperSplitterMinWidths()
        if totalW <= 0 or totalW < minLeftW + minCenterW + minRightW:
            return int(pos)

        if index == 1:
            rightW = self._lockedUpperSideWidth(
                getattr(self, "_prevRightW", sizes[2]), minRightW, totalW, minLeftW, minCenterW
            )
            minPos = minLeftW
            maxPos = max(minPos, totalW - rightW - minCenterW)
            return min(max(int(pos), minPos), maxPos)

        leftW = self._lockedUpperSideWidth(
            getattr(self, "_prevLeftW", sizes[0]), minLeftW, totalW, minCenterW, minRightW
        )
        handleW = int(self.upperSplitter.handleWidth())
        minPos = leftW + handleW + minCenterW
        maxPos = max(minPos, totalW + handleW - minRightW)
        return min(max(int(pos), minPos), maxPos)

    def _lockedUpperSideWidth(
        self,
        width: int,
        minWidth: int,
        totalWidth: int,
        firstOtherMinWidth: int,
        secondOtherMinWidth: int,
    ) -> int:
        maxWidth = max(minWidth, totalWidth - firstOtherMinWidth - secondOtherMinWidth)
        return min(max(minWidth, int(width)), maxWidth)

    def _upperSplitterMinWidths(self) -> tuple[int, int, int]:
        minLeftW = self.DEFAULT_LEFT_PANEL_MIN_WIDTH
        minCenterW = self.gameSize.width()
        minRightW = self.DEFAULT_RIGHT_PANEL_MIN_WIDTH
        leftArea = getattr(self, "leftArea", None)
        centerArea = getattr(self, "centerArea", None)
        rightArea = getattr(self, "rightArea", None)
        if isinstance(leftArea, QtWidgets.QWidget):
            minLeftW = max(minLeftW, int(leftArea.minimumWidth()))
        if isinstance(centerArea, QtWidgets.QWidget):
            minCenterW = max(minCenterW, int(centerArea.minimumWidth()))
        if isinstance(rightArea, QtWidgets.QWidget):
            minRightW = max(minRightW, int(rightArea.minimumWidth()))
        return minLeftW, minCenterW, minRightW
