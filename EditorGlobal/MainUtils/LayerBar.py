# -*- encoding: utf-8 -*-

from typing import Optional
from PyQt5 import QtCore, QtWidgets
from Widgets.Utils import SingleRowDialog


class LayerBarMixin:
    def _refreshLayerBar(self) -> None:
        self.layerList.clear()
        self.layerList.addTab(QtWidgets.QWidget(), ELOC("OVERVIEW"))
        names = self.editorPanel.getLayerNames()
        for n in names:
            self.layerList.addTab(QtWidgets.QWidget(), n)

        if self._selectedLayerName:
            found = False
            for i in range(1, self.layerList.count()):
                if self.layerList.tabText(i) == self._selectedLayerName:
                    self.layerList.setCurrentIndex(i)
                    found = True
                    break
            if not found:
                self._selectedLayerName = None
                self.layerList.setCurrentIndex(0)
        else:
            self.layerList.setCurrentIndex(0)

    def _clearLayerSelection(self) -> None:
        if self._selectedLayerName is None:
            return
        self._selectedLayerName = None
        self.editorPanel.setSelectedLayer(None)
        self.tileSelect.setLayerSelected(False)
        self.tileSelect.clearSelection()
        if self._editModeIdx == 2:
            self.editorPanel.setAcceptDrops(False)

    def _onLayerTabChanged(self, index: int) -> None:
        if index == 0:
            self._clearLayerSelection()
            return

        name = self.layerList.tabText(index)
        if name == self._selectedLayerName:
            return

        self._selectedLayerName = name
        self.editorPanel.setSelectedLayer(name)
        key = self.editorPanel.getLayerTilesetKey(name)
        if key:
            self.tileSelect.setCurrentTilesetKey(key)
        self.tileSelect.setLayerSelected(True)
        if self._editModeIdx == 2:
            self.editorPanel.setAcceptDrops(True)

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
        while True:
            dlg = SingleRowDialog(self, ELOC("ADD_LAYER"), ELOC("ADD_MESSAGE"))
            ok, name = dlg.execGetText()
            if not ok:
                return
            name = name.strip()
            if not name:
                QtWidgets.QMessageBox.warning(self, "Hint", ELOC("ADD_EMPTY"))
                continue
            if name in existing:
                QtWidgets.QMessageBox.warning(self, "Hint", ELOC("ADD_DUPLICATE"))
                continue
            break
        self.editorPanel.addEmptyLayer(name)
        if insertAfterTabIndex is not None and insertAfterTabIndex >= 1:
            names = [n for n in self.editorPanel.getLayerNames() if n != name]
            new_order = names[:insertAfterTabIndex] + [name] + names[insertAfterTabIndex:]
            self.editorPanel.reorderLayers(new_order)
        self._selectedLayerName = name
        self._refreshLayerBar()

    def _onLayerContextMenu(self, pos: QtCore.QPoint) -> None:
        tabBar = self._layerTabBar()

        tab_pos = tabBar.mapFromParent(pos)
        index = tabBar.tabAt(tab_pos)

        if index <= 0:
            menu = QtWidgets.QMenu(self)
            actAdd = menu.addAction(ELOC("ADD_LAYER"))
            action = menu.exec_(self.layerList.mapToGlobal(pos))
            if action == actAdd:
                self._onAddLayer()
            return

        self.layerList.setCurrentIndex(index)
        name = self.layerList.tabText(index)

        menu = QtWidgets.QMenu(self)
        actAdd = menu.addAction(ELOC("ADD_LAYER"))
        actRename = menu.addAction(ELOC("RENAME_LAYER"))
        actDelete = menu.addAction(ELOC("DELETE"))
        action = menu.exec_(self.layerList.mapToGlobal(pos))

        if action == actAdd:
            self._onAddLayer(insertAfterTabIndex=index)
            return

        if action == actRename:
            existing = set(self.editorPanel.getLayerNames())
            if name in existing:
                existing.remove(name)
            while True:
                dlg = SingleRowDialog(self, ELOC("RENAME_LAYER"), ELOC("RENAME_MESSAGE"), str(name))
                ok, newName = dlg.execGetText()
                if not ok:
                    return
                newName = newName.strip()
                if not newName:
                    QtWidgets.QMessageBox.warning(self, "Hint", ELOC("ADD_EMPTY"))
                    continue
                if newName in existing:
                    QtWidgets.QMessageBox.warning(self, "Hint", ELOC("ADD_DUPLICATE"))
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
