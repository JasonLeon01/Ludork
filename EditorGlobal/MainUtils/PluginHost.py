# -*- encoding: utf-8 -*-

from typing import Optional

from PyQt5 import QtCore, QtGui, QtWidgets


class PluginHostMixin:
    def addPluginToolbarWidget(self, widget: QtWidgets.QWidget) -> bool:
        layout = self.topBar.layout()
        if not isinstance(layout, QtWidgets.QHBoxLayout):
            return False
        index = self._pluginToolbarInsertIndex(layout)
        layout.insertWidget(index, widget, 0, alignment=QtCore.Qt.AlignRight)
        return True

    def addPluginPanel(self, widget: QtWidgets.QWidget) -> None:
        self.rightStack.addWidget(widget)

    def setActivePluginPanel(self, widget: QtWidgets.QWidget) -> None:
        self.rightStack.setCurrentWidget(widget)

    def addPluginTab(
        self, widget: QtWidgets.QWidget, label: str, icon: Optional[QtGui.QIcon] = None
    ) -> None:
        if icon is not None:
            self.tabWidget.addTab(widget, icon, label)
        else:
            self.tabWidget.addTab(widget, label)

    def pluginConsoleFilterMenu(self) -> Optional[QtWidgets.QMenu]:
        console = self.consoleWidget
        if not isinstance(console, QtWidgets.QWidget):
            return None
        return console.filterMenu()

    def currentEditModeIndex(self) -> int:
        return int(self._editModeIdx)

    def notifyPluginDataChanged(self) -> None:
        self.refreshLeftList()
        self._refreshInfo()

    def _pluginToolbarInsertIndex(self, layout: QtWidgets.QHBoxLayout) -> int:
        for i in range(layout.count()):
            item = layout.itemAt(i)
            if item is not None and item.widget() is self.editModeToggle:
                return i
        return layout.count()
