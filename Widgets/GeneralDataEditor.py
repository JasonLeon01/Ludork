# -*- encoding: utf-8 -*-

from typing import Any, Optional
import copy
import os
import re
import sys
from PyQt5 import QtCore, QtWidgets
from EditorGlobal import EditorStatus, GameData
from .Utils import FileSelectorDialog, OpenFormDialog, OpenItemSelectorDialog, OpenSingleRowDialog


REFERENCE_KIND_GENERAL = "general"
REFERENCE_KIND_ANIMATION = "animation"


class ListEditorWidget(QtWidgets.QWidget):
    DATA_CHANGED = QtCore.pyqtSignal(list)

    def __init__(self, data: list, referenceOptions: Optional[list[str]] = None, parent=None):
        super().__init__(parent)
        self.dataList = list(data) if isinstance(data, list) else []
        self.referenceOptions = referenceOptions
        self._setupUI()

    def _setupUI(self):
        if self.layout():
            QtWidgets.QWidget().setLayout(self.layout())

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)

        for i, val in enumerate(self.dataList):
            row = QtWidgets.QWidget()
            rowLayout = QtWidgets.QHBoxLayout(row)
            rowLayout.setContentsMargins(0, 0, 0, 0)

            editor = self._createItemEditor(i, val)

            delBtn = QtWidgets.QPushButton("-")
            delBtn.setFixedWidth(24)
            delBtn.clicked.connect(lambda _, idx=i: self._onItemRemoved(idx))

            rowLayout.addWidget(editor)
            rowLayout.addWidget(delBtn)
            layout.addWidget(row)

        addBtn = QtWidgets.QPushButton("+")
        addBtn.clicked.connect(self._onItemAdded)
        layout.addWidget(addBtn)

    def _createItemEditor(self, index: int, value: Any) -> QtWidgets.QWidget:
        if self.referenceOptions is None:
            le = QtWidgets.QLineEdit(str(value))
            le.textChanged.connect(lambda text, idx=index: self._onItemChanged(idx, text))
            return le

        combo = QtWidgets.QComboBox()
        combo.setEditable(False)
        currentValue = str(value) if value is not None else ""
        combo.addItem(ELOC("NO_SELECTION"), "")
        if currentValue and currentValue not in self.referenceOptions:
            combo.addItem(currentValue, currentValue)
        for option in self.referenceOptions:
            combo.addItem(option, option)
        currentIndex = combo.findData(currentValue)
        combo.setCurrentIndex(currentIndex if currentIndex >= 0 else 0)
        combo.currentIndexChanged.connect(
            lambda idx, itemIndex=index, c=combo: self._onItemChanged(itemIndex, str(c.itemData(idx) or ""))
        )
        return combo

    def _onItemChanged(self, index, text):
        if 0 <= index < len(self.dataList):
            self.dataList[index] = text
            self.DATA_CHANGED.emit(self.dataList)

    def _onItemRemoved(self, index):
        if 0 <= index < len(self.dataList):
            self.dataList.pop(index)
            self._setupUI()
            self.DATA_CHANGED.emit(self.dataList)

    def _onItemAdded(self):
        self.dataList.append("")
        self._setupUI()
        self.DATA_CHANGED.emit(self.dataList)


class DictEditorWidget(QtWidgets.QWidget):
    DATA_CHANGED = QtCore.pyqtSignal(dict)

    def __init__(self, data: dict, keyReferenceOptions: Optional[list[str]] = None, parent=None):
        super().__init__(parent)
        self.keyReferenceOptions = keyReferenceOptions
        self.dataItems: list[list[str]] = []
        if isinstance(data, dict):
            for key, val in data.items():
                self.dataItems.append([str(key), str(val)])
        self._setupUI()

    def _setupUI(self):
        if self.layout():
            QtWidgets.QWidget().setLayout(self.layout())

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)

        for i, pair in enumerate(self.dataItems):
            row = QtWidgets.QWidget()
            rowLayout = QtWidgets.QHBoxLayout(row)
            rowLayout.setContentsMargins(0, 0, 0, 0)

            keyEdit = self._createKeyEditor(i, pair[0])
            valueEdit = QtWidgets.QLineEdit(pair[1])
            valueEdit.textChanged.connect(lambda text, idx=i: self._onValueChanged(idx, text))

            delBtn = QtWidgets.QPushButton("-")
            delBtn.setFixedWidth(24)
            delBtn.clicked.connect(lambda _, idx=i: self._onItemRemoved(idx))

            rowLayout.addWidget(keyEdit, 1)
            rowLayout.addWidget(valueEdit, 1)
            rowLayout.addWidget(delBtn)
            layout.addWidget(row)

        addBtn = QtWidgets.QPushButton("+")
        addBtn.clicked.connect(self._onItemAdded)
        layout.addWidget(addBtn)

    def _createKeyEditor(self, index: int, value: Any) -> QtWidgets.QWidget:
        if self.keyReferenceOptions is None:
            keyEdit = QtWidgets.QLineEdit(str(value))
            keyEdit.textChanged.connect(lambda text, idx=index: self._onKeyChanged(idx, text))
            return keyEdit

        combo = QtWidgets.QComboBox()
        combo.setEditable(False)
        currentValue = str(value) if value is not None else ""
        combo.addItem(ELOC("NO_SELECTION"), "")
        if currentValue and currentValue not in self.keyReferenceOptions:
            combo.addItem(currentValue, currentValue)
        for option in self.keyReferenceOptions:
            combo.addItem(option, option)
        currentIndex = combo.findData(currentValue)
        combo.setCurrentIndex(currentIndex if currentIndex >= 0 else 0)
        combo.currentIndexChanged.connect(
            lambda idx, itemIndex=index, c=combo: self._onKeyChanged(itemIndex, str(c.itemData(idx) or ""))
        )
        return combo

    def _buildDict(self) -> dict:
        result: dict[str, str] = {}
        for key, val in self.dataItems:
            key = key.strip()
            if key:
                result[key] = val
        return result

    def _onKeyChanged(self, index: int, text: str):
        if 0 <= index < len(self.dataItems):
            self.dataItems[index][0] = text
            self.DATA_CHANGED.emit(self._buildDict())

    def _onValueChanged(self, index: int, text: str):
        if 0 <= index < len(self.dataItems):
            self.dataItems[index][1] = text
            self.DATA_CHANGED.emit(self._buildDict())

    def _onItemRemoved(self, index: int):
        if 0 <= index < len(self.dataItems):
            self.dataItems.pop(index)
            self._setupUI()
            self.DATA_CHANGED.emit(self._buildDict())

    def _onItemAdded(self):
        self.dataItems.append(["", ""])
        self._setupUI()
        self.DATA_CHANGED.emit(self._buildDict())


