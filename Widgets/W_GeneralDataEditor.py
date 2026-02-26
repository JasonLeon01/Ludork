# -*- encoding: utf-8 -*-

from typing import Any, Dict, Optional
import os
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import Locale, System
import EditorStatus
from Data import GameData
from .Utils.WU_FileSelectorDialog import FileSelectorDialog


class GeneralDataPage(QtWidgets.QWidget):
    modified = QtCore.pyqtSignal()

    def __init__(self, fileKey: str, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.fileKey = fileKey
        self._currentMemberKey: Optional[str] = None
        self._ignoreChanges = False

        layout = QtWidgets.QVBoxLayout(self)

        # Splitter
        self.splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        layout.addWidget(self.splitter)

        # Left: Members list
        leftWidget = QtWidgets.QWidget()
        leftLayout = QtWidgets.QVBoxLayout(leftWidget)
        leftLayout.setContentsMargins(0, 0, 0, 0)
        self.memberList = QtWidgets.QListWidget()
        self.memberList.currentItemChanged.connect(self._onMemberSelected)
        leftLayout.addWidget(self.memberList)

        # Add button for members
        btnLayout = QtWidgets.QHBoxLayout()
        self.btnAdd = QtWidgets.QPushButton("+")
        self.btnAdd.clicked.connect(self._onAddMember)
        btnLayout.addWidget(self.btnAdd)
        leftLayout.addLayout(btnLayout)

        # Context menu
        self.memberList.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.memberList.customContextMenuRequested.connect(self._onMemberListContextMenu)

        self.splitter.addWidget(leftWidget)

        # Right: Property editor
        self.scrollArea = QtWidgets.QScrollArea()
        self.scrollArea.setWidgetResizable(True)
        self.propertyWidget = QtWidgets.QWidget()
        self.propertyLayout = QtWidgets.QFormLayout(self.propertyWidget)
        self.scrollArea.setWidget(self.propertyWidget)
        self.splitter.addWidget(self.scrollArea)

        self.splitter.setSizes([300, 700])

        self._populateMembers()

    def _populateMembers(self):
        self.memberList.blockSignals(True)
        self.memberList.clear()

        data = GameData.generalData.get(self.fileKey, {})
        members = data.get("members", {})

        for key in members.keys():
            self.memberList.addItem(key)

        self.memberList.blockSignals(False)

        if self.memberList.count() > 0:
            self.memberList.setCurrentRow(0)
        else:
            self._clearPropertyForm()

    def _onMemberSelected(self, current: QtWidgets.QListWidgetItem, previous: QtWidgets.QListWidgetItem):
        if not current:
            self._clearPropertyForm()
            return

        self._currentMemberKey = current.text()
        self._buildPropertyForm()

    def _clearPropertyForm(self):
        while self.propertyLayout.count():
            item = self.propertyLayout.takeAt(0)
            w = item.widget()
            if w:
                w.deleteLater()

    def _buildPropertyForm(self):
        self._clearPropertyForm()

        if not self.fileKey or not self._currentMemberKey:
            return

        fileData = GameData.generalData.get(self.fileKey, {})
        params = fileData.get("params", {})
        memberData = fileData.get("members", {}).get(self._currentMemberKey, {})

        self._ignoreChanges = True

        # Always add ID (key) field first
        idLabel = QtWidgets.QLabel("ID")
        idWidget = QtWidgets.QLineEdit(self._currentMemberKey)
        idWidget.setReadOnly(True)
        self.propertyLayout.addRow(idLabel, idWidget)

        for paramKey, paramDef in params.items():
            paramType = paramDef.get("type", "string")
            paramDesc = paramDef.get("desc", "")
            paramValue = memberData.get(paramKey, paramDef.get("defaultValue"))

            label = QtWidgets.QLabel(paramKey)
            if paramDesc:
                label.setToolTip(paramDesc)

            container = QtWidgets.QWidget()
            hLayout = QtWidgets.QHBoxLayout(container)
            hLayout.setContentsMargins(0, 0, 0, 0)
            hLayout.setSpacing(5)

            widget = self._createWidget(paramKey, paramType, paramValue)
            hLayout.addWidget(widget, 1)

            delBtn = QtWidgets.QPushButton("-")
            delBtn.setFixedWidth(24)
            delBtn.setToolTip(Locale.getContent("DELETE"))
            delBtn.clicked.connect(lambda _, k=paramKey: self._onRemoveParam(k))
            hLayout.addWidget(delBtn, 0)

            self.propertyLayout.addRow(label, container)

        addBtn = QtWidgets.QPushButton("+")
        addBtn.clicked.connect(self._onAddParam)
        self.propertyLayout.addRow(QtWidgets.QLabel(""), addBtn)

        self._ignoreChanges = False

    def _onRemoveParam(self, key: str):
        reply = QtWidgets.QMessageBox.question(
            self,
            Locale.getContent("CONFIRM_DELETE"),
            Locale.getContent("CONFIRM_DELETE_PARAM").format(key),
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
        )
        if reply != QtWidgets.QMessageBox.Yes:
            return

        fileData = GameData.generalData.get(self.fileKey, {})
        params = fileData.get("params", {})
        members = fileData.get("members", {})

        if key in params:
            del params[key]

            for memberKey, memberData in members.items():
                if key in memberData:
                    del memberData[key]

            self.modified.emit()

            self._buildPropertyForm()

    def _onAddParam(self):
        if not self.fileKey or not self._currentMemberKey:
            return
        dlg = QtWidgets.QDialog(self)
        dlg.setWindowTitle(Locale.getContent("ADD_PARAM"))
        form = QtWidgets.QFormLayout(dlg)

        nameEdit = QtWidgets.QLineEdit()
        typeEdit = QtWidgets.QLineEdit()
        defaultEdit = QtWidgets.QLineEdit()

        form.addRow(QtWidgets.QLabel(Locale.getContent("PARAM_NAME")), nameEdit)
        form.addRow(QtWidgets.QLabel(Locale.getContent("PARAM_TYPE")), typeEdit)
        form.addRow(QtWidgets.QLabel(Locale.getContent("DEFAULT_VALUE")), defaultEdit)

        btnBox = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        form.addRow(btnBox)

        btnBox.accepted.connect(dlg.accept)
        btnBox.rejected.connect(dlg.reject)

        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return

        name = nameEdit.text().strip()
        if not name:
            QtWidgets.QMessageBox.warning(self, Locale.getContent("ERROR"), Locale.getContent("INVALID_NAME"))
            return

        fileData = GameData.generalData.get(self.fileKey, {})
        params = fileData.get("params", {})
        members = fileData.get("members", {})

        if name in params:
            QtWidgets.QMessageBox.warning(self, Locale.getContent("ERROR"), Locale.getContent("PARAM_EXISTS"))
            return

        t = typeEdit.text().strip()
        defaultValStr = defaultEdit.text()
        defaultVal = defaultValStr

        # Simple type conversion for basic types
        if t == "int":
            try:
                defaultVal = int(defaultValStr)
            except Exception:
                defaultVal = 0
        elif t == "float":
            try:
                defaultVal = float(defaultValStr)
            except Exception:
                defaultVal = 0.0
        elif t == "bool":
            defaultVal = defaultValStr.lower() == "true"

        params[name] = {"type": t, "defaultValue": defaultVal}
        for mKey, mData in members.items():
            mData[name] = defaultVal

        self.modified.emit()
        self._buildPropertyForm()

    def _createWidget(self, key: str, paramType: str, value: Any) -> QtWidgets.QWidget:
        if paramType == "int":
            w = QtWidgets.QSpinBox()
            w.setRange(-999999, 999999)
            w.setValue(int(value) if value is not None else 0)
            w.valueChanged.connect(lambda v, k=key: self._onValueChanged(k, v))
            return w
        elif paramType == "float":
            w = QtWidgets.QDoubleSpinBox()
            w.setRange(-999999.0, 999999.0)
            w.setValue(float(value) if value is not None else 0.0)
            w.valueChanged.connect(lambda v, k=key: self._onValueChanged(k, v))
            return w
        elif paramType == "bool":
            w = QtWidgets.QCheckBox()
            w.setChecked(bool(value) if value is not None else False)
            w.stateChanged.connect(lambda v, k=key: self._onValueChanged(k, bool(v)))
            return w
        elif paramType == "file":
            container = QtWidgets.QWidget()
            layout = QtWidgets.QHBoxLayout(container)
            layout.setContentsMargins(0, 0, 0, 0)

            le = QtWidgets.QLineEdit(str(value) if value is not None else "")
            le.setReadOnly(True)
            layout.addWidget(le)

            btn = QtWidgets.QPushButton("...")
            btn.clicked.connect(lambda _, k=key, l=le: self._onFileBrowse(k, l))
            layout.addWidget(btn)

            return container
        else:
            w = QtWidgets.QLineEdit(str(value) if value is not None else "")
            w.textChanged.connect(lambda v, k=key: self._onValueChanged(k, v))
            return w

    def _onValueChanged(self, key: str, value: Any):
        if self._ignoreChanges:
            return

        if self.fileKey and self._currentMemberKey:
            fileData = GameData.generalData.get(self.fileKey, {})
            members = fileData.get("members", {})
            if self._currentMemberKey in members:
                members[self._currentMemberKey][key] = value
                self.modified.emit()

    def _onFileBrowse(self, key: str, lineEdit: QtWidgets.QLineEdit):
        startDir = os.path.join(EditorStatus.PROJ_PATH, "Assets")
        dialog = FileSelectorDialog(self, startDir, Locale.getContent("SELECT_FILE"), "All Files (*)")
        path = dialog.execSelect()
        if path:
            relPath = os.path.relpath(path, startDir)
            # Normalize path separators
            relPath = relPath.replace("\\", "/")
            lineEdit.setText(relPath)
            self._onValueChanged(key, relPath)

    def _onMemberListContextMenu(self, position):
        item = self.memberList.itemAt(position)
        if not item:
            return

        self.memberList.setCurrentItem(item)

        menu = QtWidgets.QMenu(self)

        changeIdAction = QtWidgets.QAction(Locale.getContent("CHANGE_ID"), self)
        changeIdAction.triggered.connect(self._changeMemberID)
        menu.addAction(changeIdAction)

        removeAction = QtWidgets.QAction(Locale.getContent("REMOVE_MEMBER"), self)
        removeAction.triggered.connect(self._onRemoveMember)
        menu.addAction(removeAction)

        menu.exec_(self.memberList.mapToGlobal(position))

    def _changeMemberID(self):
        current = self.memberList.currentItem()
        if not current:
            return

        oldID = current.text()
        newID, ok = QtWidgets.QInputDialog.getText(
            self, Locale.getContent("CHANGE_ID"), Locale.getContent("ENTER_ID"), QtWidgets.QLineEdit.Normal, oldID
        )

        if ok and newID and newID != oldID:
            fileData = GameData.generalData.get(self.fileKey, {})
            members = fileData.get("members", {})
            if newID in members:
                QtWidgets.QMessageBox.warning(self, Locale.getContent("ERROR"), "ID already exists!")
                return

            # Rename in data
            if oldID in members:
                members[newID] = members.pop(oldID)

                # Rename in list
                current.setText(newID)
                self._currentMemberKey = newID

                # Refresh right panel to update ID field
                self._buildPropertyForm()

                self.modified.emit()

    def _onAddMember(self):
        if not self.fileKey:
            return

        text, ok = QtWidgets.QInputDialog.getText(self, Locale.getContent("NEW_MEMBER"), Locale.getContent("ENTER_ID"))
        if ok and text:
            fileData = GameData.generalData.get(self.fileKey, {})
            members = fileData.get("members", {})
            if text in members:
                QtWidgets.QMessageBox.warning(self, "Error", "ID already exists!")
                return

            # Create new member with default values
            params = fileData.get("params", {})
            newMember = {}
            for pk, pv in params.items():
                newMember[pk] = pv.get("defaultValue")

            members[text] = newMember
            self.memberList.addItem(text)
            self.memberList.setCurrentRow(self.memberList.count() - 1)
            self.modified.emit()

    def _onRemoveMember(self):
        row = self.memberList.currentRow()
        if row < 0:
            return

        item = self.memberList.takeItem(row)
        key = item.text()

        if self.fileKey:
            fileData = GameData.generalData.get(self.fileKey, {})
            members = fileData.get("members", {})
            if key in members:
                del members[key]
                self.modified.emit()

        if self.memberList.count() == 0:
            self._clearPropertyForm()


class GeneralDataEditor(QtWidgets.QMainWindow):
    modified = QtCore.pyqtSignal()

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.setWindowTitle(Locale.getContent("GENERAL_DATA_EDITOR"))
        self.resize(1000, 600)

        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        mainLayout = QtWidgets.QVBoxLayout(central)

        # Tab Widget
        self.tabWidget = QtWidgets.QTabWidget()
        self.tabWidget.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.tabWidget.customContextMenuRequested.connect(self._onTabContextMenu)
        mainLayout.addWidget(self.tabWidget)

        self._populateFiles()

    def _onTabContextMenu(self, position):
        menu = QtWidgets.QMenu(self)

        # Always allow adding new data type
        addAction = QtWidgets.QAction(Locale.getContent("NEW_DATA_TYPE"), self)
        addAction.triggered.connect(self._onAddDataType)
        menu.addAction(addAction)

        # Check if we clicked on a tab
        tabIndex = self.tabWidget.tabBar().tabAt(position)
        if tabIndex >= 0:
            menu.addSeparator()

            # Using lambda in a loop or direct binding can be tricky with context menus,
            # but here we capture the current tabIndex immediately.
            renameAction = QtWidgets.QAction(Locale.getContent("RENAME_DATA_TYPE"), self)
            renameAction.triggered.connect(lambda checked=False, idx=tabIndex: self._onRenameDataType(idx))
            menu.addAction(renameAction)

            deleteAction = QtWidgets.QAction(Locale.getContent("DELETE_DATA_TYPE"), self)
            deleteAction.triggered.connect(lambda checked=False, idx=tabIndex: self._onRemoveDataType(idx))
            menu.addAction(deleteAction)

        menu.exec_(self.tabWidget.mapToGlobal(position))

    def _onAddDataType(self):
        text, ok = QtWidgets.QInputDialog.getText(
            self, Locale.getContent("NEW_DATA_TYPE"), Locale.getContent("ENTER_DATA_TYPE_NAME")
        )
        if ok and text:
            data = getattr(GameData, "generalData", {})
            if text in data:
                QtWidgets.QMessageBox.warning(self, Locale.getContent("ERROR"), Locale.getContent("DATA_TYPE_EXISTS"))
                return

            # Create new data type structure
            data[text] = {"params": {}, "members": {}}

            # Re-populate to maintain sort order and fix display issues
            self._populateFiles()

            # Select the new tab
            for i in range(self.tabWidget.count()):
                if self.tabWidget.tabText(i) == text:
                    self.tabWidget.setCurrentIndex(i)
                    break

            self.modified.emit()

    def _onRenameDataType(self, index: int):
        oldKey = self.tabWidget.tabText(index)

        text, ok = QtWidgets.QInputDialog.getText(
            self,
            Locale.getContent("RENAME_DATA_TYPE"),
            Locale.getContent("ENTER_DATA_TYPE_NAME"),
            QtWidgets.QLineEdit.Normal,
            oldKey,
        )
        if ok and text and text != oldKey:
            data = getattr(GameData, "generalData", {})
            if text in data:
                QtWidgets.QMessageBox.warning(self, Locale.getContent("ERROR"), Locale.getContent("DATA_TYPE_EXISTS"))
                return

            # Rename key in dictionary
            if oldKey in data:
                data[text] = data.pop(oldKey)

            # Re-populate to maintain sort order
            self._populateFiles()

            # Select the renamed tab
            for i in range(self.tabWidget.count()):
                if self.tabWidget.tabText(i) == text:
                    self.tabWidget.setCurrentIndex(i)
                    break

            self.modified.emit()

    def _onRemoveDataType(self, index: int):
        key = self.tabWidget.tabText(index)

        reply = QtWidgets.QMessageBox.question(
            self,
            Locale.getContent("CONFIRM_DELETE"),
            Locale.getContent("CONFIRM_DELETE_DATA_TYPE").format(key),
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
        )

        if reply == QtWidgets.QMessageBox.Yes:
            data = getattr(GameData, "generalData", {})
            if key in data:
                del data[key]

            self.tabWidget.removeTab(index)
            self.modified.emit()

    def _populateFiles(self):
        self.tabWidget.clear()
        data = getattr(GameData, "generalData", {})

        # Sort keys to ensure consistent order
        for key in sorted(data.keys()):
            page = GeneralDataPage(key, self)
            page.modified.connect(self.modified.emit)
            self.tabWidget.addTab(page, key)
