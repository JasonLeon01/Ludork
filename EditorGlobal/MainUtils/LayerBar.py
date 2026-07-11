# -*- encoding: utf-8 -*-

import os
from typing import Optional
from PyQt5 import QtCore, QtWidgets
from Widgets.Utils.FileSelectorDialog import FileSelectorDialog
from Widgets.Utils.SingleRowDialog import OpenSingleRowDialog


class LayerBarMixin:
    def _refreshLayerBar(self) -> None:
        selectedLayerName = self._selectedLayerName
        wasBlocked = self.layerList.blockSignals(True)
        try:
            self.layerList.clear()
            self.layerList.addTab(QtWidgets.QWidget(), ELOC("OVERVIEW"))
            names = self.editorPanel.getLayerNames()
            for n in names:
                self.layerList.addTab(QtWidgets.QWidget(), n)

            selectedIndex = 0
            if selectedLayerName:
                found = False
                for i in range(1, self.layerList.count()):
                    if self.layerList.tabText(i) == selectedLayerName:
                        selectedIndex = i
                        found = True
                        break
                if not found:
                    selectedLayerName = None

            self.layerList.setCurrentIndex(selectedIndex)
        finally:
            self.layerList.blockSignals(wasBlocked)
        if selectedIndex == 0:
            self._clearLayerSelection(True)
        elif selectedLayerName != self._selectedLayerName:
            self._selectLayer(selectedLayerName)

    def _clearLayerSelection(self, force: bool = False) -> None:
        if self._selectedLayerName is None and not force:
            return
        self._selectedLayerName = None
        self.editorPanel.setSelectedLayer(None)
        self.tileSelect.setLayerSelected(False)
        self.tileSelect.clearSelection()
        if self._editModeIdx == 2:
            self.editorPanel.setAcceptDrops(False)

    def _selectLayer(self, name: str) -> None:
        self._selectedLayerName = name
        self.editorPanel.setSelectedLayer(name)
        key = self.editorPanel.getLayerTilesetKey(name)
        if key:
            self.tileSelect.setCurrentTilesetKey(key)
        self.tileSelect.setLayerSelected(True)
        if self._editModeIdx == 2:
            self.editorPanel.setAcceptDrops(True)

    def _onLayerTabChanged(self, index: int) -> None:
        if index == 0:
            self._clearLayerSelection()
            return

        name = self.layerList.tabText(index)
        if name == self._selectedLayerName:
            return

        self._selectLayer(name)

    def _onLayerTabMoved(self, fromIndex: int, toIndex: int) -> None:
        overview_text = ELOC("OVERVIEW")
        tabBar = self._layerTabBar()

        if self.layerList.tabText(0) != overview_text:
            for i in range(self.layerList.count()):
                if self.layerList.tabText(i) == overview_text:
                    tabBar.moveTab(i, 0)
                    break

        new_order = []
        for i in range(1, self.layerList.count()):
            new_order.append(self.layerList.tabText(i))

        self.editorPanel.reorderLayers(new_order)

    def _onAddLayer(self, checked: bool = False, insertAfterTabIndex: Optional[int] = None) -> None:
        if self.editorPanel.mapData is None:
            QtWidgets.QMessageBox.warning(self, "Hint", ELOC("ADD_ERROR"))
            return
        existing = set(self.editorPanel.getLayerNames())
        self._promptAddLayerName(existing, insertAfterTabIndex)

    def _promptAddLayerName(
        self,
        existing: set[str],
        insertAfterTabIndex: Optional[int] = None,
    ) -> None:
        def onAccepted(name: str) -> None:
            name = name.strip()
            if not name:
                QtWidgets.QMessageBox.warning(self, "Hint", ELOC("ADD_EMPTY"))
                self._promptAddLayerName(existing, insertAfterTabIndex)
                return
            if name in existing:
                QtWidgets.QMessageBox.warning(self, "Hint", ELOC("ADD_DUPLICATE"))
                self._promptAddLayerName(existing, insertAfterTabIndex)
                return
            self.editorPanel.addEmptyLayer(name)
            if insertAfterTabIndex is not None and insertAfterTabIndex >= 1:
                names = [n for n in self.editorPanel.getLayerNames() if n != name]
                new_order = names[:insertAfterTabIndex] + [name] + names[insertAfterTabIndex:]
                self.editorPanel.reorderLayers(new_order)
            self._selectLayer(name)
            self._refreshLayerBar()

        OpenSingleRowDialog(self, ELOC("ADD_LAYER"), ELOC("ADD_MESSAGE"), onAccepted=onAccepted)

    def _onLayerContextMenu(self, pos: QtCore.QPoint) -> None:
        tabBar = self._layerTabBar()

        tab_pos = tabBar.mapFromParent(pos)
        index = tabBar.tabAt(tab_pos)

        if index <= 0:
            menu = QtWidgets.QMenu(self)
            actAdd = menu.addAction(ELOC("ADD_LAYER"))
            from Utils import PluginSystem

            PluginSystem.AddRightClickActions(menu, self, "layerTab", "empty", None)
            action = menu.exec_(self.layerList.mapToGlobal(pos))
            if action == actAdd:
                self._onAddLayer()
            return

        self.layerList.setCurrentIndex(index)
        name = self.layerList.tabText(index)

        menu = QtWidgets.QMenu(self)
        actAdd = menu.addAction(ELOC("ADD_LAYER"))
        actRename = menu.addAction(ELOC("RENAME_LAYER"))
        menu.addSeparator()
        actSelectShader = menu.addAction(ELOC("SELECT_LAYER_SHADER"))
        actClearShader = menu.addAction(ELOC("CLEAR_LAYER_SHADER"))
        actClearShader.setEnabled(bool(self.editorPanel.getLayerShaderPath(name)))
        menu.addSeparator()
        actDelete = menu.addAction(ELOC("DELETE"))
        from Utils import PluginSystem

        PluginSystem.AddRightClickActions(menu, self, "layerTab", "hit", name)
        action = menu.exec_(self.layerList.mapToGlobal(pos))

        if action == actAdd:
            self._onAddLayer(insertAfterTabIndex=index)
            return

        if action == actRename:
            existing = set(self.editorPanel.getLayerNames())
            if name in existing:
                existing.remove(name)
            self._promptRenameLayer(name, existing)
            return

        if action == actSelectShader:
            self._selectLayerShader(name)
            return

        if action == actClearShader:
            self.editorPanel.setLayerShaderPath(name, "")
            return

        if action == actDelete:
            ret = QtWidgets.QMessageBox.question(
                self,
                "Hint",
                ELOC("CONFIRM_DELETE_LAYER").format(name=name),
                QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
                QtWidgets.QMessageBox.No,
            )
            if ret == QtWidgets.QMessageBox.Yes:
                if self.editorPanel.removeLayer(name):
                    if self._selectedLayerName == name:
                        self._selectedLayerName = None
                        if self._editModeIdx == 2:
                            self.editorPanel.setAcceptDrops(False)
                    self._refreshLayerBar()

    def _promptRenameLayer(self, oldName: str, existing: set[str]) -> None:
        def onAccepted(newName: str) -> None:
            newName = newName.strip()
            if not newName:
                QtWidgets.QMessageBox.warning(self, "Hint", ELOC("ADD_EMPTY"))
                self._promptRenameLayer(oldName, existing)
                return
            if newName in existing:
                QtWidgets.QMessageBox.warning(self, "Hint", ELOC("ADD_DUPLICATE"))
                self._promptRenameLayer(oldName, existing)
                return
            if self.editorPanel.renameLayer(oldName, newName):
                if self._selectedLayerName == oldName:
                    self._selectedLayerName = newName
                self._refreshLayerBar()

        OpenSingleRowDialog(
            self,
            ELOC("RENAME_LAYER"),
            ELOC("RENAME_MESSAGE"),
            oldName,
            onAccepted=onAccepted,
        )

    def _selectLayerShader(self, layerName: str) -> None:
        from EditorGlobal import EditorStatus

        shaderRoot = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Shaders")
        dialog = FileSelectorDialog(
            self,
            shaderRoot,
            FileSelectorDialog.filesFilter(["*.vert", "*.frag"]),
            ELOC("SELECT_LAYER_SHADER"),
        )
        dialog.openSelect(
            lambda selectedPath: self._applyLayerShaderSelection(layerName, selectedPath, shaderRoot)
        )

    def _applyLayerShaderSelection(self, layerName: str, selectedPath: str, shaderRoot: str) -> None:
        if not selectedPath:
            return
        shaderPath = os.path.relpath(selectedPath, shaderRoot).replace("\\", "/")
        self.editorPanel.setLayerShaderPath(layerName, shaderPath)
