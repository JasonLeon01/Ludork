# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Dict
from PyQt5 import QtCore, QtWidgets
import inspect


class FunctionPickerPopup(QtWidgets.QFrame):
    functionSelected = QtCore.pyqtSignal(str, bool)

    def __init__(self, parent: QtWidgets.QWidget, sources: Dict[str, object], filterExecOnly: bool = False) -> None:
        super().__init__(parent, QtCore.Qt.Popup)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self.setWindowFlag(QtCore.Qt.Popup, True)
        self.setWindowFlag(QtCore.Qt.FramelessWindowHint, True)

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

        self._visited = set()
        self._maxDepth = 4
        self._filterExecOnly = filterExecOnly
        self._build(sources)

        self._searchCache = []
        self._originalItems = []
        self._buildSearchCache()

        self._tree.itemDoubleClicked.connect(self._onDoubleClicked)
        self.resize(320, 420)

    def _build(self, sources: Dict[str, object]) -> None:
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

    def _addChildren(
        self,
        parent_item: QtWidgets.QTreeWidgetItem,
        obj: object,
        base: str,
        is_parent: bool,
        depth: int = 0,
        root_name: str | None = None,
    ) -> bool:
        if is_parent:
            found = False
            try:
                raw = [n for n in dir(obj) if not str(n).startswith("_")]

                def _aliasFirstKey(name):
                    try:
                        a = getattr(obj, name)
                        m = getattr(a, "__name__", None)
                        if isinstance(m, str):
                            return (0 if name != m.split(".")[-1] else 1, str(name))
                    except Exception:
                        return (1, str(name))
                    return (1, str(name))

                names = sorted(raw, key=_aliasFirstKey)
            except Exception:
                return False
            for n in names:
                p = f"{base}.{n}" if base else n
                try:
                    a = getattr(obj, n)
                except Exception:
                    continue
                if (
                    (inspect.isfunction(a) or inspect.ismethod(a))
                    and hasattr(a, "_refLocal")
                    and getattr(a, "_refLocal", None) is not None
                    and (not self._filterExecOnly or (hasattr(a, "_execSplits") and getattr(a, "_execSplits", None)))
                ):
                    it = QtWidgets.QTreeWidgetItem([n])
                    it.setData(0, QtCore.Qt.UserRole, p)
                    it.setData(0, QtCore.Qt.UserRole + 2, True)
                    parent_item.addChild(it)
                    found = True
            return found
        if depth > self._maxDepth:
            return False
        key = getattr(obj, "__name__", None) if (inspect.ismodule(obj) or inspect.isclass(obj)) else id(obj)
        if key in self._visited:
            return False
        self._visited.add(key)
        try:
            raw = [n for n in dir(obj) if not str(n).startswith("_")]

            def _aliasFirstKey(name):
                try:
                    a = getattr(obj, name)
                    m = getattr(a, "__name__", None)
                    if isinstance(m, str):
                        return (0 if name != m.split(".")[-1] else 1, str(name))
                except Exception:
                    return (1, str(name))
                return (1, str(name))

            names = sorted(raw, key=_aliasFirstKey)
        except Exception:
            return False
        found = False
        for n in names:
            p = f"{base}.{n}" if base else n
            try:
                a = getattr(obj, n)
            except Exception:
                continue
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
            elif (
                (inspect.isfunction(a) or inspect.ismethod(a))
                and hasattr(a, "_refLocal")
                and getattr(a, "_refLocal", None) is not None
                and (not self._filterExecOnly or (hasattr(a, "_execSplits") and getattr(a, "_execSplits", None)))
            ):
                mod = getattr(a, "__module__", "")
                if (root_name is None) or (isinstance(mod, str) and mod.startswith(root_name)):
                    it = QtWidgets.QTreeWidgetItem([n])
                    it.setData(0, QtCore.Qt.UserRole, p)
                    it.setData(0, QtCore.Qt.UserRole + 2, False)
                    parent_item.addChild(it)
                    found = True
        return found

    def _onDoubleClicked(self, item: QtWidgets.QTreeWidgetItem, col: int) -> None:
        path = item.data(0, QtCore.Qt.UserRole)
        if isinstance(path, str) and path.strip():
            is_parent = bool(item.data(0, QtCore.Qt.UserRole + 2))
            self.functionSelected.emit(path, is_parent)
            self.close()

    def _buildSearchCache(self) -> None:
        self._searchCache = []
        root = self._tree.invisibleRootItem()
        for i in range(root.childCount()):
            self._collectSearchItems(root.child(i), "")

    def _collectSearchItems(self, item: QtWidgets.QTreeWidgetItem, parent_hierarchy: str) -> None:
        text = item.text(0)
        path = item.data(0, QtCore.Qt.UserRole)

        current_hierarchy = f"{parent_hierarchy}.{text}" if parent_hierarchy else text

        if isinstance(path, str) and path:
            self._searchCache.append(
                {
                    "name": text,
                    "hierarchy": parent_hierarchy,
                    "path": path,
                    "is_parent": bool(item.data(0, QtCore.Qt.UserRole + 2)),
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
            if text in item_data["name"].lower():
                display_text = f"{item_data['name']} ({item_data['hierarchy']})"
                item = QtWidgets.QTreeWidgetItem([display_text])
                item.setData(0, QtCore.Qt.UserRole, item_data["path"])
                item.setData(0, QtCore.Qt.UserRole + 2, item_data["is_parent"])
                self._tree.addTopLevelItem(item)
