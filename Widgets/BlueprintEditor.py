# -*- encoding: utf-8 -*-

from __future__ import annotations

import math
import os
import copy
import dataclasses
from typing import Any, Dict, Optional, Set, get_type_hints
from PyQt5 import QtWidgets, QtCore, QtGui
from EditorGlobal import EditorStatus, GameData
from Utils import System, File
from Widgets.Utils import SingleRowDialog, NodePanel, Toast, RectViewer, DataclassWidget, FileSelectorDialog
from Widgets.Utils.MetaRely import getRelyConditionDisplay, getRelySourceSet, isRelyEditable, normaliseRelyMap


class RevertButton(QtWidgets.QPushButton):
    """A small button with a UE-style curved revert arrow icon."""

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setObjectName("RevertBtn")
        self.setFixedSize(20, 20)
        self.setCursor(QtCore.Qt.PointingHandCursor)

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:
        super().paintEvent(event)
        p = QtGui.QPainter(self)
        p.setRenderHint(QtGui.QPainter.Antialiasing)

        enabled = self.isEnabled()
        color = QtGui.QColor("#c0c0c0") if enabled else QtGui.QColor("#585858")
        pen = QtGui.QPen(color, 1.5, QtCore.Qt.SolidLine, QtCore.Qt.RoundCap, QtCore.Qt.RoundJoin)
        p.setPen(pen)
        p.setBrush(QtCore.Qt.NoBrush)

        # Curved arrow body from (14,12) to (7,7), bowing left-bottom
        path = QtGui.QPainterPath()
        path.moveTo(14, 12)
        path.cubicTo(14, 16.5, 3.5, 16, 3.5, 10.5)
        path.cubicTo(3.5, 7.5, 6, 7.5, 7, 7)
        p.drawPath(path)

        # Arrowhead at (7,7) pointing up-left
        p.setBrush(color)
        p.setPen(QtCore.Qt.NoPen)
        arrow = QtGui.QPainterPath()
        arrow.moveTo(7, 7)
        arrow.lineTo(3.5, 4.5)
        arrow.lineTo(10, 4.5)
        arrow.closeSubpath()
        p.drawPath(arrow)