class TupleEditorWidget(QtWidgets.QWidget):
    DATA_CHANGED = QtCore.pyqtSignal(list)

    def __init__(self, data: list, size: int, parent=None):
        super().__init__(parent)
        self.targetSize = size
        if isinstance(data, list):
            self.dataList = list(data)[:size]
            while len(self.dataList) < size:
                self.dataList.append("")
        else:
            self.dataList = [""] * size

        self._setupUI()

    def _setupUI(self):
        if self.layout():
            QtWidgets.QWidget().setLayout(self.layout())

        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)

        layout.addWidget(QtWidgets.QLabel("("))

        for i, val in enumerate(self.dataList):
            le = QtWidgets.QLineEdit(str(val))
            le.textChanged.connect(lambda text, idx=i: self._onItemChanged(idx, text))
            layout.addWidget(le)

            if i < len(self.dataList) - 1:
                layout.addWidget(QtWidgets.QLabel(","))

        layout.addWidget(QtWidgets.QLabel(")"))

    def _onItemChanged(self, index, text):
        if 0 <= index < len(self.dataList):
            self.dataList[index] = text
            self.DATA_CHANGED.emit(self.dataList)


class GeneralDataPage(QtWidgets.QWidget):
    MODIFIED = QtCore.pyqtSignal()
    REQUEST_BLUEPRINT_EDIT = QtCore.pyqtSignal(str, str, list)
    READ_ONLY_LINE_EDIT_STYLE = "color: #909090;"
    PARAM_TYPE_OPTIONS = ["string", "int", "float", "bool", "file", "list", "dict"]
    PARAM_TYPE_TOOLTIP_KEYS = {
        "string": "GENERAL_DATA_TYPE_TIP_STRING",
        "int": "GENERAL_DATA_TYPE_TIP_INT",
        "float": "GENERAL_DATA_TYPE_TIP_FLOAT",
        "bool": "GENERAL_DATA_TYPE_TIP_BOOL",
        "file": "GENERAL_DATA_TYPE_TIP_FILE",
        "list": "GENERAL_DATA_TYPE_TIP_LIST",
        "dict": "GENERAL_DATA_TYPE_TIP_DICT",
    }
    PARAM_DEFAULT_TOOLTIP_KEYS = {
        "string": "GENERAL_DATA_DEFAULT_TIP_STRING",
        "int": "GENERAL_DATA_DEFAULT_TIP_INT",
        "float": "GENERAL_DATA_DEFAULT_TIP_FLOAT",
        "bool": "GENERAL_DATA_DEFAULT_TIP_BOOL",
        "file": "GENERAL_DATA_DEFAULT_TIP_FILE",
        "list": "GENERAL_DATA_DEFAULT_TIP_LIST",
        "dict": "GENERAL_DATA_DEFAULT_TIP_DICT",
    }

    @staticmethod
    def _getParamReference(paramDef: dict) -> Optional[dict[str, str]]:
        if not GeneralDataPage._isParamReferenceAllowed(paramDef):
            return None
        reference = paramDef.get("reference")
        if not isinstance(reference, dict):
            return None
        kind = reference.get("kind")
        key = reference.get("key")
        if kind == REFERENCE_KIND_ANIMATION:
            return {"kind": REFERENCE_KIND_ANIMATION, "key": ""}
        if kind == REFERENCE_KIND_GENERAL and isinstance(key, str) and key:
            return {"kind": REFERENCE_KIND_GENERAL, "key": key}
        return None

    @staticmethod
    def _isParamReferenceAllowed(paramDef: dict) -> bool:
        return paramDef.get("type", "string") in ("string", "list", "dict")

    def __init__(self, fileKey: str, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.fileKey = fileKey
        self._currentMemberKey: Optional[str] = None
        self._ignoreChanges = False

        layout = QtWidgets.QVBoxLayout(self)

        self.splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        layout.addWidget(self.splitter)

        leftWidget = QtWidgets.QWidget()
        leftLayout = QtWidgets.QVBoxLayout(leftWidget)
        leftLayout.setContentsMargins(0, 0, 0, 0)
        self.memberList = QtWidgets.QListWidget()
        self.memberList.currentItemChanged.connect(self._onMemberSelected)
        leftLayout.addWidget(self.memberList)

        btnLayout = QtWidgets.QHBoxLayout()
        self.btnAdd = QtWidgets.QPushButton("+")
        self.btnAdd.clicked.connect(self._onAddMember)
        btnLayout.addWidget(self.btnAdd)
        leftLayout.addLayout(btnLayout)

        self.memberList.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.memberList.customContextMenuRequested.connect(self._onMemberListContextMenu)

        self.splitter.addWidget(leftWidget)

        rightWidget = QtWidgets.QWidget()
        rightLayout = QtWidgets.QVBoxLayout(rightWidget)
        rightLayout.setContentsMargins(0, 0, 0, 0)

        self.linkedTypeBar = QtWidgets.QWidget()
        linkedTypeLayout = QtWidgets.QHBoxLayout(self.linkedTypeBar)
        linkedTypeLayout.setContentsMargins(0, 0, 0, 4)
        self.linkedTypeLabel = QtWidgets.QLabel("")
        self.linkedTypeLabel.setStyleSheet("color: #88aaff; font-weight: bold;")
        linkedTypeLayout.addWidget(self.linkedTypeLabel)
        linkedTypeLayout.addStretch()

        self.btnEditBlueprint = QtWidgets.QPushButton(ELOC("EDIT_BLUEPRINT"))
        self.btnEditBlueprint.setEnabled(False)
        self.btnEditBlueprint.clicked.connect(self._onEditBlueprint)
        linkedTypeLayout.addWidget(self.btnEditBlueprint)

        rightLayout.addWidget(self.linkedTypeBar)

        self.scrollArea = QtWidgets.QScrollArea()
        self.scrollArea.setWidgetResizable(True)
        self.propertyWidget = QtWidgets.QWidget()
        self.propertyLayout = QtWidgets.QFormLayout(self.propertyWidget)
        self.scrollArea.setWidget(self.propertyWidget)
        rightLayout.addWidget(self.scrollArea)

        self.splitter.addWidget(rightWidget)
        self.splitter.setSizes([300, 700])

        self._updateLinkedTypeBar()
        self._populateMembers()

    def _getLinkedType(self) -> Optional[str]:
        data = GameData.generalData.get(self.fileKey, {})
        return data.get("linkedType", None)

    def _getLinkedEvents(self) -> list:
        events = self._getDefaultLinkedEvents()
        return events + self._getMemberCustomEvents(self._currentMemberKey, events)

    def _getDefaultLinkedEvents(self) -> list:
        data = GameData.generalData.get(self.fileKey, {})
        linkedType = self._getLinkedType()
        if linkedType:
            events = self._extractRegisteredEventsFromSource(linkedType)
            if events is not None:
                return events
            cls = self._resolveLinkedClass(linkedType)
            if cls:
                return self._extractRegisteredEvents(cls)
        events = data.get("events", [])
        return events if isinstance(events, list) else []

    def _getMemberCustomEvents(self, memberKey: Optional[str], defaultEvents: list) -> list:
        if not memberKey:
            return []

        fileData = GameData.generalData.get(self.fileKey, {})
        memberData = fileData.get("members", {}).get(memberKey, {})
        graph = memberData.get("_graph") if isinstance(memberData, dict) else None
        if not isinstance(graph, dict):
            return []

        nodeGraph = graph.get("nodeGraph", {})
        startNodes = graph.get("startNodes", {})
        if not isinstance(nodeGraph, dict):
            return []
        if not isinstance(startNodes, dict):
            startNodes = {}

        defaultSet = set(defaultEvents)
        events = []
        for event, eventData in nodeGraph.items():
            if event in defaultSet:
                continue
            if self._isNonEmptyGraphEvent(eventData, startNodes.get(event)):
                events.append(event)
        return events

    @staticmethod
    def _isNonEmptyGraphEvent(eventData: Any, startNode: Any) -> bool:
        if startNode is not None:
            return True
        if not isinstance(eventData, dict):
            return False
        return bool(eventData.get("nodes") or eventData.get("links"))

    @classmethod
    def _makeReadOnlyLineEdit(cls, text: str = "") -> QtWidgets.QLineEdit:
        lineEdit = QtWidgets.QLineEdit(text)
        lineEdit.setReadOnly(True)
        lineEdit.setStyleSheet(cls.READ_ONLY_LINE_EDIT_STYLE)
        return lineEdit

    @staticmethod
    def _normalizeMemberGraph(memberData: dict, events: list) -> None:
        graph = memberData.get("_graph")
        if not isinstance(graph, dict):
            graph = {}
            memberData["_graph"] = graph

        nodeGraph = graph.get("nodeGraph")
        if not isinstance(nodeGraph, dict):
            nodeGraph = {}
            graph["nodeGraph"] = nodeGraph

        startNodes = graph.get("startNodes")
        if not isinstance(startNodes, dict):
            startNodes = {}
            graph["startNodes"] = startNodes

        orderedEvents = []
        for event in events:
            if event not in orderedEvents:
                orderedEvents.append(event)

        for event in orderedEvents:
            if event not in nodeGraph:
                nodeGraph[event] = {"nodes": [], "links": []}
            if event not in startNodes:
                startNodes[event] = None

        for event in list(nodeGraph.keys()):
            if event not in orderedEvents:
                del nodeGraph[event]
        for event in list(startNodes.keys()):
            if event not in orderedEvents:
                del startNodes[event]

    def _resolveLinkedClass(self, typeName: str) -> Optional[type]:
        try:
            cls = GameData.classDict.get(f"Source.Infos.{typeName}", EditorStatus.PROJ_PATH)
            if cls and cls is not EditorStatus.PROJ_PATH:
                return cls
        except Exception:
            pass

        try:
            sourcePath = os.path.join(EditorStatus.PROJ_PATH, "Source")
            if sourcePath not in sys.path:
                sys.path.insert(0, sourcePath)
            mod = __import__(f"Source.Infos.{typeName}", fromlist=[typeName])
            return getattr(mod, typeName, None)
        except Exception:
            pass

        return None

    @staticmethod
    def _extractRegisteredEvents(targetCls) -> list:
        events = []
        for name in dir(targetCls):
            if name.startswith("_"):
                continue
            attr = getattr(targetCls, name, None)
            if callable(attr) and getattr(attr, "_eventSignature", False):
                events.append(name)
        return events

    @staticmethod
    def _extractRegisteredEventsFromSource(typeName: str) -> Optional[list]:
        sourcePath = os.path.join(EditorStatus.PROJ_PATH, "Source", "Infos", f"{typeName}.py")
        if not os.path.exists(sourcePath):
            return None
        events = []
        try:
            with open(sourcePath, "r", encoding="utf-8") as f:
                lines = f.readlines()
            for i, line in enumerate(lines):
                if "@RegisterEvent" not in line:
                    continue
                for nextLine in lines[i + 1 :]:
                    stripped = nextLine.strip()
                    if not stripped or stripped.startswith("@"):
                        continue
                    match = re.match(r"\s*def\s+(\w+)\s*\(", nextLine)
                    if match:
                        events.append(match.group(1))
                    break
        except Exception:
            return None
        return events

    def _updateLinkedTypeBar(self):
        linkedType = self._getLinkedType()
        if linkedType:
            self.linkedTypeLabel.setText(f"[{ELOC('LINKED_TYPE')}: {linkedType}]")
            self.linkedTypeBar.setVisible(True)
        else:
            self.linkedTypeBar.setVisible(False)

    def _onEditBlueprint(self):
        if not self._currentMemberKey:
            return
        events = self._getLinkedEvents()
        if events:
            self.REQUEST_BLUEPRINT_EDIT.emit(self.fileKey, self._currentMemberKey, events)

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
            self.btnEditBlueprint.setEnabled(False)
            return

        self._currentMemberKey = current.text()
        self._buildPropertyForm()

        linkedType = self._getLinkedType()
        events = self._getLinkedEvents()
        self.btnEditBlueprint.setEnabled(bool(linkedType and events))

    def _clearPropertyForm(self):
        while self.propertyLayout.count():
            item = self.propertyLayout.takeAt(0)
            if item is None:
                continue
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

        idLabel = QtWidgets.QLabel("ID")
        idWidget = self._makeReadOnlyLineEdit(self._currentMemberKey)
        self.propertyLayout.addRow(idLabel, idWidget)

        for paramKey, paramDef in params.items():
            paramType = paramDef.get("type", "string")
            paramDesc = paramDef.get("desc", "")
            paramValue = memberData.get(paramKey, self._getDefaultMemberValue(paramDef))

            label = QtWidgets.QLabel(paramKey)
            if paramDesc:
                label.setToolTip(paramDesc)
            reference = self._getParamReference(paramDef)
            if reference:
                refLabel = self._formatReferenceLabel(reference)
                label.setText(f"{paramKey} [{refLabel}]")
                label.setToolTip(f"{label.toolTip()}\n{refLabel}" if label.toolTip() else refLabel)
            if self._isParamReferenceAllowed(paramDef):
                label.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
                label.customContextMenuRequested.connect(
                    lambda pos, k=paramKey, l=label: self._onParamLabelContextMenu(k, l, pos)
                )

            container = QtWidgets.QWidget()
            hLayout = QtWidgets.QHBoxLayout(container)
            hLayout.setContentsMargins(0, 0, 0, 0)
            hLayout.setSpacing(5)

            widget = self._createWidget(paramKey, paramDef, paramValue)
            hLayout.addWidget(widget, 1)

            delBtn = QtWidgets.QPushButton("-")
            delBtn.setFixedWidth(24)
            delBtn.setToolTip(ELOC("DELETE"))
            delBtn.clicked.connect(lambda _, k=paramKey: self._onRemoveParam(k))
            hLayout.addWidget(delBtn, 0)

            self.propertyLayout.addRow(label, container)

        addBtn = QtWidgets.QPushButton("+")
        addBtn.clicked.connect(self._onAddParam)
        self.propertyLayout.addRow(QtWidgets.QLabel(""), addBtn)

        self._ignoreChanges = False

    def _formatReferenceLabel(self, reference: dict[str, str]) -> str:
        if reference.get("kind") == REFERENCE_KIND_ANIMATION:
            return ELOC("REFERENCE_TYPE_ANIMATION")
        return reference.get("key", "")

    def _onParamLabelContextMenu(self, key: str, label: QtWidgets.QLabel, position: QtCore.QPoint):
        fileData = GameData.generalData.get(self.fileKey, {})
        params = fileData.get("params", {})
        paramDef = params.get(key)
        if not isinstance(paramDef, dict):
            return
        if not self._isParamReferenceAllowed(paramDef):
            return

        menu = QtWidgets.QMenu(self)
        referenceMenu = menu.addMenu(ELOC("ADD_REFERENCE"))

        for dataKey in sorted(GameData.generalData.keys()):
            action = QtWidgets.QAction(dataKey, self)
            action.triggered.connect(
                lambda checked=False, k=key, target=dataKey: self._setParamReference(
                    k, {"kind": REFERENCE_KIND_GENERAL, "key": target}
                )
            )
            referenceMenu.addAction(action)

        referenceMenu.addSeparator()
        animationAction = QtWidgets.QAction(ELOC("REFERENCE_TYPE_ANIMATION"), self)
        animationAction.triggered.connect(
            lambda checked=False, k=key: self._setParamReference(k, {"kind": REFERENCE_KIND_ANIMATION})
        )
        referenceMenu.addAction(animationAction)

        if self._getParamReference(paramDef):
            menu.addSeparator()
            clearAction = QtWidgets.QAction(ELOC("REMOVE_REFERENCE"), self)
            clearAction.triggered.connect(lambda checked=False, k=key: self._setParamReference(k, None))
            menu.addAction(clearAction)

        from Utils import PluginSystem

        PluginSystem.AddRightClickActions(
            menu,
            self,
            "generalDataParamLabel",
            "always",
            {"key": key, "definition": paramDef},
        )
        menu.exec_(label.mapToGlobal(position))

    def _setParamReference(self, key: str, reference: Optional[dict[str, str]]):
        fileData = GameData.generalData.get(self.fileKey, {})
        params = fileData.get("params", {})
        paramDef = params.get(key)
        if not isinstance(paramDef, dict):
            return

        if reference:
            paramDef["reference"] = reference
        elif "reference" in paramDef:
            del paramDef["reference"]
        else:
            return

        self.MODIFIED.emit()
        self._buildPropertyForm()

    def _onRemoveParam(self, key: str):
        reply = QtWidgets.QMessageBox.question(
            self,
            ELOC("CONFIRM_DELETE"),
            ELOC("CONFIRM_DELETE_PARAM").format(key),
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

            self.MODIFIED.emit()

            self._buildPropertyForm()

    def _addParamFormFields(self) -> list[dict[str, object]]:
        return [
            {
                "name": "name",
                "label": ELOC("PARAM_NAME"),
                "type": "text",
                "tooltipKey": "GENERAL_DATA_PARAM_NAME_TIP",
            },
            {
                "name": "type",
                "label": ELOC("PARAM_TYPE"),
                "type": "combo",
                "options": self.PARAM_TYPE_OPTIONS,
                "initialValue": self.PARAM_TYPE_OPTIONS[0] if self.PARAM_TYPE_OPTIONS else "",
                "tooltipKeys": self.PARAM_TYPE_TOOLTIP_KEYS,
            },
            {
                "name": "defaultValue",
                "label": ELOC("DEFAULT_VALUE"),
                "type": "text",
                "tooltipSourceField": "type",
                "tooltipKeys": self.PARAM_DEFAULT_TOOLTIP_KEYS,
            },
        ]

    def _onAddParam(self):
        if not self.fileKey or not self._currentMemberKey:
            return
        OpenFormDialog(
            self,
            ELOC("ADD_PARAM"),
            self._addParamFormFields(),
            onAccepted=lambda result: self._addParam(
                str(result.get("name", "")).strip(),
                str(result.get("type", "")).strip(),
                str(result.get("defaultValue", "")),
            ),
        )

    def _addParam(self, name: str, t: str, defaultValStr: str) -> None:
        if not name:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("INVALID_NAME"))
            return

        fileData = GameData.generalData.get(self.fileKey, {})
        params = fileData.get("params", {})
        members = fileData.get("members", {})

        if name in params:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("PARAM_EXISTS"))
            return

        defaultVal = defaultValStr

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
        elif t == "list":
            try:
                if defaultValStr.strip().startswith("["):
                    import ast

                    defaultVal = ast.literal_eval(defaultValStr)
                    if not isinstance(defaultVal, list):
                        defaultVal = []
                else:
                    defaultVal = []
            except (ValueError, SyntaxError, TypeError):
                defaultVal = []
        elif t == "dict":
            try:
                if defaultValStr.strip().startswith("{"):
                    import ast

                    defaultVal = ast.literal_eval(defaultValStr)
                    if not isinstance(defaultVal, dict):
                        defaultVal = {}
                    else:
                        defaultVal = {str(k): str(v) for k, v in defaultVal.items()}
                else:
                    defaultVal = {}
            except (ValueError, SyntaxError, TypeError):
                defaultVal = {}
        elif t.startswith("tuple"):
            match = re.match(r"tuple\[(\d+)\]", t)
            if not match:
                QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("TUPLE_SIZE_ERR"))
                return

            size = int(match.group(1))
            try:
                if defaultValStr.strip().startswith("[") or defaultValStr.strip().startswith("("):
                    import ast

                    val = ast.literal_eval(defaultValStr)
                    defaultVal = list(val) if isinstance(val, (list, tuple)) else []
                else:
                    defaultVal = []
            except (ValueError, SyntaxError, TypeError):
                defaultVal = []

            defaultVal = defaultVal[:size]
            while len(defaultVal) < size:
                defaultVal.append("")

        params[name] = {"type": t, "defaultValue": defaultVal}
        for mKey, mData in members.items():
            mData[name] = self._getDefaultMemberValue(params[name])

        self.MODIFIED.emit()
        self._buildPropertyForm()

    def _getDefaultMemberValue(self, paramDef: dict) -> Any:
        if paramDef.get("type", "string") == "file":
            return ""
        return paramDef.get("defaultValue")

    def _getFileBrowseRoot(self, paramDef: dict) -> str:
        assetsRoot = os.path.abspath(os.path.join(EditorStatus.PROJ_PATH, "Assets"))
        base = str(paramDef.get("defaultValue", "") or "").strip().strip("/\\")
        if not base:
            return assetsRoot

        normalizedBase = os.path.normpath(base.replace("\\", os.sep).replace("/", os.sep))
        if os.path.isabs(normalizedBase) or normalizedBase == ".." or normalizedBase.startswith(".." + os.sep):
            return assetsRoot

        root = os.path.abspath(os.path.join(assetsRoot, normalizedBase))
        try:
            if os.path.commonpath([assetsRoot, root]) != assetsRoot:
                return assetsRoot
        except ValueError:
            return assetsRoot

        if os.path.isfile(root):
            root = os.path.dirname(root)
        if not os.path.isdir(root):
            return assetsRoot
        return root

    def _getReferenceOptions(self, reference: dict[str, str]) -> list[str]:
        if reference.get("kind") == REFERENCE_KIND_ANIMATION:
            return sorted(str(key) for key in GameData.animationsData.keys())

        if reference.get("kind") == REFERENCE_KIND_GENERAL:
            fileData = GameData.generalData.get(reference.get("key", ""), {})
            members = fileData.get("members", {}) if isinstance(fileData, dict) else {}
            if isinstance(members, dict):
                return sorted(str(key) for key in members.keys())

        return []

    def _createReferenceCombo(self, key: str, reference: dict[str, str], value: Any) -> QtWidgets.QComboBox:
        combo = QtWidgets.QComboBox()
        combo.setEditable(False)
        currentValue = str(value) if value is not None else ""
        combo.addItem(ELOC("NO_SELECTION"), "")

        options = self._getReferenceOptions(reference)
        if currentValue and currentValue not in options:
            combo.addItem(currentValue, currentValue)

        for option in options:
            combo.addItem(option, option)

        index = combo.findData(currentValue)
        combo.setCurrentIndex(index if index >= 0 else 0)
        combo.currentIndexChanged.connect(
            lambda idx, k=key, c=combo: self._onValueChanged(k, str(c.itemData(idx) or ""))
        )
        return combo

    def _createWidget(self, key: str, paramDef: dict, value: Any) -> QtWidgets.QWidget:
        reference = self._getParamReference(paramDef)
        paramType = paramDef.get("type", "string")
        if reference and paramType == "string":
            return self._createReferenceCombo(key, reference, value)

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

            le = self._makeReadOnlyLineEdit(str(value) if value is not None else "")
            layout.addWidget(le)

            btn = QtWidgets.QPushButton("...")
            btn.clicked.connect(lambda _, k=key, l=le, p=paramDef: self._onFileBrowse(k, l, p))
            layout.addWidget(btn)

            return container
        elif paramType == "list":
            options = self._getReferenceOptions(reference) if reference else None
            w = ListEditorWidget(value if isinstance(value, list) else [], options)
            w.DATA_CHANGED.connect(lambda v, k=key: self._onValueChanged(k, v))
            return w
        elif paramType == "dict":
            options = self._getReferenceOptions(reference) if reference else None
            w = DictEditorWidget(value if isinstance(value, dict) else {}, options)
            w.DATA_CHANGED.connect(lambda v, k=key: self._onValueChanged(k, v))
            return w
        elif paramType.startswith("tuple"):
            match = re.match(r"tuple\[(\d+)\]", paramType)
            if match:
                size = int(match.group(1))
                val = value if isinstance(value, (list, tuple)) else []
                w = TupleEditorWidget(list(val), size)
                w.DATA_CHANGED.connect(lambda v, k=key: self._onValueChanged(k, v))
                return w
            else:
                return QtWidgets.QLabel("Invalid Tuple Type")
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
                self.MODIFIED.emit()

    def _onFileBrowse(self, key: str, lineEdit: QtWidgets.QLineEdit, paramDef: dict):
        assetsRoot = os.path.abspath(os.path.join(EditorStatus.PROJ_PATH, "Assets"))
        startDir = self._getFileBrowseRoot(paramDef)
        dialog = FileSelectorDialog(self, startDir, FileSelectorDialog.allFilesFilter())
        dialog.openSelect(
            lambda path: self._applyFileBrowseSelection(key, lineEdit, path, assetsRoot)
        )

    def _applyFileBrowseSelection(
        self,
        key: str,
        lineEdit: QtWidgets.QLineEdit,
        path: str,
        assetsRoot: str,
    ) -> None:
        if not path:
            return
        relPath = os.path.relpath(path, assetsRoot)
        relPath = relPath.replace("\\", "/")
        lineEdit.setText(relPath)
        self._onValueChanged(key, relPath)

    def _onMemberListContextMenu(self, position):
        item = self.memberList.itemAt(position)
        if not item:
            menu = QtWidgets.QMenu(self)
            from Utils import PluginSystem

            if PluginSystem.AddRightClickActions(menu, self, "generalDataMember", "empty", None) > 0:
                menu.exec_(self.memberList.mapToGlobal(position))
            return

        self.memberList.setCurrentItem(item)

        menu = QtWidgets.QMenu(self)

        changeIdAction = QtWidgets.QAction(ELOC("CHANGE_ID"), self)
        changeIdAction.triggered.connect(self._changeMemberID)
        menu.addAction(changeIdAction)

        duplicateAction = QtWidgets.QAction(ELOC("DUPLICATE_MEMBER"), self)
        duplicateAction.triggered.connect(self._onDuplicateMember)
        menu.addAction(duplicateAction)

        removeAction = QtWidgets.QAction(ELOC("REMOVE_MEMBER"), self)
        removeAction.triggered.connect(self._onRemoveMember)
        menu.addAction(removeAction)

        linkedType = self._getLinkedType()
        events = self._getLinkedEvents()
        if linkedType and events:
            menu.addSeparator()
            bpAction = QtWidgets.QAction(ELOC("EDIT_BLUEPRINT"), self)
            bpAction.triggered.connect(self._onEditBlueprint)
            menu.addAction(bpAction)

        from Utils import PluginSystem

        PluginSystem.AddRightClickActions(menu, self, "generalDataMember", "hit", item.text())
        menu.exec_(self.memberList.mapToGlobal(position))

    def _changeMemberID(self):
        current = self.memberList.currentItem()
        if not current:
            return

        oldID = current.text()
        OpenSingleRowDialog(
            self,
            ELOC("CHANGE_ID"),
            ELOC("ENTER_ID"),
            oldID,
            onAccepted=lambda newID: self._changeMemberIDNamed(oldID, newID),
        )

    def _changeMemberIDNamed(self, oldID: str, newID: str) -> None:
        if not newID or newID == oldID:
            return
        fileData = GameData.generalData.get(self.fileKey, {})
        members = fileData.get("members", {})
        if newID in members:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("ID_ALREADY_EXISTS"))
            return

        if oldID in members:
            members[newID] = members.pop(oldID)

            items = self.memberList.findItems(oldID, QtCore.Qt.MatchExactly)
            if items:
                items[0].setText(newID)
            self._currentMemberKey = newID

            self._buildPropertyForm()

            self.MODIFIED.emit()

    def _getDuplicateMemberID(self, sourceID: str, members: dict) -> str:
        baseID = f"{sourceID}_copy"
        if baseID not in members:
            return baseID

        index = 2
        while f"{baseID}{index}" in members:
            index += 1
        return f"{baseID}{index}"

    def _onDuplicateMember(self) -> None:
        current = self.memberList.currentItem()
        if not current or not self.fileKey:
            return

        sourceID = current.text()
        fileData = GameData.generalData.get(self.fileKey, {})
        members = fileData.get("members", {})
        if sourceID not in members:
            return

        defaultID = self._getDuplicateMemberID(sourceID, members)
        OpenSingleRowDialog(
            self,
            ELOC("DUPLICATE_MEMBER"),
            ELOC("ENTER_ID"),
            defaultID,
            onAccepted=lambda newID: self._duplicateMember(sourceID, newID),
        )

    def _duplicateMember(self, sourceID: str, newID: str) -> None:
        newID = newID.strip()
        if not newID or not self.fileKey:
            return
        fileData = GameData.generalData.get(self.fileKey, {})
        members = fileData.get("members", {})
        if sourceID not in members:
            return
        if newID in members:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("ID_ALREADY_EXISTS"))
            return
        newMember = copy.deepcopy(members[sourceID])
        orderedMembers = {}
        for memberID, memberData in members.items():
            orderedMembers[memberID] = memberData
            if memberID == sourceID:
                orderedMembers[newID] = newMember
        members.clear()
        members.update(orderedMembers)

        row = self.memberList.currentRow()
        self.memberList.insertItem(row + 1, newID)
        self.memberList.setCurrentRow(row + 1)
        self.MODIFIED.emit()

    def _onAddMember(self):
        if not self.fileKey:
            return

        OpenSingleRowDialog(
            self,
            ELOC("NEW_MEMBER"),
            ELOC("ENTER_ID"),
            onAccepted=self._addMember,
        )

    def _addMember(self, text: str) -> None:
        if not text or not self.fileKey:
            return
        fileData = GameData.generalData.get(self.fileKey, {})
        members = fileData.get("members", {})
        if text in members:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("ID_ALREADY_EXISTS"))
            return
        params = fileData.get("params", {})
        newMember = {}
        for pk, pv in params.items():
            newMember[pk] = self._getDefaultMemberValue(pv)
        members[text] = newMember
        self.memberList.addItem(text)
        self.memberList.setCurrentRow(self.memberList.count() - 1)
        self.MODIFIED.emit()

    def _onRemoveMember(self):
        row = self.memberList.currentRow()
        if row < 0:
            return

        item = self.memberList.takeItem(row)
        if item is None:
            return
        key = item.text()

        if self.fileKey:
            fileData = GameData.generalData.get(self.fileKey, {})
            members = fileData.get("members", {})
            if key in members:
                del members[key]
                self.MODIFIED.emit()

        if self.memberList.count() == 0:
            self._clearPropertyForm()


