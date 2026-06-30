# -*- encoding: utf-8 -*-

from __future__ import annotations

import copy
import os
from typing import TYPE_CHECKING, Optional, Dict, Any, Set

from PyQt5 import QtWidgets, QtCore

from EditorGlobal import EditorStatus, GameData
from Widgets.Utils import DataclassWidget, FileSelectorDialog, RectViewer
from Widgets.Utils.ClassDetailMixin import ClassDetailMixin
from Widgets.Utils.MetaVarTypes import _GENERALDATA_VAR_TYPE
from Widgets.Utils.StructuredFields import IsStructuredType

if TYPE_CHECKING:
    from Widgets.EditorPanel import EditorPanel


class ActorInfoPanel(ClassDetailMixin, QtWidgets.QWidget):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._layerName: Optional[str] = None
        self._index: Optional[int] = None
        self._editorPanel: Optional[EditorPanel] = None
        self._blockSignals = False
        self.invalidVars: Set[str] = set()
        self.pathVarMap: Dict[str, str] = {}
        self.pathVars: Set[str] = set()
        self.rectRangeVars: Set[str] = set()
        self.rectRangeVarMap: Dict[str, str] = {}
        self.attrVarTypes: Dict[str, str] = {}
        self.attrGDVars: Dict[str, str] = {}
        self.attrRely: Dict[str, Any] = {}
        self.attrRelySources: Set[str] = set()
        self._classDefaultValues: Dict[str, Any] = {}
        self._classDisplayValues: Dict[str, Any] = {}

        self.setStyleSheet("")

        self.mainLayout = QtWidgets.QVBoxLayout(self)
        self.mainLayout.setContentsMargins(4, 4, 4, 4)
        self.mainLayout.setSpacing(4)

        self.titleLabel = QtWidgets.QLabel(ELOC("ACTOR_INFO"))
        self.titleLabel.setAlignment(QtCore.Qt.AlignCenter)
        self.titleLabel.setStyleSheet("font-weight: bold; background-color: #444; padding: 4px; border-radius: 4px;")
        self.mainLayout.addWidget(self.titleLabel)

        self.scrollArea = QtWidgets.QScrollArea(self)
        self.scrollArea.setWidgetResizable(True)
        self.scrollArea.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.scrollArea.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.scrollArea.setFrameShape(QtWidgets.QFrame.NoFrame)
        self.mainLayout.addWidget(self.scrollArea, 1)

        self.contentWidget = QtWidgets.QWidget(self.scrollArea)
        self.contentLayout = QtWidgets.QVBoxLayout(self.contentWidget)
        self.contentLayout.setContentsMargins(0, 0, 0, 0)
        self.contentLayout.setSpacing(4)

        self.formLayout = QtWidgets.QFormLayout()
        self.formLayout.setContentsMargins(0, 0, 0, 0)
        self.formLayout.setSpacing(4)
        self.contentLayout.addLayout(self.formLayout)

        self.tagEdit = QtWidgets.QLineEdit()
        self.tagEdit.textChanged.connect(self._onTagChanged)
        self.formLayout.addRow(ELOC("TAG"), self.tagEdit)

        self.classSeparator = QtWidgets.QFrame()
        self.classSeparator.setFrameShape(QtWidgets.QFrame.HLine)
        self.classSeparator.setFrameShadow(QtWidgets.QFrame.Sunken)
        self.contentLayout.addWidget(self.classSeparator)

        self.classTitleLabel = QtWidgets.QLabel(ELOC("CLASS_DETAIL"))
        self.classTitleLabel.setStyleSheet("font-weight: bold; padding: 4px 0 2px 0;")
        self.contentLayout.addWidget(self.classTitleLabel)

        self.classFormContainer = QtWidgets.QWidget(self.contentWidget)
        self.classFormLayout = QtWidgets.QFormLayout(self.classFormContainer)
        self.classFormLayout.setContentsMargins(0, 0, 0, 0)
        self.classFormLayout.setSpacing(4)
        self.contentLayout.addWidget(self.classFormContainer)
        self.contentLayout.addStretch()
        self.scrollArea.setWidget(self.contentWidget)

        self.noSelectionLabel = QtWidgets.QLabel(ELOC("NO_SELECTION"))
        self.noSelectionLabel.setAlignment(QtCore.Qt.AlignCenter)
        self.noSelectionLabel.setStyleSheet("color: #888; font-style: italic; margin-top: 20px;")
        self.mainLayout.addWidget(self.noSelectionLabel)

        self.setEnabled(False)
        self.noSelectionLabel.setVisible(True)
        self.titleLabel.setVisible(False)
        self.scrollArea.setVisible(False)
        self._setClassDetailVisible(False)

    def setActor(
        self,
        layerName: Optional[str],
        index: Optional[int],
        data: Optional[Dict[str, Any]],
        editorPanel: Optional[EditorPanel],
    ) -> None:
        self._editorPanel = editorPanel
        self._layerName = layerName
        self._index = index

        if layerName is None or index is None or data is None:
            self.setEnabled(False)
            self.noSelectionLabel.setVisible(True)
            self.titleLabel.setVisible(False)
            self.scrollArea.setVisible(False)
            self._clearClassDetail()
            return

        self.setEnabled(True)
        self.noSelectionLabel.setVisible(False)
        self.titleLabel.setVisible(True)
        self.scrollArea.setVisible(True)

        self._blockSignals = True
        try:
            self.tagEdit.setText(str(data.get("tag", "")))

            self._refreshClassDetail(data)
        except Exception as e:
            print(f"Error setting actor info: {e}")
            self._clearClassDetail()
        finally:
            self._blockSignals = False

    def _getActorData(self) -> Optional[Dict[str, Any]]:
        editorPanel = self._getEditorPanel()
        if editorPanel is None or not self._layerName or self._index is None:
            return None
        m = GameData.mapData.get(editorPanel.mapKey)
        if not isinstance(m, dict):
            return None
        actors = m.get("actors", {}).get(self._layerName)
        if not isinstance(actors, list) or not (0 <= self._index < len(actors)):
            return None
        return actors[self._index]

    def _getMapData(self) -> Optional[Dict[str, Any]]:
        editorPanel = self._getEditorPanel()
        if editorPanel is None:
            return None
        data = GameData.mapData.get(editorPanel.mapKey)
        return data if isinstance(data, dict) else None

    def _onTagChanged(self, text: str) -> None:
        if self._blockSignals:
            return
        data = self._getActorData()
        if data is None:
            return
        oldTag = str(data.get("tag", ""))
        tag = text
        editorPanel = self._getEditorPanel()
        if editorPanel is not None:
            tag = editorPanel.makeUniqueActorTag(text, self._layerName, self._index)
            if tag != text:
                self._blockSignals = True
                try:
                    self.tagEdit.setText(tag)
                finally:
                    self._blockSignals = False
        GameData.RecordSnapshot()
        data["tag"] = tag
        self._moveClassVarChanges(oldTag, tag)
        self._notifyActorDataChanged(render=False)

    def _notifyActorDataChanged(self, render: bool) -> None:
        editorPanel = self._getEditorPanel()
        if editorPanel is None:
            return
        editorPanel._refreshTitle()
        editorPanel.DATA_CHANGED.emit()
        if render:
            editorPanel._renderFromMapData()
            editorPanel.update()

    def _getEditorPanel(self) -> Optional[EditorPanel]:
        from Widgets.EditorPanel import EditorPanel

        editorPanel = self._editorPanel
        if isinstance(editorPanel, EditorPanel):
            return editorPanel
        return None

    def _clearClassDetail(self) -> None:
        while self.classFormLayout.rowCount() > 0:
            self.classFormLayout.removeRow(0)
        self._classDefaultValues = {}
        self._classDisplayValues = {}
        self._setClassDetailVisible(False)

    def _setClassDetailVisible(self, visible: bool) -> None:
        self.classSeparator.setVisible(visible)
        self.classTitleLabel.setVisible(visible)
        self.classFormContainer.setVisible(visible)

    def _refreshClassDetail(self, actorData: Dict[str, Any]) -> None:
        while self.classFormLayout.rowCount() > 0:
            self.classFormLayout.removeRow(0)

        cls = self._resolveActorClass(actorData.get("bp"))
        if not isinstance(cls, type):
            self._clearClassDetail()
            return

        self._reloadClassMetadata(cls)
        bpData = self._getBlueprintData(actorData.get("bp"))
        parentCls = self._getBaseClass(cls)
        attrs = copy.deepcopy(bpData.get("attrs", {})) if isinstance(bpData, dict) else {}
        if not isinstance(attrs, dict):
            attrs = {}

        if isinstance(bpData, dict):
            displayAttrs = self._getParentDisplayAttrs(parentCls, attrs)
            displayAttrs.update(attrs)
            targetCls = cls if cls is not None else parentCls
        else:
            displayAttrs = self._getClassDisplayAttrs(cls)
            targetCls = cls

        componentTypes = self._getComponentTypes(targetCls)
        componentFieldMap = self._getComponentFieldMap(componentTypes)
        componentSkipKeys = set(componentTypes.keys()) | set(componentFieldMap.keys())

        overrides = self._getClassVarChangesForActor(actorData, create=False)
        if isinstance(overrides, dict):
            for key, value in overrides.items():
                if key in displayAttrs or key in componentTypes:
                    defaultValue = displayAttrs.get(key)
                    displayAttrs[key] = self._coerceOverrideForDisplay(defaultValue, value)

        self._classDefaultValues = {}
        if isinstance(bpData, dict):
            self._classDefaultValues = self._getParentDisplayAttrs(parentCls, attrs)
            self._classDefaultValues.update(attrs)
        else:
            self._classDefaultValues = self._getClassDisplayAttrs(cls)

        self._classDisplayValues = copy.deepcopy(displayAttrs)

        self._addComponentRows(componentTypes)

        normalAttrs = {k: v for k, v in displayAttrs.items() if k not in componentSkipKeys}
        displayKeys = self._getDisplayOrder(normalAttrs, targetCls)

        typeHints = self._getTypeHints(cls)
        parentHints = self._getTypeHints(parentCls)

        for key in displayKeys:
            if key in self.invalidVars:
                continue
            value = normalAttrs[key]
            label = self._createClassAttrLabel(key)
            container = QtWidgets.QWidget()
            hbox = QtWidgets.QHBoxLayout(container)
            hbox.setContentsMargins(0, 0, 0, 0)
            hbox.setSpacing(4)

            typeHint = typeHints.get(key)
            if typeHint is None and key in parentHints:
                typeHint = parentHints[key]

            defaultValue = self._classDefaultValues.get(key)
            if typeHint and IsStructuredType(typeHint) and self.attrVarTypes.get(key) != _GENERALDATA_VAR_TYPE:
                widget = DataclassWidget(typeHint, value)
                widget.VALUE_CHANGED.connect(lambda val, k=key: self._onClassVarChanged(k, val))
            else:
                widget = self._createClassInputWidget(
                    key,
                    value,
                    type_hint=typeHint,
                    parent_val=defaultValue,
                    var_type=self.attrVarTypes.get(key, ""),
                )

            isInvalid = key in self.invalidVars
            isRectRange = key in self.rectRangeVars and not isInvalid
            isPath = key in self.pathVars and not isInvalid and not isRectRange
            relyEditable = self._isRelyEditable(key, self._classDisplayValues)

            if isinstance(widget, QtWidgets.QLineEdit):
                if isInvalid or isPath or isRectRange:
                    widget.setReadOnly(True)
                    widget.setStyleSheet("background-color: #303030; color: #aaaaaa;")
                    widget.setCursor(QtCore.Qt.ArrowCursor)
            elif isinstance(widget, QtWidgets.QCheckBox):
                if isInvalid:
                    widget.setEnabled(False)
                    widget.setStyleSheet("color: #aaaaaa;")
            else:
                elems = getattr(widget, "_elementWidgets", None)
                if elems is not None and (isInvalid or isRectRange):
                    for e in elems:
                        if isinstance(e, QtWidgets.QLineEdit):
                            e.setReadOnly(True)
                            e.setStyleSheet("background-color: #303030; color: #aaaaaa;")
                            e.setCursor(QtCore.Qt.ArrowCursor)

            pathBtn = None
            rectBtn = None
            if not relyEditable:
                self._setWidgetEditable(widget, False)

            hbox.addWidget(widget, 1)

            if isPath and isinstance(widget, QtWidgets.QLineEdit):
                pathBtn = QtWidgets.QPushButton("...")
                pathBtn.setObjectName("PathBtn")
                pathBtn.setFixedWidth(24)
                pathBtn.clicked.connect(lambda _, k=key, w=widget: self._onSelectPath(k, w))
                pathBtn.setEnabled(relyEditable)
                hbox.addWidget(pathBtn, 0)

            if isRectRange:
                rectBtn = QtWidgets.QPushButton("...")
                rectBtn.setObjectName("RectBtn")
                rectBtn.setFixedWidth(24)
                rectBtn.clicked.connect(lambda _, k=key: self._onEditRectRange(k))
                rectBtn.setEnabled(relyEditable)
                hbox.addWidget(rectBtn, 0)

            self._applyRelyTooltip(key, relyEditable, label, container, widget, pathBtn, rectBtn)
            self.classFormLayout.addRow(label, container)

        self._setClassDetailVisible(True)

    def _coerceOverrideForDisplay(self, defaultValue: Any, value: Any) -> Any:
        if isinstance(defaultValue, tuple) and isinstance(value, list):
            return tuple(value)
        return copy.deepcopy(value)

    def _resolveActorClass(self, bpRel: Any) -> Optional[type]:
        if not isinstance(bpRel, str) or not bpRel.strip():
            return None
        try:
            cls = GameData.classDict.get(bpRel, EditorStatus.PROJ_PATH)
        except Exception:
            return None
        return cls if isinstance(cls, type) else None

    def _getBlueprintData(self, bpRel: Any) -> Optional[Dict[str, Any]]:
        if not isinstance(bpRel, str):
            return None
        prefix = "Data.Blueprints."
        if not bpRel.startswith(prefix):
            return None
        key = bpRel[len(prefix) :].replace(".", "/")
        data = GameData.blueprintsData.get(key)
        return data if isinstance(data, dict) else None

    def _addComponentRows(self, componentTypes: Dict[str, type]) -> None:
        items = []
        for componentName, componentType in componentTypes.items():
            if componentName not in self._classDisplayValues:
                continue
            value = self._classDisplayValues.get(componentName)
            if value is None:
                continue
            items.append((componentName, componentType))

        if not items:
            return

        container = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout(container)
        hbox.setContentsMargins(0, 0, 0, 0)
        hbox.setSpacing(4)

        listWidget = QtWidgets.QListWidget()
        listWidget.setFixedHeight(max(72, min(160, len(items) * 28 + 12)))
        listWidget.itemDoubleClicked.connect(self._onEditComponentItem)
        for componentName, componentType in items:
            item = QtWidgets.QListWidgetItem(self._getClassAttrDisplayName(componentName))
            item.setData(QtCore.Qt.UserRole, componentName)
            item.setData(QtCore.Qt.UserRole + 1, componentType)
            desc = self._getClassAttrDisplayDesc(componentName)
            if desc:
                item.setToolTip(desc)
            listWidget.addItem(item)
        hbox.addWidget(listWidget, 1)
        self.classFormLayout.addRow(QtWidgets.QLabel(ELOC("COMPONENTS")), container)

        if listWidget.count() > 0:
            listWidget.setCurrentRow(0)

    def _onEditComponentItem(self, item: QtWidgets.QListWidgetItem) -> None:
        key = item.data(QtCore.Qt.UserRole)
        componentType = item.data(QtCore.Qt.UserRole + 1)
        if isinstance(key, str) and isinstance(componentType, type):
            QtCore.QTimer.singleShot(0, lambda k=key, t=componentType: self._onEditComponent(k, t))

    def _onEditComponent(self, key: str, componentType: type) -> None:
        currentValue = self._classDisplayValues.get(key)
        displayValue = self._normaliseComponentData(componentType, currentValue)

        dlg = QtWidgets.QDialog(self)
        dlg.setWindowTitle(self._getClassAttrDisplayName(key))
        layout = QtWidgets.QVBoxLayout(dlg)
        widget = DataclassWidget(componentType, copy.deepcopy(displayValue), dlg)
        layout.addWidget(widget)
        buttons = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        okBtn = buttons.button(QtWidgets.QDialogButtonBox.Ok)
        cancelBtn = buttons.button(QtWidgets.QDialogButtonBox.Cancel)
        if okBtn:
            okBtn.setText(ELOC("CONFIRM"))
        if cancelBtn:
            cancelBtn.setText(ELOC("CANCEL"))
        buttons.accepted.connect(dlg.accept)
        buttons.rejected.connect(dlg.reject)
        layout.addWidget(buttons)
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return

        self._onClassVarChanged(key, copy.deepcopy(widget.data), refresh=True)

    def _createClassInputWidget(
        self,
        key: str,
        value: Any,
        type_hint: Optional[type] = None,
        parent_val: Any = None,
        var_type: str = "",
    ) -> QtWidgets.QWidget:
        return self._createClassDetailInputWidget(
            key,
            value,
            self._onClassVarChanged,
            type_hint=type_hint,
            parent_val=parent_val,
            var_type=var_type,
        )

    def _onClassVarChanged(self, key: str, value: Any, refresh: bool = False) -> None:
        if self._blockSignals:
            return
        actorData = self._getActorData()
        if actorData is None:
            return

        value = self._evalTextValue(value) if isinstance(value, str) else value
        value = GameData._NormaliseBlueprintValue(copy.deepcopy(value))
        defaultValue = self._classDefaultValues.get(key)
        hasDefault = key in self._classDefaultValues
        isDefault = hasDefault and GameData._IsBlueprintValueEqual(value, defaultValue)
        changes = self._getClassVarChangesForActor(actorData, create=not isDefault)

        if changes is None:
            return

        currentExists = key in changes
        currentValue = changes.get(key)
        if isDefault:
            if not currentExists:
                return
        elif currentExists and GameData._IsBlueprintValueEqual(currentValue, value):
            return

        GameData.RecordSnapshot()
        if isDefault:
            changes.pop(key, None)
            self._cleanupClassVarChanges(actorData)
            self._classDisplayValues[key] = copy.deepcopy(defaultValue)
        else:
            changes[key] = value
            self._classDisplayValues[key] = copy.deepcopy(value)
        self._notifyActorDataChanged(render=True)

        if refresh or key in self.attrRelySources:
            QtCore.QTimer.singleShot(0, lambda: self._refreshClassDetail(actorData))

    def _getClassVarChangesForActor(
        self, actorData: Dict[str, Any], create: bool = False
    ) -> Optional[Dict[str, Any]]:
        mapData = self._getMapData()
        if mapData is None:
            return None
        tag = str(actorData.get("tag", ""))
        root = mapData.get("BPClassVarChanged")
        if not isinstance(root, dict):
            if not create:
                return None
            root = {}
            mapData["BPClassVarChanged"] = root
        changes = root.get(tag)
        if not isinstance(changes, dict):
            if not create:
                return None
            changes = {}
            root[tag] = changes
        return changes

    def _cleanupClassVarChanges(self, actorData: Dict[str, Any]) -> None:
        mapData = self._getMapData()
        if mapData is None:
            return
        root = mapData.get("BPClassVarChanged")
        if not isinstance(root, dict):
            return
        tag = str(actorData.get("tag", ""))
        changes = root.get(tag)
        if isinstance(changes, dict) and not changes:
            root.pop(tag, None)
        if not root:
            mapData.pop("BPClassVarChanged", None)

    def _moveClassVarChanges(self, oldTag: str, newTag: str) -> None:
        if oldTag == newTag:
            return
        mapData = self._getMapData()
        if mapData is None:
            return
        root = mapData.get("BPClassVarChanged")
        if not isinstance(root, dict):
            return
        oldChanges = root.pop(oldTag, None)
        if not isinstance(oldChanges, dict):
            if not root:
                mapData.pop("BPClassVarChanged", None)
            return
        newChanges = root.get(newTag)
        if isinstance(newChanges, dict):
            newChanges.update(oldChanges)
        else:
            root[newTag] = oldChanges
        if not root:
            mapData.pop("BPClassVarChanged", None)

    def _onSelectPath(self, key: str, widget: QtWidgets.QLineEdit) -> None:
        baseDir = self._getPathVarBaseDir(key)
        if not os.path.isdir(baseDir):
            assetsDir = os.path.join(EditorStatus.PROJ_PATH, "Assets")
            baseDir = assetsDir if os.path.isdir(assetsDir) else EditorStatus.PROJ_PATH
        dlg = FileSelectorDialog(self, baseDir, FileSelectorDialog.allFilesFilter(star=True))
        filePath = dlg.execSelect()
        if not filePath:
            return
        try:
            relPath = os.path.relpath(filePath, baseDir)
        except ValueError:
            relPath = filePath
        widget.setText(relPath.replace("\\", "/"))

    def _onEditRectRange(self, key: str) -> None:
        pathKey = self.rectRangeVarMap.get(key)
        if not pathKey:
            return
        pathValue = self._classDisplayValues.get(pathKey)
        if not isinstance(pathValue, str) or not pathValue:
            return
        imagePath = os.path.join(self._getPathVarBaseDir(pathKey), pathValue)
        rectValue = self._classDisplayValues.get(key)
        rectTuple = self._rectValueToTuple(rectValue)
        if rectTuple is None:
            cell = getattr(EditorStatus, "CELLSIZE", 0)
            if not isinstance(cell, int) or cell <= 0:
                cell = 32
            rectTuple = (0, 0, cell, cell)
        dlg = RectViewer(self, imagePath, rectTuple)
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return
        nx, ny, nw, nh = dlg.getRectTuple()
        self._onClassVarChanged(key, ((nx, ny), (nw, nh)), refresh=True)
