# -*- encoding: utf-8 -*-

import os
import configparser
from typing import cast
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import File, Locale
from Widgets import AboutDialog, LogDialog, PackWorker, PackSelectionDialog, MarkdownPreviewer, PackPlatform, find_python_3120_for_pack, prompt_install_python_3120
from .. import EditorStatus
from ..Data import GameData


class MenuBuilderMixin:
    def _setTopMenu(self) -> None:
        _fileMenu = cast(QtWidgets.QMenu, self._menuBar.addMenu(ELOC("FILE")))
        self._actNewProject.setShortcut(QtGui.QKeySequence.StandardKey.New)
        self._actNewProject.triggered.connect(self._onNewProject)
        self._actOpenProject.setShortcut(QtGui.QKeySequence.StandardKey.Open)
        self._actOpenProject.triggered.connect(self._onOpenProject)
        self._actSave.setShortcut(QtGui.QKeySequence.StandardKey.Save)
        self._actSave.triggered.connect(self._onSave)
        self.packAction.triggered.connect(self.packProject)
        self._actExit.setShortcut(QtGui.QKeySequence.StandardKey.Close)
        self._actExit.triggered.connect(self._onExit)
        _fileMenu.addAction(self._actNewProject)
        _fileMenu.addAction(self._actOpenProject)
        _fileMenu.addAction(self._actSave)
        _fileMenu.addAction(self.packAction)
        _fileMenu.addAction(self._actExit)

        _editMenu = cast(QtWidgets.QMenu, self._menuBar.addMenu(ELOC("EDIT")))
        _gameMenu = cast(QtWidgets.QMenu, self._menuBar.addMenu(ELOC("GAME")))
        _dbMenu = cast(QtWidgets.QMenu, self._menuBar.addMenu(ELOC("DATABASE")))
        _helpMenu = cast(QtWidgets.QMenu, self._menuBar.addMenu(ELOC("HELP")))
        _languageMenu = cast(QtWidgets.QMenu, _helpMenu.addMenu(ELOC("HELP_LANGUAGE")))

        self._actUndo.setShortcut(QtGui.QKeySequence.StandardKey.Undo)
        self._actUndo.triggered.connect(self._onUndo)
        self._actRedo.setShortcut(QtGui.QKeySequence.StandardKey.Redo)
        self._actRedo.triggered.connect(self._onRedo)
        _editMenu.addAction(self._actUndo)
        _editMenu.addAction(self._actRedo)

        self._actGameSettings.triggered.connect(self._onGameSettings)
        self._actGameSettings.setShortcut(QtGui.QKeySequence("F3"))
        _gameMenu.addAction(self._actGameSettings)
        self._actGameConfig.triggered.connect(self._onGameConfig)
        self._actGameConfig.setShortcut(QtGui.QKeySequence("F4"))
        _gameMenu.addAction(self._actGameConfig)
        self._actReloadModule.triggered.connect(self._onReloadModule)
        self._actReloadModule.setShortcut(QtGui.QKeySequence("F5"))
        _gameMenu.addAction(self._actReloadModule)
        self._actNewBlueprint.triggered.connect(self._onNewBlueprint)
        self._actNewBlueprint.setShortcut(QtGui.QKeySequence("F6"))
        _gameMenu.addAction(self._actNewBlueprint)
        self._actNewAnimation.triggered.connect(self._onNewAnimation)
        self._actNewAnimation.setShortcut(QtGui.QKeySequence("F7"))
        _gameMenu.addAction(self._actNewAnimation)

        self._actDatabaseSystemConfig.triggered.connect(self._onDatabaseSystemConfig)
        self._actDatabaseSystemConfig.setShortcut(QtGui.QKeySequence("F8"))
        self._actDatabaseTilesetsData.triggered.connect(self._onDatabaseTilesetsData)
        self._actDatabaseTilesetsData.setShortcut(QtGui.QKeySequence("F9"))
        self._actDatabaseCommonFunctions.triggered.connect(self._onDatabaseCommonFunctions)
        self._actDatabaseCommonFunctions.setShortcut(QtGui.QKeySequence("F10"))
        _dbMenu.addAction(self._actDatabaseSystemConfig)
        _dbMenu.addAction(self._actDatabaseTilesetsData)
        _dbMenu.addAction(self._actDatabaseCommonFunctions)
        _dbMenu.addAction(self._actDatabaseGeneralData)
        _dbMenu.addAction(self._actDatabaseExportLocale)

        self._actHelpExplanation.triggered.connect(self._onHelpExplanation)
        self._actHelpExplanation.setShortcut(QtGui.QKeySequence("F1"))
        _helpMenu.addAction(self._actHelpExplanation)

        self._actAbout = QtWidgets.QAction(ELOC("ABOUT_MENU"), self)
        self._actAbout.triggered.connect(self._onAbout)
        _helpMenu.addAction(self._actAbout)

        for lang in Locale.getLocaleKeys():
            act = cast(QtWidgets.QAction, _languageMenu.addAction(lang))
            act.setCheckable(True)
            if lang == EditorStatus.LANGUAGE:
                act.setChecked(True)
            act.setData(lang)
            self._languageActionGroup.addAction(act)
        self._languageActionGroup.triggered.connect(self._onLanguageChanged)

    def _onLanguageChanged(self, action: QtWidgets.QAction) -> None:
        lang = action.data()
        if not isinstance(lang, str) or not lang:
            return
        cfg = configparser.ConfigParser()
        cfg_path = os.path.join(File.getIniPath(), f"{EditorStatus.APP_NAME}.ini")
        if not os.path.exists(cfg_path):
            return
        cfg.read(cfg_path)
        if EditorStatus.APP_NAME not in cfg:
            cfg[EditorStatus.APP_NAME] = {}
        cfg[EditorStatus.APP_NAME]["Language"] = lang
        with open(cfg_path, "w") as f:
            cfg.write(f)
        QtWidgets.QMessageBox.information(self, "Hint", ELOC("LANGUAGE_CHANGE_RESTART"))

    def _refreshUndoRedo(self) -> None:
        self._actUndo.setEnabled(bool(GameData.undoStack))
        self._actRedo.setEnabled(bool(GameData.redoStack))

    def _onUndo(self, checked: bool = False) -> None:
        diffs = GameData.undo()
        self._refreshCurrentView()
        if diffs:
            self.toast.showMessage("Undo:\n" + "\n".join(diffs))

    def _onRedo(self, checked: bool = False) -> None:
        diffs = GameData.redo()
        self._refreshCurrentView()
        if diffs:
            self.toast.showMessage("Redo:\n" + "\n".join(diffs))

    def _refreshCurrentView(self):
        self.refreshLeftList()
        self.tileSelect.initTilesets()
        self._refreshInfo()
        if self.stacked.currentWidget() == self.editorViewport:
            item = self.leftList.currentItem()
            if item:
                self.editorPanel.refreshMap(item.text())
                self._refreshLayerBar()
        self.editorPanel.clearLightSelection()
        self.lightPanel.setLight(None)
        self._selectedLightMapKey = ""
        self._selectedLightIndex = None

    def _onNewProject(self, checked: bool = False) -> None:
        File.NewProject(self)

    def _onOpenProject(self, checked: bool = False) -> None:
        File.OpenProject(self)

    def _onSave(self, checked: bool = False) -> None:
        ok, content = self._saveAllChanges()
        if ok:
            QtWidgets.QMessageBox.information(self, "Hint", ELOC("SAVE_SUCCESS") + ELOC("SAVE_PATH").format(content))
        else:
            QtWidgets.QMessageBox.warning(self, "Hint", ELOC("SAVE_FAILED") + ELOC("SAVE_PATH").format(content))
        self._refreshInfo()

    def packProject(self, checked: bool = False) -> None:
        projPath = EditorStatus.PROJ_PATH
        if not projPath or not os.path.exists(projPath):
            QtWidgets.QMessageBox.warning(self, ELOC("PACK_TITLE"), ELOC("PACK_NO_PROJECT"))
            return

        entryPath = os.path.join(projPath, "Entry.py")
        if not os.path.exists(entryPath):
            QtWidgets.QMessageBox.warning(self, ELOC("PACK_TITLE"), ELOC("PACK_ENTRY_MISSING"))
            return

        selDlg = PackSelectionDialog(self)
        if selDlg.exec_() != QtWidgets.QDialog.Accepted:
            return

        platform = selDlg.getSelectedPlatform()
        distPath = os.path.join(projPath, "dist")

        python_exe = ""
        if platform == PackPlatform.IOS:
            res = QtWidgets.QMessageBox.warning(
                self,
                ELOC("PACK_TITLE"),
                ELOC("PACK_IOS_SHADER_WARNING"),
                QtWidgets.QMessageBox.Ok | QtWidgets.QMessageBox.Cancel,
                QtWidgets.QMessageBox.Ok,
            )
            if res != QtWidgets.QMessageBox.Ok:
                return
        else:
            python_exe = find_python_3120_for_pack()
            if not python_exe:
                prompt_install_python_3120(self)
                return

        self._packDialog = LogDialog(self)
        self._packDialog.setWindowModality(QtCore.Qt.ApplicationModal)
        self._packWorker = PackWorker(projPath, distPath, platform, python_exe)

        self._packWorker.log_signal.connect(self._packDialog.appendLog)
        self._packWorker.finished_signal.connect(self._packDialog.finish)

        self._packDialog.show()
        self._packWorker.start()

    def _onExit(self, checked: bool = False) -> None:
        self.close()

    def _onHelpExplanation(self, checked: bool = False) -> None:
        filePath = File.getDocPath()
        self._explanationWindow = MarkdownPreviewer(self, filePath)
        self._explanationWindow.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self._explanationWindow.setWindowModality(QtCore.Qt.ApplicationModal)
        self._explanationWindow.show()

    def _onAbout(self, checked: bool = False) -> None:
        dlg = AboutDialog(self)
        dlg.exec_()
