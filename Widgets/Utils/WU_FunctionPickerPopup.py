# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Dict
from PyQt5 import QtCore, QtWidgets
import inspect


class FunctionPickerPopup(QtWidgets.QFrame):
    functionSelected = QtCore.pyqtSignal(str, bool)

    def __init__(self, parent: QtWidgets.QWidget, sources: Dict[str, object]) -> None:
        super().__init__(parent, QtCore.Qt.Popup)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose, True)
        self.setWindowFlag(QtCore.Qt.Popup, True)
        self.setWindowFlag(QtCore.Qt.FramelessWindowHint, True)
        self._tree = QtWidgets.QTreeWidget(self)
        self._tree.setHeaderHidden(True)
        lay = QtWidgets.QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(0)
        lay.addWidget(self._tree)
        self._visited = set()
        self._max_depth = 4
        self._build(sources)
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
                names = [n for n in dir(obj) if not str(n).startswith("_")]
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
                ):
                    it = QtWidgets.QTreeWidgetItem([n])
                    it.setData(0, QtCore.Qt.UserRole, p)
                    it.setData(0, QtCore.Qt.UserRole + 2, True)
                    parent_item.addChild(it)
                    found = True
            return found
        if depth > self._max_depth:
            return False
        key = getattr(obj, "__name__", None) if (inspect.ismodule(obj) or inspect.isclass(obj)) else id(obj)
        if key in self._visited:
            return False
        self._visited.add(key)
        try:
            names = [n for n in dir(obj) if not str(n).startswith("_")]
        except Exception:
            return False
        found = False
        for n in names:
            p = f"{base}.{n}" if base else n
            try:
                a = getattr(obj, n)
            except Exception:
                continue
            if inspect.ismodule(a) or inspect.isclass(a):
                mod_name = getattr(a, "__name__", "")
                if root_name and isinstance(mod_name, str) and not mod_name.startswith(root_name):
                    continue
                child_item = QtWidgets.QTreeWidgetItem([n])
                if self._addChildren(child_item, a, p, False, depth + 1, root_name or mod_name):
                    parent_item.addChild(child_item)
                    found = True
            elif (
                (inspect.isfunction(a) or inspect.ismethod(a))
                and hasattr(a, "_refLocal")
                and getattr(a, "_refLocal", None) is not None
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
