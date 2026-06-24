# -*- encoding: utf-8 -*-

from __future__ import annotations
import inspect
import logging
import sys
from types import ModuleType
from typing import Dict, Optional, TypedDict
from PyQt5 import QtCore, QtWidgets, QtGui

from .NodeFunctionMeta import NodeFunction, bindNodeFunctionMetadata, isSelectableNodeFunction


FunctionSource = ModuleType | type
log = logging.getLogger(__name__)


class SearchItem(TypedDict):
    name: str
    hierarchy: str
    path: str
    is_parent: bool
    displayName: str


class FunctionPickerPopup(QtWidgets.QDialog):
    FUNCTION_SELECTED = QtCore.pyqtSignal(str, bool)

    def __init__(
        self, parent: Optional[QtWidgets.QWidget], sources: Dict[str, FunctionSource], filterExecOnly: bool = False
    ) -> None:
        super().__init__(parent)
        self.setModal(False)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self.setWindowFlags(QtCore.Qt.Tool | QtCore.Qt.FramelessWindowHint | QtCore.Qt.WindowStaysOnTopHint)
        self._isMac = sys.platform == "darwin"

        lay = QtWidgets.QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(0)

        self._searchEdit = QtWidgets.QLineEdit(self)
        self._searchEdit.setPlaceholderText("Search...")
        self._searchEdit.setStyleSheet(
            "QLineEdit { background-color: #333; border: none; border-bottom: 1px solid #555; padding: 4px; }"
        )
        self._searchEdit.textChanged.connect(self._onSearch)
        lay.addWidget(self._searchEdit)

        self._tree = QtWidgets.QTreeWidget(self)
        self._tree.setHeaderHidden(True)
        lay.addWidget(self._tree)

        self._visited: set[str | int | None] = set()
        self._maxDepth = 4
        self._filterExecOnly = filterExecOnly
        self._build(sources)

        self._searchCache: list[SearchItem] = []
        self._originalItems: list[QtWidgets.QTreeWidgetItem] = []
        self._buildSearchCache()

        self._tree.itemDoubleClicked.connect(self._onDoubleClicked)
        self.resize(320, 420)
        self.setAttribute(QtCore.Qt.WA_InputMethodEnabled, True)
        self._searchEdit.setAttribute(QtCore.Qt.WA_InputMethodEnabled, True)
        self._searchEdit.setFocusPolicy(QtCore.Qt.StrongFocus)
        self._ignoreDeactivate = True

    def showEvent(self, e: QtCore.QEvent) -> None:
        super().showEvent(e)
        self._searchEdit.setFocus(QtCore.Qt.OtherFocusReason)
        self.activateWindow()
        self.raise_()
        self._ignoreDeactivate = True
        QtCore.QTimer.singleShot(150, self._clearIgnoreDeactivate)

    def _clearIgnoreDeactivate(self) -> None:
        self._ignoreDeactivate = False

    def event(self, e: QtCore.QEvent) -> bool:
        if e.type() == QtCore.QEvent.WindowDeactivate:
            if getattr(self, "_isMac", False):
                return super().event(e)
            if getattr(self, "_ignoreDeactivate", False):
                return True
            self.close()
            return True
        return super().event(e)

    def keyPressEvent(self, e: QtGui.QKeyEvent) -> None:
        if e.key() == QtCore.Qt.Key_Escape:
            self.close()
            return
        super().keyPressEvent(e)

    def _build(self, sources: Dict[str, FunctionSource]) -> None:
        self._tree.clear()
        for label, obj in sources.items():
            if obj is None:
                continue
            root = QtWidgets.QTreeWidgetItem([label])
            root.setData(0, QtCore.Qt.UserRole + 2, label == "Parent")
            root_name = getattr(obj, "__name__", None)
            if self._addChildren(root, obj, "", label == "Parent", 0, root_name):
                self._tree.addTopLevelItem(root)
        self._tree.collapseAll()

    def _handleChildren(self, n: str, a: NodeFunction, p: str, parent_item: QtWidgets.QTreeWidgetItem) -> None:
        it = QtWidgets.QTreeWidgetItem([n])
        it.setData(0, QtCore.Qt.UserRole, p)
        it.setData(0, QtCore.Qt.UserRole + 2, True)
        displayName = None
        meta = getattr(a, "_meta", None)
        if isinstance(meta, dict):
            mv = meta.get("DisplayName")
            if isinstance(mv, str):
                try:
                    displayName = str(eval(mv))
                except Exception as e:
                    log.debug("Failed to evaluate node display name %r: %s", mv, e)
                    displayName = mv
        it.setData(0, QtCore.Qt.UserRole + 3, displayName if isinstance(displayName, str) else n)
        parent_item.addChild(it)

    def _addChildren(
        self,
        parent_item: QtWidgets.QTreeWidgetItem,
        obj: FunctionSource,
        base: str,
        is_parent: bool,
        depth: int = 0,
        root_name: str | None = None,
    ) -> bool:
        def _aliasFirstKey(name: str) -> tuple[int, str]:
            try:
                a = getattr(obj, name)
                m = getattr(a, "__name__", None)
                if isinstance(m, str):
                    return (0 if name != m.split(".")[-1] else 1, str(name))
            except (AttributeError, TypeError) as e:
                log.debug("Failed to inspect function alias %s: %s", name, e)
                return (1, str(name))
            return (1, str(name))

        if is_parent:
            found = False
            try:
                raw = [n for n in dir(obj) if not str(n).startswith("_")]
                names = sorted(raw, key=_aliasFirstKey)
            except Exception as e:
                log.warning("Failed to enumerate parent node functions from %s: %s", obj, e)
                return False
            for n in names:
                p = f"{base}.{n}" if base else n
                try:
                    a = getattr(obj, n)
                except AttributeError:
                    continue
                a = bindNodeFunctionMetadata(a, obj, n)
                if isSelectableNodeFunction(a, self._filterExecOnly):
                    self._handleChildren(n, a, p, parent_item)
                    found = True
            return found
        if depth > self._maxDepth:
            return False
        visitKey = None
        if inspect.ismodule(obj):
            visitKey = getattr(obj, "__name__", None)
        elif inspect.isclass(obj):
            modName = getattr(obj, "__module__", "")
            qualName = getattr(obj, "__qualname__", getattr(obj, "__name__", ""))
            visitKey = f"{modName}.{qualName}" if modName else qualName
        else:
            visitKey = id(obj)

        if visitKey in self._visited:
            return False
        self._visited.add(visitKey)
        try:
            raw = [n for n in dir(obj) if not str(n).startswith("_")]
            names = sorted(raw, key=_aliasFirstKey)
        except Exception as e:
            log.warning("Failed to enumerate node functions from %s: %s", obj, e)
            return False
        found = False
        for n in names:
            p = f"{base}.{n}" if base else n
            try:
                a = getattr(obj, n)
            except AttributeError:
                continue
            a = bindNodeFunctionMetadata(a, obj, n)
            if inspect.ismodule(a):
                mod_name = getattr(a, "__name__", "")
                if root_name and isinstance(mod_name, str) and not mod_name.startswith(root_name):
                    continue
                child_item = QtWidgets.QTreeWidgetItem([n])
                if self._addChildren(child_item, a, p, False, depth + 1, root_name or mod_name):
                    parent_item.addChild(child_item)
                    found = True
            elif inspect.isclass(a):
                mod_name = getattr(a, "__module__", "")
                if root_name and isinstance(mod_name, str) and not mod_name.startswith(root_name):
                    continue
                child_item = QtWidgets.QTreeWidgetItem([n])
                # Pass root_name down as is, because for a class, its name is not a module path prefix
                if self._addChildren(child_item, a, p, False, depth + 1, root_name):
                    parent_item.addChild(child_item)
                    found = True
            elif isSelectableNodeFunction(a, self._filterExecOnly):
                mod = getattr(a, "__module__", "")
                if (root_name is None) or (isinstance(mod, str) and mod.startswith(root_name)):
                    self._handleChildren(n, a, p, parent_item)
                    found = True
        return found

    def _onDoubleClicked(self, item: QtWidgets.QTreeWidgetItem, col: int) -> None:
        path = item.data(0, QtCore.Qt.UserRole)
        if isinstance(path, str) and path.strip():
            is_parent = bool(item.data(0, QtCore.Qt.UserRole + 2))
            self.FUNCTION_SELECTED.emit(path, is_parent)
            self.close()

    def _buildSearchCache(self) -> None:
        self._searchCache = []
        root = self._tree.invisibleRootItem()
        if not root:
            return
        for i in range(root.childCount()):
            self._collectSearchItems(root.child(i), "")

    def _collectSearchItems(self, item: QtWidgets.QTreeWidgetItem, parent_hierarchy: str) -> None:
        text = item.text(0)
        path = item.data(0, QtCore.Qt.UserRole)
        displayName = item.data(0, QtCore.Qt.UserRole + 3)

        current_hierarchy = f"{parent_hierarchy}.{text}" if parent_hierarchy else text

        if isinstance(path, str) and path:
            self._searchCache.append(
                {
                    "name": text,
                    "hierarchy": parent_hierarchy,
                    "path": path,
                    "is_parent": bool(item.data(0, QtCore.Qt.UserRole + 2)),
                    "displayName": displayName if isinstance(displayName, str) else text,
                }
            )

        for i in range(item.childCount()):
            self._collectSearchItems(item.child(i), current_hierarchy)

    def _onSearch(self, text: str) -> None:
        text = text.strip().lower()
        if not text:
            if self._originalItems:
                self._tree.clear()
                self._tree.addTopLevelItems(self._originalItems)
                self._originalItems = []
            return

        if not self._originalItems:
            self._originalItems = [self._tree.takeTopLevelItem(0) for _ in range(self._tree.topLevelItemCount())]

        self._tree.clear()

        for item_data in self._searchCache:
            dn = item_data.get("displayName")
            dn_str = dn.lower() if isinstance(dn, str) else ""
            if text in item_data["name"].lower() or (dn_str and text in dn_str):
                display_text = f"{item_data['name']} ({item_data['hierarchy']})"
                item = QtWidgets.QTreeWidgetItem([display_text])
                item.setData(0, QtCore.Qt.UserRole, item_data["path"])
                item.setData(0, QtCore.Qt.UserRole + 2, item_data["is_parent"])
                self._tree.addTopLevelItem(item)
