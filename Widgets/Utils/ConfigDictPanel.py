# -*- encoding: utf-8 -*-

from typing import Any, List, Tuple
import os
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import System
from EditorGlobal import EditorStatus, GameData
from .FileSelectorDialog import FileSelectorDialog


class ConfigDictPanel(QtWidgets.QWidget):
    CONTENT_HEIGHT_CHANGED = QtCore.pyqtSignal()
    MODIFIED = QtCore.pyqtSignal()

    def __init__(self, parent: QtWidgets.QWidget, filename: str, data: dict[str, Any]) -> None:
        super().__init__(parent)
        self._filename = filename
        self._data = data
        self.setObjectName("ConfigDictPanel")
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        System.SetStyle(self, "config.qss")
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)
        title = QtWidgets.QLabel(filename)
        title.setAlignment(QtCore.Qt.AlignHCenter | QtCore.Qt.AlignVCenter)
        f = title.font()
        f.setBold(True)
        f.setPixelSize(16)
        title.setFont(f)
        layout.addWidget(title)
        form = QtWidgets.QFormLayout()
        form.setContentsMargins(0, 0, 0, 0)
        form.setSpacing(8)
        layout.addLayout(form)
        for key, val in data.items():
            if not isinstance(val, dict):
                continue
            t = str(val.get("type", "")).strip()
            base_t, arr_len = self._parseType(t)
            label = QtWidgets.QLabel(ELOC(key))
            if arr_len is None and base_t.endswith("[]"):
                base_t = base_t[:-2]
            is_array = (arr_len is not None) or t.endswith("[]")
            if is_array:
                w = self._createArrayWidget(key, base_t, arr_len, val)
                form.addRow(label, w)
            else:
                w = self._createSingleWidget(key, base_t, val)
                form.addRow(label, w)

    def _parseType(self, t: str) -> Tuple[str, int | None]:
        if "[" in t and "]" in t:
            inner = t[t.index("[") + 1 : t.index("]")]
            base = t[: t.index("[")]
            if inner == "":
                return base + "[]", None
            try:
                n = int(inner)
                return base, n
            except ValueError:
                return t, None
        return t, None

    def _createLineEdit(self, vtype: str, initial: Any, readOnly: bool = False) -> QtWidgets.QLineEdit:
        edit = QtWidgets.QLineEdit(self)
        edit.setReadOnly(readOnly)
        if vtype == "int":
            edit.setValidator(QtGui.QIntValidator(edit))
            edit.setText(str(int(initial) if initial is not None and str(initial).strip() != "" else 0))
        elif vtype == "float":
            edit.setValidator(QtGui.QDoubleValidator(edit))
            edit.setText(str(float(initial) if initial is not None and str(initial).strip() != "" else 0.0))
        else:
            edit.setText(str(initial if initial is not None else ""))
        return edit

    def _getFileFilters(self, ext: Any) -> List[str]:
        filters: List[str] = []
        if isinstance(ext, str) and ext:
            filters.append(f"*{ext}")
        elif isinstance(ext, list):
            for e in ext:
                if isinstance(e, str) and e:
                    filters.append(f"*{e}")
        return filters

    def _selectFileName(self, val: dict[str, Any]) -> str | None:
        base = str(val.get("base", "")).strip()
        rootKey = val.get("root")
        if rootKey:
            root = os.path.join(EditorStatus.PROJ_PATH, str(rootKey).strip())
        else:
            root = os.path.join(EditorStatus.PROJ_PATH, "Assets")
        if base:
            root = os.path.join(root, base)
        filters = self._getFileFilters(val.get("ext"))
        filter_str = FileSelectorDialog.filesFilter(filters) if filters else FileSelectorDialog.allFilesFilter(star=True)
        dlg = FileSelectorDialog(self, root, filter_str)
        fp = dlg.execSelect()
        if not fp:
            return None
        bn = os.path.basename(fp)
        if filters and not any(bn.endswith(e.replace("*", "")) for e in filters):
            return None
        return bn

    def _createFileRow(
        self,
        key: str,
        val: dict[str, Any],
        initial: str | None,
        list_ref: List[str] | None = None,
        index: int | None = None,
    ) -> QtWidgets.QWidget:
        w = QtWidgets.QWidget(self)
        h = QtWidgets.QHBoxLayout(w)
        h.setContentsMargins(0, 0, 0, 0)
        h.setSpacing(6)
        edit = self._createLineEdit("string", initial or "", readOnly=True)
        btn = QtWidgets.QPushButton("...")
        h.addWidget(edit, 1)
        h.addWidget(btn, 0)

        def on_browse():
            bn = self._selectFileName(val)
            if bn is None:
                return
            GameData.RecordSnapshot()
            edit.setText(bn)
            if list_ref is not None and index is not None:
                if index >= 0 and index < len(list_ref):
                    list_ref[index] = bn
                else:
                    list_ref.append(bn)
                val["value"] = list_ref
            else:
                val["value"] = bn
            self.MODIFIED.emit()

        btn.clicked.connect(on_browse)
        return w

    def _createSingleWidget(self, key: str, base_t: str, val: dict[str, Any]) -> QtWidgets.QWidget:
        if base_t == "file":
            return self._createFileRow(key, val, val.get("value"))
        edit = self._createLineEdit(base_t, val.get("value"))

        def on_changed(text: str):
            GameData.RecordSnapshot()
            if base_t == "int":
                try:
                    val["value"] = int(text) if text.strip() else 0
                except Exception as e:
                    print(f"Error parsing int: {e}")
            elif base_t == "float":
                try:
                    val["value"] = float(text) if text.strip() else 0.0
                except Exception as e:
                    print(f"Error parsing float: {e}")
            else:
                val["value"] = text
            self.MODIFIED.emit()

        edit.textChanged.connect(on_changed)
        return edit

    def _createArrayWidget(
        self, key: str, base_t: str, arr_len: int | None, val: dict[str, Any]
    ) -> QtWidgets.QWidget:
        container = QtWidgets.QWidget(self)
        v = QtWidgets.QVBoxLayout(container)
        v.setContentsMargins(0, 0, 0, 0)
        v.setSpacing(6)
        values = val.get("value")
        if not isinstance(values, list):
            values = []
        if arr_len is not None:
            if len(values) < arr_len:
                while len(values) < arr_len:
                    if base_t == "int":
                        values.append(0)
                    elif base_t == "float":
                        values.append(0.0)
                    else:
                        values.append("")
        var_len = arr_len is None

        def add_row(initial_val: Any = "", i: int | None = None):
            row = QtWidgets.QWidget(self)
            h = QtWidgets.QHBoxLayout(row)
            h.setContentsMargins(0, 0, 0, 0)
            h.setSpacing(6)
            if base_t == "file":
                edit = self._createLineEdit(
                    "string", initial_val if isinstance(initial_val, str) else "", readOnly=True
                )
                browse = QtWidgets.QPushButton("...")
                h.addWidget(edit, 1)
                h.addWidget(browse, 0)
                if var_len:
                    minus = QtWidgets.QPushButton("-")
                    minus.setObjectName("MinusBtn")
                    h.insertWidget(1, minus)

                def on_browse():
                    bn = self._selectFileName(val)
                    if bn is None:
                        return
                    edit.setText(bn)
                    idx_now = v.indexOf(row)
                    if idx_now >= 0 and idx_now < len(values):
                        GameData.RecordSnapshot()
                        values[idx_now] = bn
                        val["value"] = values
                        self.MODIFIED.emit()

                browse.clicked.connect(on_browse)
                if var_len:

                    def on_minus():
                        idx_now = v.indexOf(row)
                        if idx_now >= 0 and idx_now < len(values):
                            GameData.RecordSnapshot()
                            values.pop(idx_now)
                            val["value"] = values
                        v.removeWidget(row)
                        row.setParent(None)
                        row.deleteLater()
                        self.CONTENT_HEIGHT_CHANGED.emit()
                        self.MODIFIED.emit()

                    minus.clicked.connect(on_minus)
            else:
                edit = self._createLineEdit(base_t, initial_val)
                h.addWidget(edit, 1)
                if var_len:
                    minus = QtWidgets.QPushButton("-")
                    minus.setObjectName("MinusBtn")
                    h.addWidget(minus, 0)

                def on_changed(text: str):
                    GameData.RecordSnapshot()
                    idx_now = v.indexOf(row)
                    if base_t == "int":
                        try:
                            values[idx_now] = int(text) if text.strip() else 0
                        except Exception as e:
                            print(f"Error parsing int: {e}")
                    elif base_t == "float":
                        try:
                            values[idx_now] = float(text) if text.strip() else 0.0
                        except Exception as e:
                            print(f"Error parsing float: {e}")
                    else:
                        values[idx_now] = text
                    val["value"] = values
                    self.MODIFIED.emit()

                edit.textChanged.connect(on_changed)

                if var_len:

                    def on_minus():
                        idx_now = v.indexOf(row)
                        if idx_now >= 0 and idx_now < len(values):
                            GameData.RecordSnapshot()
                            values.pop(idx_now)
                            val["value"] = values
                        v.removeWidget(row)
                        row.setParent(None)
                        row.deleteLater()
                        self.CONTENT_HEIGHT_CHANGED.emit()

                    minus.clicked.connect(on_minus)

            last_idx = v.count() - 1
            if arr_len is None and last_idx >= 0:
                item = v.itemAt(last_idx)
                if item and item.widget() is not None and isinstance(item.widget(), QtWidgets.QPushButton):
                    v.insertWidget(last_idx, row)
                else:
                    v.addWidget(row)
            else:
                v.addWidget(row)

        for i in range(len(values)):
            add_row(values[i], i)
        if arr_len is None:
            btn = QtWidgets.QPushButton("+")

            def on_add():
                GameData.RecordSnapshot()
                values.append("")
                val["value"] = values
                add_row("", len(values) - 1)
                self.CONTENT_HEIGHT_CHANGED.emit()
                self.MODIFIED.emit()

            btn.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Fixed)
            btn.clicked.connect(on_add)
            v.addWidget(btn)
        else:
            val["value"] = values[:arr_len]
        return container

    def getData(self) -> dict[str, Any]:
        return self._data