class BluePrintEditor(QtWidgets.QWidget):
    MODIFIED = QtCore.pyqtSignal()

    def __init__(self, title: str, data: Dict[str, Any], parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setWindowFlags(QtCore.Qt.Window)
        self.setWindowTitle(title)
        self.setMaximumHeight(600)
        self.resize(1080, 600)
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        System.SetStyle(self, "blueprintEditor.qss")
        self.title = title
        self.data = copy.deepcopy(data)
        self.graphs: Dict[str, Any] = {}
        self.invalidVars: Set[str] = self._getInvalidVars()
        self.pathVars: Set[str] = self._getPathVars()
        rectMap = self._getRectRangeVars()
        self.rectRangeVars: Set[str] = set(rectMap.keys())
        self.rectRangeVarMap: Dict[str, str] = rectMap
        self.attrRely: Dict[str, Any] = self._getMetaRely()
        self.attrRelySources: Set[str] = getRelySourceSet(self.attrRely)
        self.setupUI()
        self.toast = Toast(self)

    def _resolveClass(self) -> Optional[type]:
        if self.title.startswith("__info__/"):
            return None
        key = os.path.join("Data", "Blueprints", self.title).replace("/", ".").replace("\\", ".")
        try:
            cls = GameData.classDict.get(key, EditorStatus.PROJ_PATH)
        except (ImportError, Exception):
            return None
        return cls if isinstance(cls, type) else None

    def _getBaseClass(self, cls: Optional[type]) -> Optional[type]:
        if isinstance(cls, type) and cls.__bases__:
            return cls.__bases__[0]
        return None

    def _getClassAttrValue(self, cls: Optional[type], name: str) -> tuple[bool, Any]:
        if not isinstance(cls, type):
            return False, None
        try:
            return True, getattr(cls, name)
        except AttributeError:
            return False, None
        except Exception:
            return False, None

    def _hasClassAttr(self, cls: Optional[type], name: str) -> bool:
        found, _value = self._getClassAttrValue(cls, name)
        return found

    def _getWidgetCurrentValue(self, widget: QtWidgets.QWidget) -> Any:
        """Extract the current value from any widget type created by createInputWidget."""
        if isinstance(widget, QtWidgets.QCheckBox):
            return widget.isChecked()
        if isinstance(widget, QtWidgets.QSpinBox):
            return widget.value()
        if isinstance(widget, QtWidgets.QDoubleSpinBox):
            return widget.value()
        if isinstance(widget, QtWidgets.QLineEdit):
            text = widget.text()
            try:
                return eval(text)
            except Exception:
                return text
        if isinstance(widget, DataclassWidget):
            return widget.data
        elems = getattr(widget, "_elementWidgets", None)
        if elems is not None:
            values = []
            for e in elems:
                text = e.text()
                try:
                    values.append(eval(text))
                except Exception:
                    values.append(text)
            if getattr(widget, "_listIsTuple", False):
                return tuple(values)
            return values
        return None

    def _updateRevertButtonState(
        self, btn: RevertButton, current_val: Any, parent_val: Any
    ) -> None:
        """Enable or disable the revert button based on value comparison."""
        equal = GameData._isBlueprintValueEqual(current_val, parent_val)
        btn.setEnabled(not equal)

    def _onRevertAttr(self, key: str, parent_val: Any, widget: QtWidgets.QWidget) -> None:
        """Revert the attribute to its parent class value."""
        # For complex widgets where direct set is unreliable, rebuild the form
        is_complex = isinstance(widget, DataclassWidget) or hasattr(widget, "_elementWidgets")
        if is_complex:
            if isinstance(parent_val, (list, tuple)):
                value = copy.deepcopy(list(parent_val))
            elif dataclasses.is_dataclass(parent_val) and not isinstance(parent_val, type):
                value = dataclasses.asdict(parent_val)
            else:
                value = copy.deepcopy(parent_val)
            self.onDataChanged(key, value, True)
            self.refreshAttrs()
            return

        # Simple widgets: set value directly
        if isinstance(widget, QtWidgets.QCheckBox):
            widget.setChecked(bool(parent_val))
        elif isinstance(widget, QtWidgets.QSpinBox):
            try:
                widget.setValue(int(parent_val))
            except (ValueError, TypeError):
                pass
        elif isinstance(widget, QtWidgets.QDoubleSpinBox):
            try:
                widget.setValue(float(parent_val))
            except (ValueError, TypeError):
                pass
        elif isinstance(widget, QtWidgets.QLineEdit):
            widget.setText(str(parent_val))
        # onDataChanged will be triggered by the widget's own signal

    def _connectRevertUpdate(
        self, widget: QtWidgets.QWidget, revertBtn: RevertButton, parent_val: Any
    ) -> None:
        """Connect widget value-changed signals to keep the revert button state in sync."""
        if isinstance(widget, QtWidgets.QCheckBox):
            widget.toggled.connect(
                lambda checked, b=revertBtn, pv=parent_val: self._updateRevertButtonState(b, checked, pv)
            )
        elif isinstance(widget, (QtWidgets.QSpinBox, QtWidgets.QDoubleSpinBox)):
            widget.valueChanged.connect(
                lambda val, b=revertBtn, pv=parent_val: self._updateRevertButtonState(b, val, pv)
            )
        elif isinstance(widget, QtWidgets.QLineEdit):
            widget.textChanged.connect(
                lambda text, b=revertBtn, pv=parent_val: self._onRevertTextChanged(b, text, pv)
            )
        elif isinstance(widget, DataclassWidget):
            widget.VALUE_CHANGED.connect(
                lambda data, b=revertBtn, pv=parent_val: self._updateRevertButtonState(b, data, pv)
            )
        else:
            elems = getattr(widget, "_elementWidgets", None)
            if elems is not None:
                for e in elems:
                    if isinstance(e, QtWidgets.QLineEdit):
                        e.textChanged.connect(
                            lambda text, b=revertBtn, pv=parent_val, w=widget: self._onContainerRevertChanged(b, w, pv)
                        )

    def _onRevertTextChanged(self, btn: RevertButton, text: str, parent_val: Any) -> None:
        """Handle text changes from QLineEdit to update revert button state."""
        try:
            val = eval(text)
        except Exception:
            val = text
        self._updateRevertButtonState(btn, val, parent_val)

    def _onContainerRevertChanged(self, btn: RevertButton, widget: QtWidgets.QWidget, parent_val: Any) -> None:
        """Handle changes from list/tuple container widgets to update revert button state."""
        val = self._getWidgetCurrentValue(widget)
        self._updateRevertButtonState(btn, val, parent_val)

    def _getInvalidVars(self) -> Set[str]:
        cls = self._resolveClass()
        if cls is None:
            return set()
        invalid = getattr(cls, "_invalidVars", ())
        return set(invalid)

    def _getPathVars(self) -> Set[str]:
        cls = self._resolveClass()
        if cls is None:
            return set()
        paths = getattr(cls, "_pathVars", ())
        return set(paths)

    def _getRectRangeVars(self) -> Dict[str, str]:
        cls = self._resolveClass()
        if cls is None:
            return {}
        rects = getattr(cls, "_rectRangeVars", {})
        if isinstance(rects, dict):
            return dict(rects)
        return {}

    def _getMetaRely(self) -> Dict[str, Any]:
        cls = self._resolveClass()
        if cls is None or not isinstance(cls, type):
            return {}
        result: Dict[str, Any] = {}
        try:
            mro = list(reversed(cls.mro()))
        except:
            mro = [cls]
        for base in mro:
            meta = getattr(base, "__dict__", {}).get("_meta")
            if not isinstance(meta, dict):
                continue
            result.update(normaliseRelyMap(meta.get("Rely")))
        return result

    def _setWidgetEditable(self, widget: QtWidgets.QWidget, editable: bool) -> None:
        widget.setEnabled(editable)
        if isinstance(widget, (QtWidgets.QLineEdit, QtWidgets.QPlainTextEdit, QtWidgets.QTextEdit)):
            widget.setReadOnly(not editable)

    def _getRelyTooltip(self, key: str, relyEditable: bool) -> str:
        if relyEditable:
            return ""
        condition = getRelyConditionDisplay(key, self.attrRely)
        if not condition:
            return ""
        source, value = condition
        return ELOC("META_RELY_TOOLTIP").format(source=source, value=value)

    def _applyRelyTooltip(self, key: str, relyEditable: bool, *widgets: Optional[QtWidgets.QWidget]) -> None:
        tip = self._getRelyTooltip(key, relyEditable)
        for widget in widgets:
            if widget is not None:
                widget.setToolTip(tip)

    def _getComponentTypes(self, cls: Optional[type]) -> Dict[str, Any]:
        if cls is None or not isinstance(cls, type):
            return {}

        result: Dict[str, Any] = {}
        try:
            mro = list(reversed(cls.mro()))
        except:
            mro = [cls]

        for base in mro:
            componentTypes = getattr(base, "__dict__", {}).get("_componentTypes")
            if not isinstance(componentTypes, dict):
                continue
            for name, componentType in componentTypes.items():
                if (
                    isinstance(name, str)
                    and isinstance(componentType, type)
                    and dataclasses.is_dataclass(componentType)
                ):
                    result[name] = componentType
        return result

    def _getComponentFieldMap(self, componentTypes: Dict[str, Any]) -> Dict[str, str]:
        result: Dict[str, str] = {}
        for componentName, componentType in componentTypes.items():
            for field in dataclasses.fields(componentType):
                result[field.name] = componentName
        return result

    def _getComponentDefaults(self, componentType: Any) -> Dict[str, Any]:
        result: Dict[str, Any] = {}
        for field in dataclasses.fields(componentType):
            if field.default is not dataclasses.MISSING:
                result[field.name] = copy.deepcopy(field.default)
            elif field.default_factory is not dataclasses.MISSING:
                try:
                    result[field.name] = field.default_factory()
                except:
                    pass
        return result

    def _normaliseComponentData(self, componentType: Any, value: Any) -> Dict[str, Any]:
        if dataclasses.is_dataclass(value) and not isinstance(value, type):
            value = dataclasses.asdict(value)
        if not isinstance(value, dict):
            value = {}
        result = self._getComponentDefaults(componentType)
        for field in dataclasses.fields(componentType):
            if field.name in value:
                result[field.name] = copy.deepcopy(value[field.name])
        return result

    def _normaliseComponentAttrs(self, cls: Optional[type], attrs: Dict[str, Any]) -> bool:
        componentTypes = self._getComponentTypes(cls)
        changed = False
        for componentName, componentType in componentTypes.items():
            componentData = self._normaliseComponentData(componentType, attrs.get(componentName))
            moved = False
            for field in dataclasses.fields(componentType):
                if field.name not in attrs:
                    continue
                componentData[field.name] = attrs.pop(field.name)
                moved = True
            if moved:
                attrs[componentName] = componentData
                changed = True
        return changed

    def _addSeparator(self) -> None:
        line = QtWidgets.QFrame()
        line.setFrameShape(QtWidgets.QFrame.HLine)
        line.setFrameShadow(QtWidgets.QFrame.Sunken)
        self.formLayout.addRow(line)

    def _getComponentValue(
        self, cls: Optional[type], componentName: str, componentType: Any, attrs: Dict[str, Any]
    ) -> tuple[Optional[Dict[str, Any]], bool]:
        if componentName in attrs:
            return self._normaliseComponentData(componentType, attrs[componentName]), True
        found, value = self._getClassAttrValue(cls, componentName)
        if found:
            try:
                return self._normaliseComponentData(componentType, value), False
            except:
                pass
        return None, False

    def _getAddableComponents(
        self, cls: Optional[type], componentTypes: Dict[str, Any], attrs: Dict[str, Any]
    ) -> Dict[str, Any]:
        result: Dict[str, Any] = {}
        for componentName, componentType in componentTypes.items():
            if componentName in attrs:
                continue
            if self._hasClassAttr(cls, componentName):
                continue
            result[componentName] = componentType
        return result

    def _addComponentRows(self, cls: Optional[type], componentTypes: Dict[str, Any], attrs: Dict[str, Any]) -> None:
        items = []
        for componentName, componentType in componentTypes.items():
            value, isLocal = self._getComponentValue(cls, componentName, componentType, attrs)
            if value is None:
                continue
            items.append((componentName, componentType, isLocal))

        addable = self._getAddableComponents(cls, componentTypes, attrs)
        if not items and not addable:
            return

        container = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout(container)
        hbox.setContentsMargins(0, 0, 0, 0)
        hbox.setSpacing(4)

        listWidget = QtWidgets.QListWidget()
        listWidget.setFixedHeight(max(72, min(160, len(items) * 28 + 12)))
        listWidget.itemDoubleClicked.connect(self.onEditComponentItem)
        for componentName, componentType, isLocal in items:
            item = QtWidgets.QListWidgetItem(componentName)
            item.setData(QtCore.Qt.UserRole, componentName)
            item.setData(QtCore.Qt.UserRole + 1, componentType)
            item.setData(QtCore.Qt.UserRole + 2, isLocal)
            listWidget.addItem(item)
        hbox.addWidget(listWidget, 1)

        btnBox = QtWidgets.QWidget()
        btnLayout = QtWidgets.QVBoxLayout(btnBox)
        btnLayout.setContentsMargins(0, 0, 0, 0)
        btnLayout.setSpacing(4)

        addCompBtn = QtWidgets.QPushButton("+")
        addCompBtn.setToolTip(ELOC("ADD_COMPONENT"))
        addCompBtn.setFixedWidth(24)
        addCompBtn.setEnabled(bool(addable))
        addCompBtn.clicked.connect(lambda *_: QtCore.QTimer.singleShot(0, self.onAddComponent))
        btnLayout.addWidget(addCompBtn)

        delCompBtn = QtWidgets.QPushButton("-")
        delCompBtn.setObjectName("MinusBtn")
        delCompBtn.setFixedWidth(24)
        delCompBtn.clicked.connect(lambda _, w=listWidget: self.onDeleteSelectedComponent(w))
        btnLayout.addWidget(delCompBtn)
        btnLayout.addStretch()

        def updateDeleteButton() -> None:
            item = listWidget.currentItem()
            delCompBtn.setEnabled(item is not None and bool(item.data(QtCore.Qt.UserRole + 2)))

        listWidget.currentItemChanged.connect(lambda *_: updateDeleteButton())
        updateDeleteButton()
        hbox.addWidget(btnBox, 0)
        self.formLayout.addRow(QtWidgets.QLabel(ELOC("COMPONENTS")), container)

        if listWidget.count() > 0:
            listWidget.setCurrentRow(0)

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        toast = getattr(self, "toast", None)
        if isinstance(toast, Toast):
            toast._updatePosition()
            toast.raise_()

    def setupUI(self) -> None:
        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        self.splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        layout.addWidget(self.splitter)

        self.leftPanel = QtWidgets.QWidget()
        self.leftLayout = QtWidgets.QVBoxLayout(self.leftPanel)

        self.formLayout = QtWidgets.QFormLayout()
        self.leftLayout.addLayout(self.formLayout)
        self.leftLayout.addStretch()

        self.leftScroll = QtWidgets.QScrollArea()
        self.leftPanel.setSizePolicy(QtWidgets.QSizePolicy.Preferred, QtWidgets.QSizePolicy.Expanding)
        self.leftPanel.setMinimumWidth(320)
        self.leftScroll.setWidgetResizable(True)
        self.leftScroll.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.leftScroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.leftScroll.setFrameShape(QtWidgets.QFrame.NoFrame)
        self.leftScroll.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.leftScroll.setMinimumWidth(320)
        self.leftScroll.setWidget(self.leftPanel)

        self.rightPanel = QtWidgets.QWidget()
        self.rightLayout = QtWidgets.QVBoxLayout(self.rightPanel)

        self.nodeGraphList = QtWidgets.QListWidget()
        self.nodeGraphList.setFlow(QtWidgets.QListWidget.LeftToRight)
        self.nodeGraphList.setFixedHeight(50)
        self.rightLayout.addWidget(self.nodeGraphList)
        self.nodeGraphList.currentTextChanged.connect(self.onGraphSelected)
        self.nodeGraphList.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.nodeGraphList.customContextMenuRequested.connect(self.onGraphListContextMenu)
        self._delShortcut = QtWidgets.QShortcut(
            QtGui.QKeySequence.Delete, self.nodeGraphList, context=QtCore.Qt.WidgetShortcut
        )
        self._delShortcut.activated.connect(self._onDeleteEvent)
        self._renameShortcut = QtWidgets.QShortcut(
            QtGui.QKeySequence(QtCore.Qt.Key_F2), self.nodeGraphList, context=QtCore.Qt.WidgetShortcut
        )
        self._renameShortcut.activated.connect(self._onRenameEvent)

        self.stackedWidget = QtWidgets.QStackedWidget()
        self.rightLayout.addWidget(self.stackedWidget)

        self.splitter.addWidget(self.leftScroll)
        self.splitter.addWidget(self.rightPanel)
        self.splitter.setStretchFactor(0, 1)
        self.splitter.setStretchFactor(1, 1)
        self.splitter.setSizes([max(320, int(self.width() * 0.4)), max(320, int(self.width() * 0.6))])

        self.refreshAttrs()
        self.refreshGraphList()

    def onGraphSelected(self, text: str) -> None:
        if not text:
            return

        if text in self.graphs:
            self.stackedWidget.setCurrentWidget(self.graphs[text])
            return

        parentCls = self._resolveClass()

        graph = GameData.genGraphFromData(
            self.data["graph"],
            parentCls,
        )
        panel = NodePanel(self, graph, text, self.title, self._refreshData)
        self.graphs[text] = panel
        self.stackedWidget.addWidget(panel)
        self.stackedWidget.setCurrentWidget(panel)

    def refreshGraphList(self) -> None:
        self.nodeGraphList.clear()
        if "graph" in self.data and isinstance(self.data["graph"], dict):
            nodeGraph = self.data["graph"].get("nodeGraph")
            if isinstance(nodeGraph, dict):
                for key in nodeGraph.keys():
                    self.nodeGraphList.addItem(key)

        if self.nodeGraphList.count() > 0:
            self.nodeGraphList.setCurrentRow(0)

    def _getDisplayOrder(self, attrs: Dict[str, Any], cls: Optional[type]) -> list[str]:
        if not cls or not isinstance(cls, type):
            return list(attrs.keys())

        defined_order = []
        try:
            mro = list(reversed(cls.mro()))
        except:
            mro = [cls]

        for base in mro:
            if base is object:
                continue

            ann = getattr(base, "__annotations__", {})
            for k in ann:
                if k not in defined_order:
                    defined_order.append(k)

            for k in getattr(base, "__dict__", {}):
                if k.startswith("_"):
                    continue
                if k in defined_order:
                    continue
                try:
                    v = getattr(base, k)
                    if callable(v) or isinstance(v, property):
                        continue
                except:
                    pass
                defined_order.append(k)

        ordered = [k for k in defined_order if k in attrs]
        seen = set(ordered)
        remaining = [k for k in attrs.keys() if k not in seen]
        return ordered + remaining

    def _copyAttrValue(self, value: Any) -> Any:
        if dataclasses.is_dataclass(value) and not isinstance(value, type):
            return dataclasses.asdict(value)
        try:
            return copy.deepcopy(value)
        except:
            return value

    def _getParentDisplayAttrs(self, parent_cls: Optional[type], attrs: Dict[str, Any]) -> Dict[str, Any]:
        if not parent_cls:
            return {}

        result = {}
        for attr_name in dir(parent_cls):
            if attr_name.startswith("_") or attr_name in attrs:
                continue

            try:
                attr_val = getattr(parent_cls, attr_name)
            except:
                continue

            if callable(attr_val) or isinstance(attr_val, property):
                continue

            result[attr_name] = self._copyAttrValue(attr_val)
        return result

    def refreshAttrs(self) -> None:
        while self.formLayout.rowCount() > 0:
            self.formLayout.removeRow(0)

        cls = self._resolveClass()
        self.attrRely = self._getMetaRely()
        self.attrRelySources = getRelySourceSet(self.attrRely)
        type_hints = {}
        parent_cls = None
        parent_hints = {}
        if cls is not None:
            try:
                type_hints = get_type_hints(cls)
            except:
                type_hints = getattr(cls, "__annotations__", {})

            parent_cls = self._getBaseClass(cls)
            if parent_cls is not None:
                try:
                    parent_hints = get_type_hints(parent_cls)
                except:
                    parent_hints = getattr(parent_cls, "__annotations__", {})

        parent_val = self.data.get("parent", "")
        label = QtWidgets.QLabel(ELOC("PARENT"))
        widget = self.createInputWidget("parent", parent_val, isAttr=False)
        self.formLayout.addRow(label, widget)
        self._addSeparator()

        attrs = self.data.get("attrs")
        if not isinstance(attrs, dict):
            attrs = {}
            self.data["attrs"] = attrs

        target_cls = cls if cls is not None else parent_cls
        componentsChanged = self._normaliseComponentAttrs(target_cls, attrs)
        if componentsChanged:
            GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        componentTypes = self._getComponentTypes(target_cls)
        componentFieldMap = self._getComponentFieldMap(componentTypes)
        componentSkipKeys = set(componentTypes.keys()) | set(componentFieldMap.keys())
        componentParentCls = parent_cls if parent_cls is not None else target_cls
        self._addComponentRows(componentParentCls, componentTypes, attrs)
        self._addSeparator()

        displayAttrs = self._getParentDisplayAttrs(parent_cls, attrs)
        displayAttrs.update(attrs)
        displayAttrs = {k: v for k, v in displayAttrs.items() if k not in componentSkipKeys}
        display_keys = self._getDisplayOrder(displayAttrs, target_cls)

        for key in display_keys:
            value = displayAttrs[key]
            label = QtWidgets.QLabel(str(key))
            container = QtWidgets.QWidget()
            hbox = QtWidgets.QHBoxLayout(container)
            hbox.setContentsMargins(0, 0, 0, 0)
            hbox.setSpacing(4)

            is_dc = False
            type_hint = type_hints.get(key)
            if type_hint is None and key in parent_hints:
                type_hint = parent_hints[key]

            found_attr_parent, attr_parent_val = self._getClassAttrValue(parent_cls, key)
            if not found_attr_parent:
                attr_parent_val = None

            if type_hint and dataclasses.is_dataclass(type_hint):
                widget = DataclassWidget(type_hint, value)
                widget.VALUE_CHANGED.connect(lambda val, k=key: self.onDataChanged(k, val, True))
                is_dc = True
            else:
                widget = self.createInputWidget(key, value, type_hint=type_hint, parent_val=attr_parent_val)

            isInvalid = key in self.invalidVars
            isRectRange = key in self.rectRangeVars and not isInvalid
            isPath = key in self.pathVars and not isInvalid and not isRectRange
            relyEditable = isRelyEditable(key, self.attrRely, displayAttrs)

            if is_dc:
                if isInvalid:
                    widget.setEnabled(False)
            elif isinstance(widget, QtWidgets.QLineEdit):
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
                pathBtn.clicked.connect(lambda _, k=key, w=widget: self.onSelectPath(k, w))
                pathBtn.setEnabled(relyEditable)
                hbox.addWidget(pathBtn, 0)

            if isRectRange:
                rectBtn = QtWidgets.QPushButton("...")
                rectBtn.setObjectName("RectBtn")
                rectBtn.setFixedWidth(24)
                rectBtn.clicked.connect(lambda _, k=key: self.onEditRectRange(k))
                rectBtn.setEnabled(relyEditable)
                hbox.addWidget(rectBtn, 0)

            self._applyRelyTooltip(key, relyEditable, label, container, widget, pathBtn, rectBtn)

            has_parent_attr = False
            if parent_cls:
                if self._hasClassAttr(parent_cls, key):
                    has_parent_attr = True
                elif key in parent_hints:
                    has_parent_attr = True

            if not has_parent_attr:
                minusBtn = QtWidgets.QPushButton("-")
                minusBtn.setObjectName("MinusBtn")
                minusBtn.setFixedWidth(24)
                minusBtn.clicked.connect(lambda _, k=key: self.onDeleteAttr(k))
                hbox.addWidget(minusBtn, 0)
            else:
                revertBtn = RevertButton()
                revertBtn.setEnabled(relyEditable)
                if relyEditable:
                    current_val = self._getWidgetCurrentValue(widget)
                    self._updateRevertButtonState(revertBtn, current_val, attr_parent_val)
                revertBtn.clicked.connect(
                    lambda _, k=key, pv=attr_parent_val, w=widget: self._onRevertAttr(k, pv, w)
                )
                self._connectRevertUpdate(widget, revertBtn, attr_parent_val)
                hbox.addWidget(revertBtn, 0)

            self.formLayout.addRow(label, container)

        addBtn = QtWidgets.QPushButton("+")
        addBtn.clicked.connect(self.onAddAttr)
        self.formLayout.addRow(addBtn)

    def createInputWidget(
        self, key: str, value: Any, isAttr: bool = True, type_hint: Any = None, parent_val: Any = None
    ) -> QtWidgets.QWidget:
        if isAttr and isinstance(value, bool):
            w = QtWidgets.QCheckBox()
            w.setChecked(bool(value))
            w.toggled.connect(lambda checked, k=key: self.onDataChanged(k, checked, True))
            return w

        if isAttr and (type_hint is int or (isinstance(value, int) and not isinstance(value, bool))):
            w = QtWidgets.QSpinBox()
            w.setRange(-2147483648, 2147483647)
            try:
                w.setValue(int(value))
            except (ValueError, TypeError):
                w.setValue(0)
            w.valueChanged.connect(lambda val, k=key: self.onDataChanged(k, val, True))
            return w

        if isAttr and (type_hint is float or isinstance(value, float)):
            w = QtWidgets.QDoubleSpinBox()
            w.setRange(-999999999.0, 999999999.0)
            try:
                w.setValue(float(value))
            except (ValueError, TypeError):
                w.setValue(0.0)
            w.valueChanged.connect(lambda val, k=key: self.onDataChanged(k, val, True))
            return w

        is_list = False
        if isAttr:
            if type_hint:
                origin = getattr(type_hint, "__origin__", None)
                if origin is list:
                    is_list = True
            elif parent_val is not None and isinstance(parent_val, list):
                is_list = True
            elif isinstance(value, list):
                is_list = True

        if is_list:
            container = QtWidgets.QWidget()
            layout = QtWidgets.QVBoxLayout(container)
            layout.setContentsMargins(0, 0, 0, 0)
            layout.setSpacing(2)

            if not isinstance(value, list):
                if isinstance(value, tuple):
                    value = list(value)
                else:
                    value = []

            edits = []
            for i, item in enumerate(value):
                row = QtWidgets.QWidget()
                row_layout = QtWidgets.QHBoxLayout(row)
                row_layout.setContentsMargins(0, 0, 0, 0)
                row_layout.setSpacing(2)

                e = QtWidgets.QLineEdit(str(item))
                e.textChanged.connect(lambda _, k=key, c=container: self._onListItemChanged(k, c))
                row_layout.addWidget(e)
                edits.append(e)

                removeBtn = QtWidgets.QPushButton("-")
                removeBtn.setFixedWidth(24)
                removeBtn.clicked.connect(lambda _, idx=i, k=key, c=container: self._onRemoveListItem(k, c, idx))
                row_layout.addWidget(removeBtn)

                layout.addWidget(row)

            addBtn = QtWidgets.QPushButton("+")
            addBtn.clicked.connect(lambda _, k=key, c=container: self._onAddListItem(k, c))
            layout.addWidget(addBtn)

            container._elementWidgets = edits
            container._listIsTuple = False
            container._originalTypeHint = type_hint
            return container

        if isAttr and isinstance(value, tuple):
            container = QtWidgets.QWidget()
            layout = QtWidgets.QHBoxLayout(container)
            layout.setContentsMargins(0, 0, 0, 0)
            layout.setSpacing(2)
            edits = []
            for item in value:
                e = QtWidgets.QLineEdit(str(item))
                layout.addWidget(e)
                edits.append(e)
            container._elementWidgets = edits
            container._listIsTuple = True
            for _e in edits:
                _e.textChanged.connect(lambda _, k=key, c=container: self._onListItemChanged(k, c))
            return container
        w = QtWidgets.QLineEdit(str(value))
        w.textChanged.connect(lambda val, k=key, attr=isAttr: self.onDataChanged(k, val, attr))
        return w

    def _onRemoveListItem(self, key: str, container: QtWidgets.QWidget, index: int) -> None:
        elems = getattr(container, "_elementWidgets", [])
        if 0 <= index < len(elems):
            values = []
            for i, e in enumerate(elems):
                if i == index:
                    continue
                text = e.text()
                try:
                    v = eval(text)
                except:
                    v = text
                values.append(v)

            self.onDataChanged(key, values, True)
            self.refreshAttrs()

    def _onAddListItem(self, key: str, container: QtWidgets.QWidget) -> None:
        elems = getattr(container, "_elementWidgets", [])
        values = []
        for e in elems:
            text = e.text()
            try:
                v = eval(text)
            except:
                v = text
            values.append(v)

        default_val = ""
        type_hint = getattr(container, "_originalTypeHint", None)
        if type_hint:
            args = getattr(type_hint, "__args__", [])
            if args:
                arg_type = args[0]
                if arg_type is int:
                    default_val = 0
                elif arg_type is float:
                    default_val = 0.0
                elif arg_type is bool:
                    default_val = False
                elif arg_type is str:
                    default_val = ""
        elif values:
            try:
                default_val = type(values[-1])()
            except:
                pass

        values.append(default_val)
        self.onDataChanged(key, values, True)
        self.refreshAttrs()

    def _onListItemChanged(self, key: str, container: QtWidgets.QWidget) -> None:
        elems = getattr(container, "_elementWidgets", [])
        values = []
        for e in elems:
            text = e.text()
            try:
                v = eval(text)
            except:
                v = text
            values.append(v)
        if getattr(container, "_listIsTuple", False):
            self.onDataChanged(key, tuple(values), True)
        else:
            self.onDataChanged(key, values, True)

    def onDataChanged(self, key: str, value: Any, isAttr: bool = True) -> None:
        try:
            value = eval(value)
        except:
            pass
        if isAttr:
            if "attrs" in self.data and isinstance(self.data["attrs"], dict):
                self.data["attrs"][key] = value
        else:
            self.data[key] = value
        GameData.recordSnapshot()
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.MODIFIED.emit()
        if isAttr and key in self.attrRelySources:
            QtCore.QTimer.singleShot(0, self.refreshAttrs)

    def onDeleteAttr(self, key: str) -> None:
        if "attrs" in self.data and isinstance(self.data["attrs"], dict):
            if key in self.data["attrs"]:
                GameData.recordSnapshot()
                del self.data["attrs"][key]
                self.refreshAttrs()
                GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
                self.MODIFIED.emit()

    def onDeleteComponent(self, key: str) -> None:
        self.onDeleteAttr(key)

    def onDeleteSelectedComponent(self, listWidget: QtWidgets.QListWidget) -> None:
        item = listWidget.currentItem()
        if item is None or not bool(item.data(QtCore.Qt.UserRole + 2)):
            return
        key = item.data(QtCore.Qt.UserRole)
        if isinstance(key, str):
            QtCore.QTimer.singleShot(0, lambda k=key: self.onDeleteComponent(k))

    def onEditComponentItem(self, item: QtWidgets.QListWidgetItem) -> None:
        key = item.data(QtCore.Qt.UserRole)
        if isinstance(key, str):
            QtCore.QTimer.singleShot(0, lambda k=key: self.onEditComponent(k))

    def onEditComponent(self, key: str) -> None:
        attrs = self.data.get("attrs")
        if not isinstance(attrs, dict):
            attrs = {}
            self.data["attrs"] = attrs

        cls = self._resolveClass()
        componentTypes = self._getComponentTypes(cls)
        componentType = componentTypes.get(key)
        if componentType is None:
            return

        parent_cls = None
        if cls is not None:
            parent_cls = self._getBaseClass(cls)
        sourceCls = parent_cls if parent_cls is not None else cls
        value, _ = self._getComponentValue(sourceCls, key, componentType, attrs)
        if value is None:
            value = self._getComponentDefaults(componentType)

        dlg = QtWidgets.QDialog(self)
        dlg.setWindowTitle(key)
        layout = QtWidgets.QVBoxLayout(dlg)
        widget = DataclassWidget(componentType, copy.deepcopy(value), dlg)
        layout.addWidget(widget)
        buttons = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        buttons.accepted.connect(dlg.accept)
        buttons.rejected.connect(dlg.reject)
        layout.addWidget(buttons)
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return

        GameData.recordSnapshot()
        attrs[key] = copy.deepcopy(widget.data)
        self.refreshAttrs()
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.MODIFIED.emit()

    def onAddComponent(self) -> None:
        attrs = self.data.get("attrs")
        if not isinstance(attrs, dict):
            attrs = {}
            self.data["attrs"] = attrs

        cls = self._resolveClass()
        componentTypes = self._getComponentTypes(cls)
        parent_cls = None
        if cls is not None:
            parent_cls = self._getBaseClass(cls)
        addable = self._getAddableComponents(parent_cls, componentTypes, attrs)
        if not addable:
            return

        displayItems = [f"{name} ({componentType.__name__})" for name, componentType in addable.items()]
        selected, ok = QtWidgets.QInputDialog.getItem(
            self,
            ELOC("ADD_COMPONENT"),
            ELOC("COMPONENT_NAME"),
            displayItems,
            0,
            False,
        )
        if not ok or not selected:
            return

        componentName = selected.split(" ", 1)[0]
        componentType = addable.get(componentName)
        if componentType is None:
            return

        GameData.recordSnapshot()
        attrs[componentName] = self._getComponentDefaults(componentType)
        self.refreshAttrs()
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.MODIFIED.emit()

    def onAddAttr(self) -> None:
        dlg = SingleRowDialog(self, ELOC("ADD_ATTR"), ELOC("ATTR_NAME"), "", None)
        ok, key = dlg.execGetText()
        if ok:
            key = key.strip()
            if not key:
                return

            if key[0].isdigit():
                QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("ATTR_NAME_CANNOT_START_WITH_DIGIT"))
                return

            if key in self.invalidVars:
                QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("INVALID_NAME"))
                return

            if "attrs" not in self.data or not isinstance(self.data["attrs"], dict):
                self.data["attrs"] = {}
            cls = self._resolveClass()
            componentTypes = self._getComponentTypes(cls)
            componentFieldMap = self._getComponentFieldMap(componentTypes)
            if key in componentTypes or key in componentFieldMap:
                QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("INVALID_NAME"))
                return
            if key in self.data["attrs"]:
                QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("ATTR_EXISTS"))
                return

            GameData.recordSnapshot()
            self.data["attrs"][key] = ""
            self.refreshAttrs()
            GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
            self.MODIFIED.emit()

    def onSelectPath(self, key: str, widget: QtWidgets.QLineEdit) -> None:
        baseDir = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Characters")
        if not os.path.isdir(baseDir):
            baseDir = EditorStatus.PROJ_PATH
        dlg = FileSelectorDialog(self, baseDir, FileSelectorDialog.allFilesFilter(star=True))
        filePath = dlg.execSelect()
        if not filePath:
            return
        try:
            relPath = os.path.relpath(filePath, baseDir)
        except ValueError:
            relPath = filePath
        relPath = relPath.replace("\\", "/")
        widget.setText(relPath)

    def onEditRectRange(self, key: str) -> None:
        attrs = self.data.get("attrs")
        if not isinstance(attrs, dict):
            return
        pathKey = self.rectRangeVarMap.get(key)
        if not pathKey:
            return
        pathValue = attrs.get(pathKey)
        if not isinstance(pathValue, str) or not pathValue:
            cls = self._resolveClass()
            if cls is not None:
                found, parentVal = self._getClassAttrValue(cls, pathKey)
                if found and isinstance(parentVal, str) and parentVal:
                    pathValue = parentVal
        if not isinstance(pathValue, str) or not pathValue:
            return
        baseDir = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Characters")
        imagePath = os.path.join(baseDir, pathValue)
        rectValue = attrs.get(key)
        rectTuple = None
        if isinstance(rectValue, (list, tuple)) and len(rectValue) >= 2:
            p0 = rectValue[0]
            p1 = rectValue[1]
            if isinstance(p0, (list, tuple)) and len(p0) >= 2 and isinstance(p1, (list, tuple)) and len(p1) >= 2:
                try:
                    x = int(p0[0])
                    y = int(p0[1])
                    w = int(p1[0])
                    h = int(p1[1])
                    rectTuple = (x, y, w, h)
                except Exception:
                    rectTuple = None
        if rectTuple is None:
            cell = getattr(EditorStatus, "CELLSIZE", 0)
            if not isinstance(cell, int) or cell <= 0:
                cell = 32
            rectTuple = (0, 0, cell, cell)
        dlg = RectViewer(self, imagePath, rectTuple)
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return
        nx, ny, nw, nh = dlg.getRectTuple()
        newValue = ((nx, ny), (nw, nh))
        self.onDataChanged(key, newValue, True)
        self.refreshAttrs()

    def _refreshListFromData(self) -> None:
        if self.title not in GameData.blueprintsData:
            return
        self.data = copy.deepcopy(GameData.blueprintsData[self.title])
        self.refreshAttrs()
        current_row = self.nodeGraphList.currentRow()
        self.refreshGraphList()
        if current_row >= 0 and current_row < self.nodeGraphList.count():
            self.nodeGraphList.setCurrentRow(current_row)

    def _refreshCurrentPanel(self) -> None:
        current_widget = self.stackedWidget.currentWidget()
        if isinstance(current_widget, NodePanel):
            graph_key = current_widget.name
            parentCls = self._resolveClass()
            graph = GameData.genGraphFromData(
                self.data["graph"],
                parentCls,
            )
            current_widget.nodeGraph = graph
            current_widget._refreshPanel()

    def _onUndo(self) -> None:
        diffs = GameData.undo()
        self._refreshListFromData()
        self._refreshCurrentPanel()
        File.mainWindow.setWindowTitle(System.GetTitle())
        if diffs:
            self.toast.showMessage("Undo:\n" + "\n".join(diffs))

    def _onRedo(self) -> None:
        diffs = GameData.redo()
        self._refreshListFromData()
        self._refreshCurrentPanel()
        File.mainWindow.setWindowTitle(System.GetTitle())
        if diffs:
            self.toast.showMessage("Redo:\n" + "\n".join(diffs))

    def _refreshData(self, name: str, data: Dict[str, Any]) -> None:
        GameData.recordSnapshot()
        if name in GameData.blueprintsData:
            GameData.blueprintsData[name]["graph"] = data
        self.data["graph"] = data
        self.MODIFIED.emit()

    def onGraphListContextMenu(self, pos: QtCore.QPoint) -> None:
        index = self.nodeGraphList.indexAt(pos)
        has_item = index.isValid()
        if has_item:
            self.nodeGraphList.setCurrentRow(index.row())
        menu = QtWidgets.QMenu(self)
        action_new = menu.addAction(ELOC("NEW_EVENT"))
        if action_new is None:
            return
        action_new.triggered.connect(self._onNewEvent)
        if has_item:
            action_rename = menu.addAction(ELOC("RENAME_EVENT"))
            action_del = menu.addAction(ELOC("DELETE_EVENT"))

            if action_rename is None or action_del is None:
                return

            action_rename.triggered.connect(self._onRenameEvent)
            action_del.triggered.connect(self._onDeleteEvent)
        menu.exec_(self.nodeGraphList.mapToGlobal(pos))

    def _onNewEvent(self) -> None:
        dlg = SingleRowDialog(self, ELOC("NEW_EVENT"), ELOC("ENTER_EVENT_NAME"), "", None)
        ok, name = dlg.execGetText()
        if not ok:
            return
        name = name.strip()
        if not name:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("INVALID_NAME"))
            return
        if name[0].isdigit():
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("ATTR_NAME_CANNOT_START_WITH_DIGIT"))
            return
        graph = self.data.get("graph")
        if not isinstance(graph, dict):
            graph = {}
            self.data["graph"] = graph
        nodeGraph = graph.get("nodeGraph")
        if not isinstance(nodeGraph, dict):
            nodeGraph = {}
            graph["nodeGraph"] = nodeGraph
        startNodes = graph.get("startNodes")
        if not isinstance(startNodes, dict):
            startNodes = {}
            graph["startNodes"] = startNodes
        if name in nodeGraph:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("EVENT_EXISTS"))
            return
        GameData.recordSnapshot()
        nodeGraph[name] = {"nodes": [], "links": []}
        startNodes[name] = None
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.refreshGraphList()
        items = self.nodeGraphList.findItems(name, QtCore.Qt.MatchExactly)
        if items:
            self.nodeGraphList.setCurrentItem(items[0])
        self.MODIFIED.emit()

    def _onDeleteEvent(self) -> None:
        item = self.nodeGraphList.currentItem()
        if not item:
            return
        name = item.text()
        ret = QtWidgets.QMessageBox.question(
            self,
            ELOC("CONFIRM_DELETE"),
            ELOC("CONFIRM_DELETE_EVENT").format(name=name),
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No,
        )
        if ret != QtWidgets.QMessageBox.Yes:
            return
        graph = self.data.get("graph")
        if not isinstance(graph, dict):
            return
        nodeGraph = graph.get("nodeGraph")
        startNodes = graph.get("startNodes")
        GameData.recordSnapshot()
        if isinstance(nodeGraph, dict) and name in nodeGraph:
            del nodeGraph[name]
        if isinstance(startNodes, dict) and name in startNodes:
            del startNodes[name]
        if name in self.graphs:
            panel = self.graphs.pop(name)
            self.stackedWidget.removeWidget(panel)
            panel.deleteLater()
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.refreshGraphList()
        if self.nodeGraphList.count() > 0:
            self.nodeGraphList.setCurrentRow(0)
        self.MODIFIED.emit()

    def _onRenameEvent(self) -> None:
        item = self.nodeGraphList.currentItem()
        if not item:
            return
        old_name = item.text()
        dlg = SingleRowDialog(self, ELOC("RENAME_EVENT"), ELOC("ENTER_EVENT_NAME"), old_name, None)
        ok, new_name = dlg.execGetText()
        if not ok:
            return
        new_name = new_name.strip()
        if not new_name or new_name == old_name:
            return
        if new_name[0].isdigit():
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("ATTR_NAME_CANNOT_START_WITH_DIGIT"))
            return
        graph = self.data.get("graph")
        if not isinstance(graph, dict):
            return
        nodeGraph = graph.get("nodeGraph")
        startNodes = graph.get("startNodes")
        if not isinstance(nodeGraph, dict) or not isinstance(startNodes, dict):
            return
        if new_name in nodeGraph:
            QtWidgets.QMessageBox.warning(self, ELOC("ERROR"), ELOC("EVENT_EXISTS"))
            return
        GameData.recordSnapshot()
        new_nodeGraph = {}
        for k, v in nodeGraph.items():
            if k == old_name:
                new_nodeGraph[new_name] = v
            else:
                new_nodeGraph[k] = v
        graph["nodeGraph"] = new_nodeGraph
        new_startNodes = {}
        for k, v in startNodes.items():
            if k == old_name:
                new_startNodes[new_name] = v
            else:
                new_startNodes[k] = v
        graph["startNodes"] = new_startNodes
        if old_name in self.graphs:
            panel = self.graphs.pop(old_name)
            panel.setName(new_name)
            self.graphs[new_name] = panel
        GameData.blueprintsData[self.title] = copy.deepcopy(self.data)
        self.refreshGraphList()
        items = self.nodeGraphList.findItems(new_name, QtCore.Qt.MatchExactly)
        if items:
            self.nodeGraphList.setCurrentItem(items[0])
        self.MODIFIED.emit()
