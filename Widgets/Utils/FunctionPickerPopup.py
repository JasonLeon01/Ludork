# -*- encoding: utf-8 -*-

from __future__ import annotations

import inspect
import logging
import sys
from dataclasses import dataclass, field
from types import ModuleType
from typing import Any, Dict, Optional, get_type_hints

from PyQt5 import QtCore, QtGui, QtWidgets

from EditorGlobal.QmlDialogHost import QmlDialogHost

from .NodeFunctionMeta import BindNodeFunctionMetadata, IsSelectableNodeFunction, NodeFunction


FunctionSource = ModuleType | type
log = logging.getLogger(__name__)


@dataclass
class FunctionItem:
    name: str
    path: str = ""
    isParent: bool = False
    displayName: str = ""
    hierarchy: str = ""
    children: list[FunctionItem] = field(default_factory=list)
    expanded: bool = False


class FunctionPickerModel(QtCore.QAbstractListModel):
    NameRole = QtCore.Qt.UserRole + 1
    PathRole = QtCore.Qt.UserRole + 2
    IsParentRole = QtCore.Qt.UserRole + 3
    DepthRole = QtCore.Qt.UserRole + 4
    ExpandableRole = QtCore.Qt.UserRole + 5
    ExpandedRole = QtCore.Qt.UserRole + 6
    HierarchyRole = QtCore.Qt.UserRole + 7

    def __init__(self, parent: QtCore.QObject | None = None) -> None:
        super().__init__(parent)
        self._roots: list[FunctionItem] = []
        self._rows: list[tuple[FunctionItem, int]] = []
        self._searchItems: list[FunctionItem] = []
        self._searchText = ""

    def roleNames(self) -> dict[int, bytes]:
        return {
            self.NameRole: b"itemName",
            self.PathRole: b"itemPath",
            self.IsParentRole: b"isParentFunction",
            self.DepthRole: b"itemDepth",
            self.ExpandableRole: b"isExpandable",
            self.ExpandedRole: b"isExpanded",
            self.HierarchyRole: b"itemHierarchy",
        }

    def rowCount(self, parent: QtCore.QModelIndex = QtCore.QModelIndex()) -> int:
        return 0 if parent.isValid() else len(self._rows)

    def data(self, index: QtCore.QModelIndex, role: int = QtCore.Qt.DisplayRole) -> object:
        if not index.isValid() or not 0 <= index.row() < len(self._rows):
            return None
        item, depth = self._rows[index.row()]
        if role in (QtCore.Qt.DisplayRole, self.NameRole):
            if self._searchText and item.hierarchy:
                return f"{item.name} ({item.hierarchy})"
            return item.name
        if role == self.PathRole:
            return item.path
        if role == self.IsParentRole:
            return item.isParent
        if role == self.DepthRole:
            return depth
        if role == self.ExpandableRole:
            return bool(item.children) and not self._searchText
        if role == self.ExpandedRole:
            return item.expanded
        if role == self.HierarchyRole:
            return item.hierarchy
        return None

    def setRoots(self, roots: list[FunctionItem]) -> None:
        self.beginResetModel()
        self._roots = roots
        self._searchItems = []
        for root in roots:
            self._collectSearchItems(root, "")
        self._rebuildRows()
        self.endResetModel()

    def setSearchText(self, text: str) -> None:
        normalised = text.strip().casefold()
        if normalised == self._searchText:
            return
        self.beginResetModel()
        self._searchText = normalised
        self._rebuildRows()
        self.endResetModel()

    def toggle(self, row: int) -> None:
        item = self.item(row)
        if item is None or not item.children or self._searchText:
            return
        item.expanded = not item.expanded
        self.beginResetModel()
        self._rebuildRows()
        self.endResetModel()

    def item(self, row: int) -> FunctionItem | None:
        if 0 <= row < len(self._rows):
            return self._rows[row][0]
        return None

    def _collectSearchItems(self, item: FunctionItem, parentHierarchy: str) -> None:
        currentHierarchy = f"{parentHierarchy}.{item.name}" if parentHierarchy else item.name
        if item.path:
            item.hierarchy = parentHierarchy
            self._searchItems.append(item)
        for child in item.children:
            self._collectSearchItems(child, currentHierarchy)

    def _rebuildRows(self) -> None:
        if self._searchText:
            self._rows = [
                (item, 0)
                for item in self._searchItems
                if self._searchText in item.name.casefold()
                or self._searchText in item.displayName.casefold()
            ]
            return
        rows: list[tuple[FunctionItem, int]] = []

        def appendVisible(item: FunctionItem, depth: int) -> None:
            rows.append((item, depth))
            if item.expanded:
                for child in item.children:
                    appendVisible(child, depth + 1)

        for root in self._roots:
            appendVisible(root, 0)
        self._rows = rows