class GeneralDataEditor(QtWidgets.QMainWindow):
    MODIFIED = QtCore.pyqtSignal()

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.setWindowTitle(ELOC("GENERAL_DATA_EDITOR"))
        self.resize(1000, 600)

        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        mainLayout = QtWidgets.QVBoxLayout(central)

        self.tabWidget = QtWidgets.QTabWidget()
        self.tabWidget.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.tabWidget.customContextMenuRequested.connect(self._onTabContextMenu)
        self.tabWidget.tabBar().setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.tabWidget.tabBar().customContextMenuRequested.connect(self._onTabBarContextMenu)
        mainLayout.addWidget(self.tabWidget)

        self._populateFiles()

    def _scanInfoTypes(self) -> list:
        infoTypes = []
        infosPath = os.path.join(EditorStatus.PROJ_PATH, "Source", "Infos")

        if not os.path.isdir(infosPath):
            return infoTypes

        for fileName in os.listdir(infosPath):
            if not fileName.endswith(".py") or fileName.startswith("_"):
                continue
            filePath = os.path.join(infosPath, fileName)
            try:
                with open(filePath, "r", encoding="utf-8") as f:
                    content = f.read()
                if "InfoBase" in content or "_infoType" in content:
                    className = fileName[:-3]
                    infoTypes.append(className)
            except Exception:
                continue

        if not infoTypes:
            infoTypes = ["ItemInfo", "EnemyInfo"]

        return infoTypes

    def _onTabContextMenu(self, position: QtCore.QPoint):
        tabBar = self.tabWidget.tabBar()
        tabIndex = tabBar.tabAt(tabBar.mapFrom(self.tabWidget, position)) if tabBar else -1
        self._showTabContextMenu(self.tabWidget.mapToGlobal(position), tabIndex)

    def _onTabBarContextMenu(self, position: QtCore.QPoint):
        tabBar = self.tabWidget.tabBar()
        tabIndex = tabBar.tabAt(position) if tabBar else -1
        self._showTabContextMenu(tabBar.mapToGlobal(position), tabIndex)

    def _showTabContextMenu(self, globalPos: QtCore.QPoint, tabIndex: int):
        menu = QtWidgets.QMenu(self)

        addAction = QtWidgets.QAction(ELOC("NEW_DATA_TYPE"), self)
        addAction.triggered.connect(self._onAddDataType)
        menu.addAction(addAction)

        if tabIndex >= 0:
            menu.addSeparator()

            renameAction = QtWidgets.QAction(ELOC("RENAME_DATA_TYPE"), self)
            renameAction.triggered.connect(lambda checked=False, idx=tabIndex: self._onRenameDataType(idx))
            menu.addAction(renameAction)

            linkAction = QtWidgets.QAction(ELOC("SET_LINKED_TYPE"), self)
            linkAction.triggered.connect(lambda checked=False, idx=tabIndex: self._onSetLinkedType(idx))
            menu.addAction(linkAction)

            menu.addSeparator()

            deleteAction = QtWidgets.QAction(ELOC("DELETE_DATA_TYPE"), self)
            deleteAction.triggered.connect(lambda checked=False, idx=tabIndex: self._onRemoveDataType(idx))
            menu.addAction(deleteAction)

        from Utils import PluginSystem

        PluginSystem.AddRightClickActions(
            menu,
            self,
            "generalDataTab",
            "hit" if tabIndex >= 0 else "empty",
            self.tabWidget.tabText(tabIndex) if tabIndex >= 0 else None,
        )
        menu.exec_(globalPos)

    def _onAddDataType(self):
        infoTypes = self._scanInfoTypes()
        options = [ELOC("NO_LINKED_TYPE")] + infoTypes

        OpenItemSelectorDialog(
            self,
            ELOC("SELECT_LINKED_TYPE"),
            ELOC("SELECT_LINKED_TYPE_DESC"),
            options,
            onAccepted=lambda linkedType: self._requestDataTypeName(linkedType, options[0]),
        )

    def _requestDataTypeName(self, linkedType: str, noLinkedType: str) -> None:
        actualLinkedType = linkedType if linkedType != noLinkedType else None
        OpenSingleRowDialog(
            self,
            ELOC("NEW_DATA_TYPE"),
            ELOC("ENTER_DATA_TYPE_NAME"),
            onAccepted=lambda text: self._addDataType(text, actualLinkedType),
        )

    def _addDataType(self, text: str, linkedType: Optional[str]) -> None:
        if not text:
            return
        data = getattr(GameData, "generalData", {})
        if text in data:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("DATA_TYPE_EXISTS"))
            return
        newEntry = {"params": {}, "members": {}}
        if linkedType:
            newEntry["linkedType"] = linkedType
        data[text] = newEntry
        self._populateFiles()
        for i in range(self.tabWidget.count()):
            if self.tabWidget.tabText(i) == text:
                self.tabWidget.setCurrentIndex(i)
                break

        self.MODIFIED.emit()

    def _onSetLinkedType(self, index: int):
        key = self.tabWidget.tabText(index)
        data = getattr(GameData, "generalData", {})
        fileData = data.get(key, {})

        infoTypes = self._scanInfoTypes()
        currentLinked = fileData.get("linkedType", "")
        options = [ELOC("NO_LINKED_TYPE")] + infoTypes

        currentIndex = 0
        if currentLinked in infoTypes:
            currentIndex = infoTypes.index(currentLinked) + 1

        OpenItemSelectorDialog(
            self,
            ELOC("SET_LINKED_TYPE"),
            ELOC("SELECT_LINKED_TYPE_DESC"),
            options,
            currentIndex,
            onAccepted=lambda linkedType: self._setLinkedType(index, linkedType, options[0]),
        )

    def _setLinkedType(self, index: int, linkedType: str, noLinkedType: str) -> None:
        key = self.tabWidget.tabText(index)
        data = getattr(GameData, "generalData", {})
        fileData = data.get(key, {})
        isLinked = linkedType != noLinkedType
        if isLinked:
            fileData["linkedType"] = linkedType
            if "events" in fileData:
                del fileData["events"]
        else:
            if "linkedType" in fileData:
                del fileData["linkedType"]
            if "events" in fileData:
                del fileData["events"]

        self._populateFiles()
        self.tabWidget.setCurrentIndex(index)
        self.MODIFIED.emit()

    def _onRenameDataType(self, index: int):
        oldKey = self.tabWidget.tabText(index)

        OpenSingleRowDialog(
            self,
            ELOC("RENAME_DATA_TYPE"),
            ELOC("ENTER_DATA_TYPE_NAME"),
            oldKey,
            onAccepted=lambda text: self._renameDataType(oldKey, text),
        )

    def _renameDataType(self, oldKey: str, text: str) -> None:
        if not text or text == oldKey:
            return
        data = getattr(GameData, "generalData", {})
        if text in data:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("DATA_TYPE_EXISTS"))
            return
        if oldKey in data:
            data[text] = data.pop(oldKey)
        self._populateFiles()
        for i in range(self.tabWidget.count()):
            if self.tabWidget.tabText(i) == text:
                self.tabWidget.setCurrentIndex(i)
                break
        self.MODIFIED.emit()

    def _onRemoveDataType(self, index: int):
        key = self.tabWidget.tabText(index)

        reply = QtWidgets.QMessageBox.question(
            self,
            ELOC("CONFIRM_DELETE"),
            ELOC("CONFIRM_DELETE_DATA_TYPE").format(key),
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
        )

        if reply == QtWidgets.QMessageBox.Yes:
            data = getattr(GameData, "generalData", {})
            if key in data:
                del data[key]

            self.tabWidget.removeTab(index)
            self.MODIFIED.emit()

    def _onBlueprintEditRequest(self, fileKey: str, memberKey: str, events: list):
        import copy

        fileData = GameData.generalData.get(fileKey, {})
        members = fileData.get("members", {})
        memberData = members.get(memberKey, {})
        linkedType = fileData.get("linkedType", "")

        GeneralDataPage._normalizeMemberGraph(memberData, events)
        members[memberKey] = memberData

        bpData = {
            "parent": f"Source.{linkedType}" if linkedType else "",
            "attrs": {"ID": memberKey},
            "graph": copy.deepcopy(memberData["_graph"]),
        }

        from Widgets.BlueprintEditor import BluePrintEditor

        bpTitle = f"__info__/{fileKey}/{memberKey}"
        editor = BluePrintEditor(bpTitle, bpData, self)

        def _onInfoBlueprintModified():
            memberData["_graph"] = copy.deepcopy(editor.data.get("graph", {}))
            self.MODIFIED.emit()

        editor.MODIFIED.connect(_onInfoBlueprintModified)
        editor.show()
        editor.raise_()

    def _populateFiles(self):
        self.tabWidget.clear()
        data = getattr(GameData, "generalData", {})

        for key in sorted(data.keys()):
            page = GeneralDataPage(key, self)
            page.MODIFIED.connect(self.MODIFIED.emit)
            page.REQUEST_BLUEPRINT_EDIT.connect(self._onBlueprintEditRequest)
            self.tabWidget.addTab(page, key)

    def selectDataType(self, key: str) -> bool:
        if key not in getattr(GameData, "generalData", {}):
            return False
        for i in range(self.tabWidget.count()):
            if self.tabWidget.tabText(i) == key:
                self.tabWidget.setCurrentIndex(i)
                self.activateWindow()
                self.raise_()
                return True
        return False