class FunctionPickerPopup(QmlDialogHost):
    FUNCTION_SELECTED = QtCore.pyqtSignal(str, bool)

    def __init__(
        self,
        parent: Optional[QtWidgets.QWidget],
        sources: Dict[str, FunctionSource],
        filterExecOnly: bool = False,
        contextSensitive: bool = True,
        contextClass: Optional[type] = None,
    ) -> None:
        self._sources = sources
        self._filterExecOnly = filterExecOnly
        self._contextSensitive = contextSensitive
        self._mroClasses: set[type] = set()
        if contextClass is not None:
            try:
                self._mroClasses = set(inspect.getmro(contextClass))
            except TypeError:
                self._mroClasses = set()
        self._visited: set[str | int | None] = set()
        self._maxDepth = 4
        self._isMac = sys.platform == "darwin"

        super().__init__(parent, "", QtCore.QSize(320, 420), QtCore.QSize(260, 280))
        self.setModal(False)
        self.setWindowModality(QtCore.Qt.NonModal)
        self.setWindowFlags(QtCore.Qt.Tool | QtCore.Qt.FramelessWindowHint | QtCore.Qt.WindowStaysOnTopHint)
        self._model = FunctionPickerModel(self)
        self._rebuildModel()
        self.loadQml(
            "Dialogs/FunctionPickerPopup.qml",
            {
                "functionPickerModel": self._model,
                "functionPickerContextSensitive": self._contextSensitive,
            },
        )
        self._ignoreDeactivate = True

    def showEvent(self, event: QtGui.QShowEvent) -> None:
        super().showEvent(event)
        self.activateWindow()
        self.raise_()
        self._ignoreDeactivate = True
        QtCore.QTimer.singleShot(150, self._clearIgnoreDeactivate)

    def event(self, event: QtCore.QEvent) -> bool:
        if event.type() == QtCore.QEvent.WindowDeactivate and not self._isMac:
            if self._ignoreDeactivate:
                return True
            self.close()
            return True
        return super().event(event)

    def _clearIgnoreDeactivate(self) -> None:
        self._ignoreDeactivate = False

    @QtCore.pyqtSlot(str)
    def setSearchText(self, text: str) -> None:
        self._model.setSearchText(text)

    @QtCore.pyqtSlot(bool)
    def setContextSensitive(self, checked: bool) -> None:
        if self._contextSensitive == checked:
            return
        self._contextSensitive = checked
        self._rebuildModel()

    @QtCore.pyqtSlot(int)
    def activateRow(self, row: int) -> None:
        item = self._model.item(row)
        if item is None:
            return
        if item.path:
            self.FUNCTION_SELECTED.emit(item.path, item.isParent)
            self.close()
            return
        self._model.toggle(row)

    @QtCore.pyqtSlot(int)
    def toggleRow(self, row: int) -> None:
        self._model.toggle(row)

    def _rebuildModel(self) -> None:
        self._visited.clear()
        roots: list[FunctionItem] = []
        for label, source in self._sources.items():
            if source is None:
                continue
            root = FunctionItem(label)
            rootName = getattr(source, "__name__", None)
            if self._addChildren(root, source, "", label == "Parent", 0, rootName):
                roots.append(root)
        self._model.setRoots(roots)

    def _functionItem(self, name: str, function: NodeFunction, path: str, isParent: bool) -> FunctionItem:
        displayName = name
        meta = getattr(function, "_meta", None)
        if isinstance(meta, dict):
            metaValue = meta.get("DisplayName")
            if isinstance(metaValue, str):
                try:
                    displayName = str(eval(metaValue))
                except Exception as error:
                    log.debug("Failed to evaluate node display name %r: %s", metaValue, error)
                    displayName = metaValue
        return FunctionItem(name, path, isParent, displayName)

    def _isInNodeFunctions(self, base: str, source: FunctionSource, inherited: bool) -> bool:
        if inherited:
            return True
        moduleName = getattr(source, "__name__", "")
        return moduleName == "Source.NodeFunctions" or base == "NodeFunctions"

    def _isFunctionContextRelevant(self, function: Any) -> bool:
        if not self._mroClasses:
            return False
        try:
            hints = get_type_hints(function)
        except Exception:
            return False
        for hint in hints.values():
            if hint in self._mroClasses:
                return True
            for argument in getattr(hint, "__args__", ()):
                if argument in self._mroClasses:
                    return True
                for inner in getattr(argument, "__args__", ()):
                    if inner in self._mroClasses:
                        return True
            if isinstance(hint, str) and any(hint == cls.__name__ for cls in self._mroClasses):
                return True
        return False

    def _addChildren(
        self,
        parentItem: FunctionItem,
        source: FunctionSource,
        base: str,
        isParent: bool,
        depth: int = 0,
        rootName: str | None = None,
        isNodeFunctions: bool = False,
        isInMroClass: bool = False,
    ) -> bool:
        def aliasFirstKey(name: str) -> tuple[int, str]:
            try:
                value = getattr(source, name)
                actualName = getattr(value, "__name__", None)
                if isinstance(actualName, str):
                    return 0 if name != actualName.split(".")[-1] else 1, name
            except (AttributeError, TypeError) as error:
                log.debug("Failed to inspect function alias %s: %s", name, error)
            return 1, name

        try:
            names = sorted((name for name in dir(source) if not name.startswith("_")), key=aliasFirstKey)
        except Exception as error:
            log.warning("Failed to enumerate node functions from %s: %s", source, error)
            return False

        if isParent:
            for name in names:
                path = f"{base}.{name}" if base else name
                try:
                    value = BindNodeFunctionMetadata(getattr(source, name), source, name)
                except AttributeError:
                    continue
                if IsSelectableNodeFunction(value, self._filterExecOnly):
                    parentItem.children.append(self._functionItem(name, value, path, True))
            return bool(parentItem.children)

        if depth > self._maxDepth:
            return False
        if inspect.ismodule(source):
            visitKey: str | int | None = getattr(source, "__name__", None)
        elif inspect.isclass(source):
            moduleName = getattr(source, "__module__", "")
            qualifiedName = getattr(source, "__qualname__", getattr(source, "__name__", ""))
            visitKey = f"{moduleName}.{qualifiedName}" if moduleName else qualifiedName
        else:
            visitKey = id(source)
        if visitKey in self._visited:
            return False
        self._visited.add(visitKey)

        for name in names:
            path = f"{base}.{name}" if base else name
            try:
                value = BindNodeFunctionMetadata(getattr(source, name), source, name)
            except AttributeError:
                continue
            if inspect.ismodule(value):
                moduleName = getattr(value, "__name__", "")
                if rootName and isinstance(moduleName, str) and not moduleName.startswith(rootName):
                    continue
                child = FunctionItem(name)
                childNodeFunctions = self._isInNodeFunctions(path, value, isNodeFunctions)
                if self._addChildren(
                    child, value, path, False, depth + 1, rootName or moduleName,
                    childNodeFunctions, isInMroClass,
                ):
                    parentItem.children.append(child)
            elif inspect.isclass(value):
                moduleName = getattr(value, "__module__", "")
                if rootName and isinstance(moduleName, str) and not moduleName.startswith(rootName):
                    continue
                child = FunctionItem(name)
                childInMro = isInMroClass or value in self._mroClasses
                if self._addChildren(
                    child, value, path, False, depth + 1, rootName,
                    isNodeFunctions, childInMro,
                ):
                    parentItem.children.append(child)
            elif IsSelectableNodeFunction(value, self._filterExecOnly):
                moduleName = getattr(value, "__module__", "")
                if rootName is not None and (not isinstance(moduleName, str) or not moduleName.startswith(rootName)):
                    continue
                if self._contextSensitive and not isNodeFunctions and not isInMroClass:
                    if not self._isFunctionContextRelevant(value):
                        continue
                parentItem.children.append(self._functionItem(name, value, path, False))
        return bool(parentItem.children)
