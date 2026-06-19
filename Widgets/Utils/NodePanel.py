# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
import inspect
import copy
from typing import TYPE_CHECKING, Any, Callable, Dict, List, Optional, Union, Tuple
from PyQt5 import QtWidgets, QtCore, QtGui
from NodeGraphQt import NodeGraph, BaseNode
from NodeGraphQt.widgets.viewer import NodeViewer
from NodeGraphQt.widgets.scene import NodeScene
from NodeGraphQt.widgets.node_widgets import NodeBaseWidget
from NodeGraphQt.qgraphics.port import PortItem
from NodeGraphQt.qgraphics.node_base import NodeItem
from EditorGlobal import EditorStatus, GameData
from Utils import System
from .ColourPickerDialog import ColourVarEditor
from .DialogUtils import getIndependentDialogParent
from .FileSelectorDialog import FileSelectorDialog
from .FunctionPickerPopup import FunctionPickerPopup
from .MetaRely import getRelyConditionDisplay, isRelyEditable, normaliseRelyMap, toBool
from .MetaVarTypes import getMetaVarTypes
from .GraphLayout import computeGraphLayoutPositions
from .MoveRouteEditor import MoveRouteEditor
from .NodeFunctionMeta import getExecSplits, getLatents, getReturnTypes, hasExecOutputs
from .TransferPosEditor import TransferPosEditor
from .TypedValueEditor import (
    TypedValueEditor,
    formatParamTypeName,
    getSimpleContainerEditorType,
    parseParamInitialValue,
)
from .VectorVarEditor import VectorVarEditor, isVectorVarType, normaliseVectorVarType

if TYPE_CHECKING:
    from Engine.NodeGraph.Graph import Graph  # type: ignore
    from Engine.NodeGraph.Node import Node as GraphNode  # type: ignore


class CustomViewer(NodeViewer):
    LIVE_CONNECTION_PROMPT = QtCore.pyqtSignal(object, QtCore.QPointF)

    def applyLiveConnection(self, event):
        pos_items = self.scene().items(event.scenePos())
        end_port = None
        for item in pos_items:
            if isinstance(item, PortItem):
                end_port = item
                break
        if (
            self._LIVE_PIPE.isVisible()
            and end_port is None
            and self._detached_port is None
            and self._start_port is not None
        ):
            self.LIVE_CONNECTION_PROMPT.emit(self._start_port, event.scenePos())
            return
        return super(CustomViewer, self).applyLiveConnection(event)


MIN_VIEW_SCALE = 0.05
MAX_VIEW_SCALE = 5.0
DEFAULT_PARAM_NODE_IDENTIFIER = "!DefaultParam"
DEFAULT_PARAM_NODE_TYPE = "!DefaultParam.DefaultParamNode"  # type_ = __identifier__ + '.' + __name__
TRACKPAD_ZOOM_WHEEL_BLOCK_MS = 200
PAN_EMULATION_MODIFIER = QtCore.Qt.MetaModifier
WidgetValue = Any
CONNECTED_PARAM_VALUE = object()


def isBasicPythonDefault(value: Any) -> bool:
    if value is None or isinstance(value, (bool, int, float, str)):
        return True
    if isinstance(value, (list, tuple)):
        return all(isBasicPythonDefault(item) for item in value)
    if isinstance(value, dict):
        return all(isBasicPythonDefault(key) and isBasicPythonDefault(item) for key, item in value.items())
    return False


def formatNodeParamDefault(value: Any) -> str:
    if value is inspect.Parameter.empty:
        return ""
    if isinstance(value, str):
        return value
    if isBasicPythonDefault(value):
        return repr(value)
    return str(value)


def makeNodeParamsFromSignature(func: Callable) -> List[str]:
    sig = inspect.signature(func)
    params: List[str] = []
    for _name, param in sig.parameters.items():
        params.append(formatNodeParamDefault(param.default))
    return params


def getNodeParamValue(node: GraphNode, paramIndex: int, paramName: str) -> Any:
    if paramIndex < len(node.params):
        return node.params[paramIndex]
    paramDefaults = node.getParamDefaults()
    if paramName in paramDefaults:
        return formatNodeParamDefault(paramDefaults[paramName])
    return ""


def _patchDetachedNodePaintGuard() -> None:
    if getattr(NodeItem, "_ludorkDetachedPaintGuard", False):
        return

    originalPaint = NodeItem.paint

    def safePaint(
        self: NodeItem,
        painter: QtGui.QPainter,
        option: QtWidgets.QStyleOptionGraphicsItem,
        widget: Optional[QtWidgets.QWidget],
    ) -> None:
        scene = self.scene()
        if not isinstance(scene, NodeScene):
            return
        viewer = scene.viewer()
        if not isinstance(viewer, NodeViewer):
            return
        return originalPaint(self, painter, option, widget)

    NodeItem.paint = safePaint
    NodeItem._ludorkDetachedPaintGuard = True


_patchDetachedNodePaintGuard()


_RESERVED_NODE_PROPERTIES = {
    "type_",
    "id",
    "icon",
    "name",
    "color",
    "border_color",
    "text_color",
    "disabled",
    "selected",
    "visible",
    "width",
    "height",
    "pos",
    "layout_direction",
    "inputs",
    "outputs",
    "port_deletion_allowed",
    "subgraph_session",
}


def MakeSafeNodePropertyName(name: str, usedNames: set) -> str:
    base = name
    if base in _RESERVED_NODE_PROPERTIES:
        if base:
            base = f"param{base[:1].upper()}{base[1:]}"
        else:
            base = "param"
    if base in _RESERVED_NODE_PROPERTIES:
        base = "param"
    safe = base
    i = 2
    while safe in usedNames or safe in _RESERVED_NODE_PROPERTIES:
        safe = f"{base}{i}"
        i += 1
    usedNames.add(safe)
    return safe


def getMetaPathVars(meta: Any) -> Dict[str, str]:
    if not isinstance(meta, dict):
        return {}

    result: Dict[str, str] = {}
    collectMetaPathVars(result, meta.get("PathVars", ()))
    return result


def getMetaMoveRouteVars(meta: Any) -> set[str]:
    if not isinstance(meta, dict):
        return set()
    rawVars = meta.get("MoveRouteVars")
    if isinstance(rawVars, str):
        return {rawVars}
    if isinstance(rawVars, (list, tuple, set)):
        return {name for name in rawVars if isinstance(name, str)}
    return set()


def getMetaTransferVars(meta: Any) -> Dict[str, str]:
    if not isinstance(meta, dict):
        return {}
    rawVars = meta.get("Transfer")
    if not isinstance(rawVars, (list, tuple)):
        return {}
    result: Dict[str, str] = {}
    for item in rawVars:
        if isinstance(item, (list, tuple)) and len(item) >= 2:
            locName, mapName = str(item[0]), str(item[1])
            if locName and mapName:
                result[locName] = mapName
    return result


def collectMetaPathVars(paths: Dict[str, str], value: Any) -> None:
    if isinstance(value, tuple) and len(value) >= 2 and isinstance(value[0], str):
        paths[value[0]] = normalisePathVarAssetsDir(value[1])
        return
    if isinstance(value, (list, tuple, set)):
        for item in value:
            if isinstance(item, str):
                paths[item] = "Characters"
                continue
            collectMetaPathVars(paths, item)


def normalisePathVarAssetsDir(value: Any) -> str:
    if not isinstance(value, str):
        return ""
    value = value.replace("\\", "/").strip("/")
    if value in ("", "."):
        return ""
    return value


def getPathVarBaseDir(pathVarMap: Dict[str, str], key: str) -> str:
    assetsDir = os.path.join(EditorStatus.PROJ_PATH, "Assets")
    subDir = pathVarMap.get(key, "")
    if not subDir:
        return assetsDir
    baseDir = os.path.normpath(os.path.join(assetsDir, subDir))
    try:
        assetsAbs = os.path.normcase(os.path.abspath(assetsDir))
        baseAbs = os.path.normcase(os.path.abspath(baseDir))
        if os.path.commonpath([assetsAbs, baseAbs]) != assetsAbs:
            return assetsDir
    except ValueError:
        return assetsDir
    return baseDir


def getWidgetValue(widget: QtWidgets.QWidget) -> WidgetValue:
    if isinstance(widget, NodePathEditor):
        return widget.getValue()
    if isinstance(widget, ColourVarEditor):
        return widget.getValue()
    if isinstance(widget, VectorVarEditor):
        return widget.getValue()
    if isinstance(widget, MoveRouteEditor):
        return widget.getValue()
    if isinstance(widget, TransferPosEditor):
        return widget.getValue()
    if isinstance(widget, TypedValueEditor):
        return widget.getValue()
    if isinstance(widget, QtWidgets.QLineEdit):
        return widget.text()
    if isinstance(widget, (QtWidgets.QPlainTextEdit, QtWidgets.QTextEdit)):
        return widget.toPlainText()
    if isinstance(widget, QtWidgets.QCheckBox):
        return widget.isChecked()
    if isinstance(widget, (QtWidgets.QSpinBox, QtWidgets.QDoubleSpinBox)):
        return widget.value()
    if isinstance(widget, QtWidgets.QComboBox):
        return widget.currentText()
    return None


def setEditorWidgetEditable(widget: QtWidgets.QWidget, editable: bool) -> None:
    if isinstance(widget, NodePathEditor):
        widget.setEditable(editable)
        return
    if isinstance(widget, ColourVarEditor):
        widget.setEditable(editable)
        return
    if isinstance(widget, VectorVarEditor):
        widget.setEditable(editable)
        return
    if isinstance(widget, MoveRouteEditor):
        widget.setEditable(editable)
        return
    if isinstance(widget, TransferPosEditor):
        widget.setEditable(editable)
        return
    if isinstance(widget, TypedValueEditor):
        widget.setEditable(editable)
        return
    widget.setEnabled(editable)
    if isinstance(widget, (QtWidgets.QLineEdit, QtWidgets.QPlainTextEdit, QtWidgets.QTextEdit)):
        widget.setReadOnly(not editable)
    if isinstance(widget, QtWidgets.QCheckBox):
        widget.setStyleSheet("" if editable else "color: #888888;")


class NodePlainTextEdit(QtWidgets.QPlainTextEdit):
    EDITING_FINISHED = QtCore.pyqtSignal()

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super(NodePlainTextEdit, self).__init__(parent)
        self.setLineWrapMode(QtWidgets.QPlainTextEdit.WidgetWidth)
        self.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Fixed)
        self.setFixedHeight(60)

    def focusOutEvent(self, event: QtGui.QFocusEvent) -> None:
        super(NodePlainTextEdit, self).focusOutEvent(event)
        self.EDITING_FINISHED.emit()

    def keyPressEvent(self, event: QtGui.QKeyEvent) -> None:
        if event.key() in (QtCore.Qt.Key_Return, QtCore.Qt.Key_Enter) and (
            event.modifiers() & QtCore.Qt.ControlModifier
        ):
            self.EDITING_FINISHED.emit()
            event.accept()
            return
        super(NodePlainTextEdit, self).keyPressEvent(event)


class NodePathEditor(QtWidgets.QWidget):
    VALUE_CHANGED = QtCore.pyqtSignal(str)

    def __init__(self, value: Any = None, baseDir: str = "", parent: Optional[QtWidgets.QWidget] = None) -> None:
        super(NodePathEditor, self).__init__(parent)
        self._baseDir = baseDir
        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)

        self._pathEdit = QtWidgets.QLineEdit(self)
        self._pathEdit.setText("" if value is None else str(value))
        self._pathEdit.setReadOnly(True)
        self._pathEdit.setCursor(QtCore.Qt.ArrowCursor)
        self._pathEdit.setFocusPolicy(QtCore.Qt.NoFocus)
        self._pathEdit.editingFinished.connect(self._emitValueChanged)
        System.SetStyle(self._pathEdit, "nodeInput.qss")
        layout.addWidget(self._pathEdit, 1)

        self._browseBtn = QtWidgets.QPushButton("...", self)
        self._browseBtn.setObjectName("PathBtn")
        self._browseBtn.setFixedWidth(24)
        self._browseBtn.clicked.connect(self._onBrowse)
        layout.addWidget(self._browseBtn, 0)

    def getValue(self) -> str:
        return self._pathEdit.text()

    def setValue(self, value: Any, emit: bool = True) -> None:
        text = "" if value is None else str(value)
        wasBlocked = self._pathEdit.blockSignals(True)
        self._pathEdit.setText(text)
        self._pathEdit.blockSignals(wasBlocked)
        if emit:
            self.VALUE_CHANGED.emit(text)

    def setEditable(self, editable: bool) -> None:
        self.setEnabled(editable)
        self._pathEdit.setReadOnly(True)
        self._browseBtn.setEnabled(editable)

    def _getBaseDir(self) -> str:
        baseDir = self._baseDir
        if not os.path.isdir(baseDir):
            assetsDir = os.path.join(EditorStatus.PROJ_PATH, "Assets")
            baseDir = assetsDir if os.path.isdir(assetsDir) else EditorStatus.PROJ_PATH
        return baseDir

    def _onBrowse(self) -> None:
        baseDir = self._getBaseDir()
        dlg = FileSelectorDialog(getIndependentDialogParent(self), baseDir, FileSelectorDialog.allFilesFilter(star=True))
        filePath = dlg.execSelect()
        if not filePath:
            return
        try:
            relPath = os.path.relpath(filePath, baseDir)
        except ValueError:
            relPath = filePath
        self.setValue(relPath.replace("\\", "/"))

    def _emitValueChanged(self) -> None:
        self.VALUE_CHANGED.emit(self.getValue())


class NodePathWidget(NodeBaseWidget):
    def __init__(
        self,
        parent: Optional[QtWidgets.QWidget] = None,
        name: str = "",
        label: str = "",
        value: Any = None,
        baseDir: str = "",
    ) -> None:
        super(NodePathWidget, self).__init__(parent)
        self.set_name(name)
        self.set_label(label)
        editor = NodePathEditor(value, baseDir)
        self.set_custom_widget(editor)
        editor.VALUE_CHANGED.connect(self.on_value_changed)

    def get_value(self) -> str:
        editor = self.get_custom_widget()
        if not isinstance(editor, NodePathEditor):
            return ""
        return editor.getValue()

    def set_value(self, value: WidgetValue) -> None:
        editor = self.get_custom_widget()
        if not isinstance(editor, NodePathEditor):
            return
        editor.setValue(value, emit=False)


class NodeMultiLineTextWidget(NodeBaseWidget):
    def __init__(
        self, parent: Optional[QtWidgets.QWidget] = None, name: str = "", label: str = "", text: str = ""
    ) -> None:
        super(NodeMultiLineTextWidget, self).__init__(parent)
        self.set_name(name)
        self.set_label(label)
        editor = NodePlainTextEdit()
        editor.setPlainText(text or "")
        self.set_custom_widget(editor)
        editor.textChanged.connect(self.on_value_changed)

    def get_value(self) -> str:
        editor = self.get_custom_widget()
        if not isinstance(editor, NodePlainTextEdit):
            return ""
        return editor.toPlainText()

    def set_value(self, value: WidgetValue) -> None:
        editor = self.get_custom_widget()
        if not isinstance(editor, NodePlainTextEdit):
            return
        wasBlocked = editor.blockSignals(True)
        editor.setPlainText("" if value is None else str(value))
        editor.blockSignals(wasBlocked)


class NodeColourWidget(NodeBaseWidget):
    def __init__(
        self, parent: Optional[QtWidgets.QWidget] = None, name: str = "", label: str = "", value: Any = None
    ) -> None:
        super(NodeColourWidget, self).__init__(parent)
        self.set_name(name)
        self.set_label(label)
        editor = ColourVarEditor(value)
        self.set_custom_widget(editor)
        editor.VALUE_CHANGED.connect(self.on_value_changed)

    def get_value(self) -> WidgetValue:
        editor = self.get_custom_widget()
        if not isinstance(editor, ColourVarEditor):
            return None
        return editor.getValue()

    def set_value(self, value: WidgetValue) -> None:
        editor = self.get_custom_widget()
        if not isinstance(editor, ColourVarEditor):
            return
        editor.setValue(value, emit=False)


class NodeVectorWidget(NodeBaseWidget):
    def __init__(
        self,
        parent: Optional[QtWidgets.QWidget] = None,
        name: str = "",
        label: str = "",
        varType: str = "",
        value: Any = None,
    ) -> None:
        super(NodeVectorWidget, self).__init__(parent)
        self.set_name(name)
        self.set_label(label)
        editor = VectorVarEditor(varType, value)
        self.set_custom_widget(editor)
        editor.VALUE_CHANGED.connect(self.on_value_changed)

    def get_value(self) -> WidgetValue:
        editor = self.get_custom_widget()
        if not isinstance(editor, VectorVarEditor):
            return None
        return editor.getValue()

    def set_value(self, value: WidgetValue) -> None:
        editor = self.get_custom_widget()
        if not isinstance(editor, VectorVarEditor):
            return
        editor.setValue(value, emit=False)


class NodeMoveRouteWidget(NodeBaseWidget):
    def __init__(
        self, parent: Optional[QtWidgets.QWidget] = None, name: str = "", label: str = "", value: Any = None
    ) -> None:
        super(NodeMoveRouteWidget, self).__init__(parent)
        self.set_name(name)
        self.set_label(label)
        editor = MoveRouteEditor(value)
        self.set_custom_widget(editor)
        editor.VALUE_CHANGED.connect(self.on_value_changed)

    def get_value(self) -> WidgetValue:
        editor = self.get_custom_widget()
        if not isinstance(editor, MoveRouteEditor):
            return []
        return editor.getValue()

    def set_value(self, value: WidgetValue) -> None:
        editor = self.get_custom_widget()
        if not isinstance(editor, MoveRouteEditor):
            return
        editor.setValue(value, emit=False)


class NodeTransferWidget(NodeBaseWidget):
    def __init__(
        self,
        parent: Optional[QtWidgets.QWidget] = None,
        name: str = "",
        label: str = "",
        editor: Optional[TransferPosEditor] = None,
    ) -> None:
        super(NodeTransferWidget, self).__init__(parent)
        self.set_name(name)
        self.set_label(label)
        if editor is None:
            editor = TransferPosEditor()
        self.set_custom_widget(editor)
        editor.VALUE_CHANGED.connect(self.on_value_changed)

    def get_value(self) -> WidgetValue:
        editor = self.get_custom_widget()
        if not isinstance(editor, TransferPosEditor):
            return None
        return editor.getValue()

    def set_value(self, value: WidgetValue) -> None:
        editor = self.get_custom_widget()
        if not isinstance(editor, TransferPosEditor):
            return
        editor.setValue(value, emit=False)


class NodeTypedValueWidget(NodeBaseWidget):
    def __init__(
        self,
        parent: Optional[QtWidgets.QWidget] = None,
        name: str = "",
        label: str = "",
        value: Any = None,
        valueType: Any = None,
    ) -> None:
        super(NodeTypedValueWidget, self).__init__(parent)
        self.set_name(name)
        self.set_label(label)
        editor = TypedValueEditor(value, valueType)
        self.set_custom_widget(editor)
        editor.VALUE_CHANGED.connect(self.on_value_changed)

    def get_value(self) -> WidgetValue:
        editor = self.get_custom_widget()
        if not isinstance(editor, TypedValueEditor):
            return None
        return editor.getValue()

    def set_value(self, value: WidgetValue) -> None:
        editor = self.get_custom_widget()
        if not isinstance(editor, TypedValueEditor):
            return
        editor.setValue(value, emit=False)


def MakeInit(currNode: GraphNode) -> Callable[[BaseNode], None]:
    def subClassInit(self: BaseNode) -> None:
        super(self.__class__, self).__init__()
        self._port_types = {}
        self._widgetNameByPort = {}
        usedNames = set()
        latents = getLatents(currNode.nodeFunction)
        execSplits = getExecSplits(currNode.nodeFunction)
        returnTypes = getReturnTypes(currNode.nodeFunction)
        if latents:
            self.add_input("in")
            self._port_types["in"] = "Exec"
            for key in latents:
                self.add_output(f"out_{key}")
                self._port_types[f"out_{key}"] = "Exec"
        elif execSplits:
            self.add_input("in")
            self._port_types["in"] = "Exec"
            for key in execSplits:
                self.add_output(f"out_{key}")
                self._port_types[f"out_{key}"] = "Exec"

        if returnTypes:
            for name, r_type in returnTypes.items():
                self.add_output(name)
                self._port_types[name] = "Params"

        paramList = currNode.getParamList()
        keys = list(paramList.keys())
        has_invalid = False

        meta = getattr(currNode.nodeFunction, "_meta", {})
        dropBox = meta.get("DropBox", [])
        varTypes = getMetaVarTypes(meta)
        pathVarMap = getMetaPathVars(meta)
        moveRouteVars = getMetaMoveRouteVars(meta)
        transferVars = getMetaTransferVars(meta)
        self._relyMap = normaliseRelyMap(meta.get("Rely"))
        self.META = meta

        _pendingTransferWires: List[Tuple[TransferPosEditor, str]] = []

        for i, name in enumerate(keys):
            widgetName = MakeSafeNodePropertyName(name, usedNames)
            self._widgetNameByPort[name] = widgetName
            init_val = getNodeParamValue(currNode, i, name)
            self.add_input(name, multi_input=False)
            self._port_types[name] = "Params"

            param_type = paramList[name]
            varType = varTypes.get(name, "")
            isPath = name in pathVarMap
            isMoveRoute = name in moveRouteVars
            isTransfer = name in transferVars

            type_str = formatParamTypeName(param_type)
            if isTransfer:
                type_str = "TransferPos"
            elif isMoveRoute:
                type_str = "MoveRoute"
            elif varType == "ColourVar":
                type_str = "ColourVar"
            elif isVectorVarType(varType):
                type_str = normaliseVectorVarType(varType)
            elif isPath:
                type_str = "Path"
            display_label = f"{name} ({type_str})"

            containerEditorType = getSimpleContainerEditorType(param_type)

            if isTransfer:
                editor = TransferPosEditor(value=init_val)
                nodeWidget = NodeTransferWidget(
                    self.view, name=widgetName, label=display_label, editor=editor
                )
                self.add_custom_widget(nodeWidget)
                _pendingTransferWires.append((editor, transferVars[name]))
            elif isMoveRoute:
                nodeWidget = NodeMoveRouteWidget(
                    self.view, name=widgetName, label=display_label, value=init_val
                )
                self.add_custom_widget(nodeWidget)
            elif varType == "ColourVar":
                nodeWidget = NodeColourWidget(
                    self.view, name=widgetName, label=display_label, value=init_val
                )
                self.add_custom_widget(nodeWidget)
            elif isVectorVarType(varType):
                nodeWidget = NodeVectorWidget(
                    self.view, name=widgetName, label=display_label, varType=varType, value=init_val
                )
                self.add_custom_widget(nodeWidget)
            elif isPath:
                nodeWidget = NodePathWidget(
                    self.view,
                    name=widgetName,
                    label=display_label,
                    value=init_val,
                    baseDir=getPathVarBaseDir(pathVarMap, name),
                )
                self.add_custom_widget(nodeWidget)
            elif name in dropBox:
                items = dropBox[name]
                if isinstance(items, (list, tuple)):
                    items = [str(x) for x in items]
                else:
                    items = []
                self.add_combo_menu(name=widgetName, label=display_label, items=items)
                w = self.get_widget(widgetName)
                if w:
                    le = w.get_custom_widget()
                    if isinstance(le, QtWidgets.QComboBox):
                        le.setCurrentText(str(init_val))
                    System.SetStyle(le, "nodeInput.qss")
            elif param_type is bool or param_type == "bool":
                boolVal = toBool(init_val)
                self.add_checkbox(
                    name=widgetName,
                    label=display_label,
                    text="",
                    state=boolVal if boolVal is not None else bool(init_val),
                )
            elif param_type is int or param_type == "int":
                try:
                    val = int(init_val)
                except:
                    val = 0
                self.add_spinbox(
                    name=widgetName,
                    label=display_label,
                    value=val,
                    min_value=-2147483648,
                    max_value=2147483647,
                    double=False,
                )
                w = self.get_widget(widgetName)
                if w:
                    le = w.get_custom_widget()
                    System.SetStyle(le, "nodeInput.qss")
            elif param_type is float or param_type == "float":
                try:
                    val = float(init_val)
                except:
                    val = 0.0
                self.add_spinbox(
                    name=widgetName,
                    label=display_label,
                    value=val,
                    min_value=-999999999.0,
                    max_value=999999999.0,
                    double=True,
                )
                w = self.get_widget(widgetName)
                if w:
                    le = w.get_custom_widget()
                    System.SetStyle(le, "nodeInput.qss")
            elif containerEditorType is not None:
                parsedVal = parseParamInitialValue(init_val, param_type)
                nodeWidget = NodeTypedValueWidget(
                    self.view,
                    name=widgetName,
                    label=display_label,
                    value=parsedVal,
                    valueType=containerEditorType,
                )
                self.add_custom_widget(nodeWidget)
                w = self.get_widget(widgetName)
                if w:
                    le = w.get_custom_widget()
                    System.SetStyle(le, "nodeInput.qss")
            else:
                nodeWidget = NodeMultiLineTextWidget(
                    self.view, name=widgetName, label=display_label, text=str(init_val)
                )
                self.add_custom_widget(nodeWidget)
                w = self.get_widget(widgetName)
                if w:
                    le = w.get_custom_widget()
                    System.SetStyle(le, "nodeInput.qss")

        for _transferEditor, _mapParamName in _pendingTransferWires:
            _selfRef = self
            def _makeTransferGetter(selfRef: Any, mParam: str) -> Callable[[], str]:
                def _getter() -> str:
                    mapWidgetName = getattr(selfRef, "_widgetNameByPort", {}).get(mParam, mParam)
                    w = selfRef.get_widget(mapWidgetName)
                    if not w:
                        return ""
                    cw = w.get_custom_widget()
                    v = getWidgetValue(cw)
                    return str(v) if v is not None else ""
                return _getter
            _transferEditor.setMapKeyGetter(_makeTransferGetter(_selfRef, _mapParamName))

        self._string_mode = has_invalid
        if has_invalid:
            for i, name in enumerate(keys):
                widgetName = self._widgetNameByPort.get(name, name)
                w = self.get_widget(widgetName)
                if w:
                    cw = w.get_custom_widget()
                    if isinstance(cw, QtWidgets.QLineEdit):
                        cw.setValidator(None)

    return subClassInit


class NodePanel(QtWidgets.QWidget):
    _COPY_BUFFER: Optional[List[Any]] = None
    MODIFIED = QtCore.pyqtSignal()

    def __init__(
        self,
        parent: QtWidgets.QWidget,
        graph: Graph,  # type: ignore
        key: str,
        name: str,
        refreshCallable: Callable[[str, Dict[str, Any]], None],
    ):
        super(NodePanel, self).__init__(parent)
        self._isLoading = True
        self._parent = parent
        self.setWindowTitle("Node Panel")
        self.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self.graph = NodeGraph(viewer=CustomViewer())
        self.graphWidget = self.graph.widget
        self.graphWidget.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        self._isSpacePressed = False
        self._isPanEmulationActive = False
        self._trackpadZoomWheelBlockUntilMs = 0
        viewer = self.graph.viewer()
        viewer.setFocusPolicy(QtCore.Qt.StrongFocus)
        viewer.installEventFilter(self)
        viewer.viewport().installEventFilter(self)
        self.nodeGraph = graph
        self.key = key
        self.name = name
        self._refreshCallable = refreshCallable
        self.classDict: Dict[str, type] = {}
        self.nodes: List[BaseNode] = []
        self.defaultNodes: List[BaseNode] = []
        self._pending_conn = None
        self._connectionPickerPopup: Optional[FunctionPickerPopup] = None
        self._functionPickerPopup: Optional[FunctionPickerPopup] = None
        self._createNodeScenePos: Optional[QtCore.QPointF] = None
        self.paramChangeTimerByWidget: Dict[int, QtCore.QTimer] = {}
        self._setupLayout()
        self._registerNodes()
        self._createNodes()
        self._setupSignals()
        self._createLinks()
        self._isLoading = False

    def hideEvent(self, event: QtGui.QHideEvent) -> None:
        self._closeFunctionPickerPopups()
        super(NodePanel, self).hideEvent(event)

    def setName(self, name: str) -> None:
        oldName = self.key
        if oldName == name:
            return
        for attrName in (
            "dataNodes",
            "nodes",
            "links",
            "startNodes",
            "nodeRely",
            "nodeNexts",
            "_executionLocked",
            "_latentPendingCount",
        ):
            data = getattr(self.nodeGraph, attrName, None)
            if isinstance(data, dict) and oldName in data and name not in data:
                data[name] = data.pop(oldName)
        self.key = name

    def _setupLayout(self):
        main_layout = QtWidgets.QVBoxLayout(self)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)
        main_layout.addWidget(self.graphWidget)

    def _registerNodes(self):
        # Register default parameter node type (once per panel)
        if DEFAULT_PARAM_NODE_IDENTIFIER not in self.classDict:

            def _defaultParamInit(self: BaseNode) -> None:
                super(self.__class__, self).__init__()
                self.add_output("value")
                self._port_types = {"value": "Params"}

            cls = type("DefaultParamNode", (BaseNode,), {"__init__": _defaultParamInit})
            cls.__identifier__ = DEFAULT_PARAM_NODE_IDENTIFIER
            cls.NODE_NAME = "EventParam"
            self.classDict[DEFAULT_PARAM_NODE_IDENTIFIER] = cls
            self.graph.register_node(cls)

        for node in self.nodeGraph.nodes[self.key]:
            nodeFunctionName = node.functionName
            if not nodeFunctionName in self.classDict:
                self.classDict[nodeFunctionName] = type("Class", (BaseNode,), {"__init__": MakeInit(node)})
                self.classDict[nodeFunctionName].__identifier__ = nodeFunctionName
                self.classDict[nodeFunctionName].NODE_NAME = nodeFunctionName
                self.graph.register_node(self.classDict[nodeFunctionName])

    def handleMeta(self, obj: BaseNode, defaultName: str, metaRefer: Dict[str, str]) -> None:
        rawMeta = getattr(obj, "META", {})
        if not isinstance(rawMeta, dict):
            rawMeta = {}
        meta: Dict[str, Any] = rawMeta
        obj.set_name(defaultName)
        for metaKey, metaValue in meta.items():
            try:
                metaValue = eval(metaValue)
            except:
                pass
            if metaKey == "DisplayName":
                obj.set_name(metaValue)
            if metaKey == "DisplayDesc":
                obj.view.setToolTip(metaRefer["originalName"] + "\n\n" + metaValue)

    def resolveWidgetName(self, node: BaseNode, portName: str) -> str:
        widgetNameByPort = getattr(node, "_widgetNameByPort", None)
        if isinstance(widgetNameByPort, dict):
            return widgetNameByPort.get(portName, portName)
        return portName

    def _createDefaultParamNodes(self) -> None:
        """Create visual nodes for event parameters (non-deletable, not stored in nodes[]).

        These nodes show the parameter name/type and have a single Params output port
        that other nodes can connect to. They are referenced as ``"default_0"`` etc. in links.
        """
        self.defaultNodes = []
        eventParams = self.nodeGraph.eventParams.get(self.key, [])
        if not eventParams:
            return

        parentClass = self.nodeGraph.parentClass
        method = getattr(parentClass, self.key, None) if parentClass else None
        paramTypes: Dict[str, Any] = {}
        if method and callable(method):
            try:
                sig = inspect.signature(method)
                for pname, pobj in sig.parameters.items():
                    if pname == "self":
                        continue
                    ann = pobj.annotation
                    if ann == inspect.Parameter.empty:
                        ann = "Any"
                    paramTypes[pname] = ann
            except (ValueError, TypeError):
                pass

        for i, paramName in enumerate(eventParams):
            paramType = paramTypes.get(paramName, "Any")
            typeName = paramType.__name__ if hasattr(paramType, "__name__") else str(paramType)
            displayName = f"{paramName} ({typeName})"

            pos = [0.0, i * 64.0]

            nodeInst = self.graph.create_node(
                DEFAULT_PARAM_NODE_TYPE,
                name=displayName,
                pos=pos,
                push_undo=False,
            )
            nodeInst.set_color(60, 100, 50)
            nodeInst.view.setToolTip(f"Event parameter: {paramName} ({typeName})")
            self.defaultNodes.append(nodeInst)

    def _createNodes(self):
        self._createDefaultParamNodes()
        start_idx = self._getStartIndex()
        for i, node in enumerate(self.nodeGraph.nodes[self.key]):
            nodeInst = self.graph.create_node(f"{node.functionName}.Class", pos=node.position, push_undo=False)

            metaRefer: Dict[str, str] = {}
            metaRefer["originalName"] = node.functionName
            parts = metaRefer["originalName"].split(".")
            if len(parts) > 1:
                displayName = parts[-1]
            else:
                displayName = f"(parent){metaRefer['originalName']}"

            nodeInst.view.setToolTip(metaRefer["originalName"])
            self.handleMeta(nodeInst, displayName, metaRefer)

            self.nodes.append(nodeInst)
            if start_idx is not None and i == start_idx:
                s_item = QtWidgets.QGraphicsSimpleTextItem("S", nodeInst.view)
                s_item.setBrush(QtGui.QBrush(QtGui.QColor(255, 215, 0)))
                f = QtGui.QFont()
                f.setBold(True)
                f.setPointSize(14)
                s_item.setFont(f)
                s_item.setZValue(nodeInst.view.zValue() + 2)
                s_item.setPos(6, 4)
            paramList = node.getParamList()
            keys = list(paramList.keys())
            paramIndex = 0
            for name in keys:
                widgetName = self.resolveWidgetName(nodeInst, name)
                w = nodeInst.get_widget(widgetName)
                if w:
                    le = w.get_custom_widget()
                    val = getNodeParamValue(node, paramIndex, name)
                    if isinstance(le, QtWidgets.QLineEdit):
                        le.setText(str(val))
                        le.editingFinished.connect(
                            lambda n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                    elif isinstance(le, QtWidgets.QPlainTextEdit):
                        wasBlocked = le.blockSignals(True)
                        le.setPlainText(str(val))
                        le.blockSignals(wasBlocked)
                        if isinstance(le, NodePlainTextEdit):
                            le.EDITING_FINISHED.connect(
                                lambda n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                            )
                        le.textChanged.connect(
                            lambda n=i, p=paramIndex, widget=le: self.scheduleParamChanged(n, p, widget)
                        )
                    elif isinstance(le, QtWidgets.QCheckBox):
                        boolVal = toBool(val)
                        le.setChecked(boolVal if boolVal is not None else bool(val))
                        le.toggled.connect(
                            lambda checked, n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                    elif isinstance(le, QtWidgets.QSpinBox):
                        try:
                            le.setValue(int(val))
                        except:
                            le.setValue(0)
                        le.valueChanged.connect(
                            lambda val, n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                    elif isinstance(le, QtWidgets.QDoubleSpinBox):
                        try:
                            le.setValue(float(val))
                        except:
                            le.setValue(0.0)
                        le.valueChanged.connect(
                            lambda val, n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                    elif isinstance(le, QtWidgets.QComboBox):
                        le.setCurrentText(str(val))
                        le.currentIndexChanged.connect(
                            lambda idx, n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                    elif isinstance(le, ColourVarEditor):
                        le.setValue(val, emit=False)
                        le.VALUE_CHANGED.connect(
                            lambda _, n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                    elif isinstance(le, VectorVarEditor):
                        le.setValue(val, emit=False)
                        le.VALUE_CHANGED.connect(
                            lambda _, n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                    elif isinstance(le, MoveRouteEditor):
                        le.setValue(val, emit=False)
                        le.VALUE_CHANGED.connect(
                            lambda _, n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                    elif isinstance(le, TransferPosEditor):
                        le.setValue(val, emit=False)
                        le.VALUE_CHANGED.connect(
                            lambda _, n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                        _tvars = getMetaTransferVars(getattr(node.nodeFunction, "_meta", {}))
                        _mapPN = _tvars.get(name, "")
                        if _mapPN and _mapPN in paramList:
                            _mapPI = list(paramList.keys()).index(_mapPN)

                            def _makeMapKeySetter(
                                _ni: int,
                                _mpi: int,
                                _nInst: Any,
                                _mPN: str,
                                _panelSelf: Any,
                            ) -> Callable[[str], None]:
                                def _onMapKeyChanged(_mapKey: str) -> None:
                                    _mapWN = _panelSelf.resolveWidgetName(_nInst, _mPN)
                                    _mw = _nInst.get_widget(_mapWN)
                                    if not _mw:
                                        return
                                    _mcw = _mw.get_custom_widget()
                                    if isinstance(_mcw, NodePlainTextEdit):
                                        wasBlocked = _mcw.blockSignals(True)
                                        _mcw.setPlainText(_mapKey)
                                        _mcw.blockSignals(wasBlocked)
                                    elif isinstance(_mcw, QtWidgets.QPlainTextEdit):
                                        wasBlocked = _mcw.blockSignals(True)
                                        _mcw.setPlainText(_mapKey)
                                        _mcw.blockSignals(wasBlocked)
                                    elif isinstance(_mcw, QtWidgets.QLineEdit):
                                        wasBlocked = _mcw.blockSignals(True)
                                        _mcw.setText(_mapKey)
                                        _mcw.blockSignals(wasBlocked)
                                    else:
                                        return
                                    _panelSelf._onParamChanged(_ni, _mpi, _mcw)
                                return _onMapKeyChanged

                            le.MAP_KEY_CHANGED.connect(
                                _makeMapKeySetter(i, _mapPI, nodeInst, _mapPN, self)
                            )
                    elif isinstance(le, NodePathEditor):
                        le.setValue(val, emit=False)
                        le.VALUE_CHANGED.connect(
                            lambda _, n=i, p=paramIndex, widget=le: self._onParamChanged(n, p, widget)
                        )
                    elif isinstance(le, TypedValueEditor):
                        le.setValue(parseParamInitialValue(val, paramList[name]), emit=False)
                        le.VALUE_CHANGED.connect(
                            lambda _, n=i, p=paramIndex, widget=le: self.scheduleParamChanged(n, p, widget)
                        )
                paramIndex += 1
            self._applyNodeRely(i)

    def _setNodeWidgetEditable(self, nodeWidget: NodeBaseWidget, editable: bool) -> None:
        try:
            nodeWidget.setEnabled(editable)
        except AttributeError:
            pass
        customWidget = nodeWidget.get_custom_widget()
        if isinstance(customWidget, QtWidgets.QWidget):
            setEditorWidgetEditable(customWidget, editable)

    def _isNodeParamConnected(self, nodeIndex: int, nodeInst: BaseNode, name: str, paramIndex: int) -> bool:
        nodeRely = self.nodeGraph.nodeRely.get(self.key, {})
        if isinstance(nodeRely, dict):
            depMap = nodeRely.get(nodeIndex, {})
            if isinstance(depMap, dict) and paramIndex in depMap:
                return True
        port = nodeInst.get_input(name)
        if port is None:
            return False
        try:
            return bool(port.connected_ports())
        except AttributeError:
            return False

    def _getNodeParamValues(self, nodeIndex: int, nodeInst: BaseNode, node: GraphNode) -> Dict[str, Any]:
        values: Dict[str, Any] = {}
        paramList = node.getParamList()
        for paramIndex, name in enumerate(paramList.keys()):
            if self._isNodeParamConnected(nodeIndex, nodeInst, name, paramIndex):
                values[name] = CONNECTED_PARAM_VALUE
                continue
            widgetName = self.resolveWidgetName(nodeInst, name)
            nodeWidget = nodeInst.get_widget(widgetName)
            customWidget = nodeWidget.get_custom_widget() if isinstance(nodeWidget, NodeBaseWidget) else None
            if isinstance(customWidget, QtWidgets.QWidget):
                values[name] = getWidgetValue(customWidget)
            elif paramIndex < len(node.params):
                values[name] = node.params[paramIndex]
        return values

    def _applyNodeRely(self, nodeIndex: int) -> None:
        if not (0 <= nodeIndex < len(self.nodes)):
            return
        if self.key not in self.nodeGraph.nodes or not (0 <= nodeIndex < len(self.nodeGraph.nodes[self.key])):
            return
        nodeInst = self.nodes[nodeIndex]
        node = self.nodeGraph.nodes[self.key][nodeIndex]
        meta = getattr(node.nodeFunction, "_meta", {})
        relyMap = normaliseRelyMap(meta.get("Rely")) if isinstance(meta, dict) else {}
        if not relyMap:
            return
        values = self._getNodeParamValues(nodeIndex, nodeInst, node)
        for name in relyMap.keys():
            widgetName = self.resolveWidgetName(nodeInst, name)
            nodeWidget = nodeInst.get_widget(widgetName)
            if not nodeWidget:
                continue
            relyEditable = isRelyEditable(name, relyMap, values)
            if isinstance(nodeWidget, NodeBaseWidget):
                self._setNodeWidgetEditable(nodeWidget, relyEditable)
            tip = ""
            if not relyEditable:
                condition = getRelyConditionDisplay(name, relyMap)
                if condition:
                    source, value = condition
                    tip = ELOC("META_RELY_TOOLTIP").format(source=source, value=value)
            try:
                nodeWidget.setToolTip(tip)
            except AttributeError:
                pass
            customWidget = nodeWidget.get_custom_widget() if isinstance(nodeWidget, NodeBaseWidget) else None
            if isinstance(customWidget, QtWidgets.QWidget):
                customWidget.setToolTip(tip)

    def _applyAllNodeRely(self) -> None:
        for nodeIndex in range(len(self.nodes)):
            self._applyNodeRely(nodeIndex)

    def scheduleParamChanged(self, nodeIndex: int, paramIndex: int, widget: QtWidgets.QWidget) -> None:
        key = id(widget)
        timer = self.paramChangeTimerByWidget.get(key)
        if not timer:
            timer = QtCore.QTimer(self)
            timer.setSingleShot(True)
            timer.timeout.connect(lambda n=nodeIndex, p=paramIndex, w=widget: self._onParamChanged(n, p, w))
            self.paramChangeTimerByWidget[key] = timer
        timer.start(300)

    def _getStartIndex(self) -> Optional[int]:
        start = self.nodeGraph.startNodes.get(self.key)
        if isinstance(start, int):
            if 0 <= start < len(self.nodeGraph.nodes.get(self.key, [])):
                return start
            return None
        data_list = self.nodeGraph.dataNodes.get(self.key, [])
        if isinstance(data_list, list) and start in data_list:
            return data_list.index(start)
        return None

    def organizeLayout(self) -> None:
        if not self.nodes:
            return
        positions = self._computeOrganizedPositions()
        if not positions:
            return
        self._applyOrganizedPositions(positions)
        self.graph.clear_selection()
        for nodeInst in self.nodes + self.defaultNodes:
            nodeInst.set_selected(True)
        QtCore.QTimer.singleShot(0, self.graph.fit_to_selection)
        self.graph.clear_selection()

    def _computeOrganizedPositions(self) -> Dict[Union[int, str], Tuple[float, float]]:
        nodeRely = self.nodeGraph.nodeRely.get(self.key, {})
        if not isinstance(nodeRely, dict):
            nodeRely = {}
        return computeGraphLayoutPositions(
            len(self.nodes),
            self.nodeGraph.links.get(self.key, []),
            nodeRely,
            self._getStartIndex(),
            len(self.defaultNodes),
        )

    def _applyOrganizedPositions(self, positions: Dict[Union[int, str], Tuple[float, float]]) -> None:
        from NodeGraph import EditorDataNode

        for idx, nodeInst in enumerate(self.nodes):
            if idx not in positions:
                continue
            x, y = positions[idx]
            nodeInst.set_pos(x, y)
            graphNode = self.nodeGraph.nodes[self.key][idx]
            graphNode.position = [x, y]
            dataNodes = self.nodeGraph.dataNodes.get(self.key, [])
            if 0 <= idx < len(dataNodes):
                dataNode = dataNodes[idx]
                if isinstance(dataNode, EditorDataNode):
                    dataNode.pos = [x, y]

        for idx, nodeInst in enumerate(self.defaultNodes):
            key = f"default_{idx}"
            if key not in positions:
                continue
            x, y = positions[key]
            nodeInst.set_pos(x, y)

        GameData.recordSnapshot()
        self._refreshCallable(self.name, self.nodeGraph.asDict())
        self.MODIFIED.emit()

    def _resolveLinkNodeRef(self, ref: Union[int, str]) -> Tuple[BaseNode, Optional[Any]]:
        """Resolve a link endpoint reference to a visual node and its data node.

        Returns ``(nodeInst, nodeData)``. For default parameter nodes (string refs),
        ``nodeData`` is ``None``.
        """
        if isinstance(ref, str) and ref.startswith("default_"):
            idx = int(ref.split("_")[1])
            if 0 <= idx < len(self.defaultNodes):
                return self.defaultNodes[idx], None
            raise IndexError(f"Default param node index {idx} out of range")
        if 0 <= ref < len(self.nodes):
            return self.nodes[ref], self.nodeGraph.nodes[self.key][ref]
        raise IndexError(f"Node index {ref} out of range")

    def _createLinks(self):
        for link in self.nodeGraph.links[self.key]:
            left = link["left"]
            right = link["right"]
            leftOutPin = link["leftOutPin"]
            rightInPin = link["rightInPin"]

            leftNodeInst, leftNodeData = self._resolveLinkNodeRef(left)
            rightNodeInst, rightNodeData = self._resolveLinkNodeRef(right)

            linkType = link["linkType"]
            if linkType == "Exec":
                if leftNodeData is None:
                    continue  # Default nodes have no exec outputs
                latents = getLatents(leftNodeData.nodeFunction)
                execSplits = getExecSplits(leftNodeData.nodeFunction)
                if latents:
                    keys = list(latents.keys())
                    if leftOutPin < len(keys):
                        out_name = f"out_{keys[leftOutPin]}"
                        left_port = leftNodeInst.get_output(out_name)
                        right_port = rightNodeInst.get_input("in")
                        if left_port and right_port:
                            left_port.connect_to(right_port)
                elif execSplits:
                    splits = list(execSplits.keys())
                    if leftOutPin < len(splits):
                        out_name = f"out_{splits[leftOutPin]}"
                        left_port = leftNodeInst.get_output(out_name)
                        right_port = rightNodeInst.get_input("in")
                        if left_port and right_port:
                            left_port.connect_to(right_port)
            elif linkType == "Params":
                if leftNodeData is None:
                    # Default parameter node — single "value" output
                    out_name = "value"
                else:
                    returnTypes = getReturnTypes(leftNodeData.nodeFunction)
                    if returnTypes:
                        return_names = list(returnTypes.keys())
                        if leftOutPin < len(return_names):
                            out_name = return_names[leftOutPin]
                        else:
                            continue
                    else:
                        continue
                left_port = leftNodeInst.get_output(out_name)
                if left_port is None:
                    continue
                if rightNodeData is not None:
                    param_names = [k for k in rightNodeData.getParamList().keys()]
                    if rightInPin < len(param_names):
                        in_name = param_names[rightInPin]
                        right_port = rightNodeInst.get_input(in_name)
                        if left_port and right_port:
                            left_port.connect_to(right_port)
                            widgetName = self.resolveWidgetName(rightNodeInst, in_name)
                            if rightNodeInst.get_widget(widgetName):
                                rightNodeInst.hide_widget(widgetName, push_undo=False)
            else:
                raise ValueError(f"Unknown link type: {linkType}")

    def _setupSignals(self):
        self.graph.port_connected.connect(self.onPortConnected)
        self.graph.port_disconnected.connect(self.onPortDisconnected)
        self.graph.viewer().moved_nodes.connect(self.onNodesMoved)
        self.graph.viewer().LIVE_CONNECTION_PROMPT.connect(self._onLiveConnectionPrompt)

        QtWidgets.QShortcut(QtGui.QKeySequence.New, self, self._onCreate, context=QtCore.Qt.WidgetWithChildrenShortcut)
        QtWidgets.QShortcut(QtGui.QKeySequence.Copy, self, self._onCopy, context=QtCore.Qt.WidgetWithChildrenShortcut)
        QtWidgets.QShortcut(QtGui.QKeySequence.Paste, self, self._onPaste, context=QtCore.Qt.WidgetWithChildrenShortcut)
        QtWidgets.QShortcut(
            QtGui.QKeySequence.Delete, self, self._onDelete, context=QtCore.Qt.WidgetWithChildrenShortcut
        )
        QtWidgets.QShortcut(QtGui.QKeySequence.Undo, self, self._onUndo, context=QtCore.Qt.WidgetWithChildrenShortcut)
        QtWidgets.QShortcut(QtGui.QKeySequence.Redo, self, self._onRedo, context=QtCore.Qt.WidgetWithChildrenShortcut)

    def _closeFunctionPickerPopup(self, attrName: str) -> None:
        popup = getattr(self, attrName, None)
        if not isinstance(popup, FunctionPickerPopup):
            setattr(self, attrName, None)
            return
        try:
            popup.FUNCTION_SELECTED.disconnect()
        except (TypeError, RuntimeError):
            pass
        try:
            popup.destroyed.disconnect()
        except (TypeError, RuntimeError):
            pass
        try:
            popup.close()
        except RuntimeError:
            pass
        setattr(self, attrName, None)

    def _closeFunctionPickerPopups(self) -> None:
        self._closeFunctionPickerPopup("_connectionPickerPopup")
        self._closeFunctionPickerPopup("_functionPickerPopup")
        self._pending_conn = None
        try:
            self.graph.viewer().end_live_connection()
        except RuntimeError:
            pass

    def _getNodeRef(self, nodeInst: BaseNode) -> Union[int, str]:
        """Get the link reference for a visual node instance.

        Returns an ``int`` index for regular nodes, or ``"default_N"`` for default param nodes.
        """
        if nodeInst in self.defaultNodes:
            idx = self.defaultNodes.index(nodeInst)
            return f"default_{idx}"
        return self.nodes.index(nodeInst)

    def onPortConnected(self, portIn, portOut):
        node_in = portIn.node()
        node_out = portOut.node()

        type_in = getattr(node_in, "_port_types", {}).get(portIn.name())
        type_out = getattr(node_out, "_port_types", {}).get(portOut.name())

        if type_in and type_out and type_in != type_out:
            portIn.disconnect_from(portOut)
            return

        if not self._isLoading:
            left = self._getNodeRef(node_out)
            right = self._getNodeRef(node_in)
            leftNodeData = None if isinstance(left, str) else self.nodeGraph.nodes[self.key][left]
            rightNodeData = None if isinstance(right, str) else self.nodeGraph.nodes[self.key][right]

            linkType = type_out
            leftOutPin = -1
            rightInPin = -1

            if linkType == "Exec":
                if leftNodeData is None:
                    return  # Default nodes have no exec outputs
                latents = getLatents(leftNodeData.nodeFunction)
                execSplits = getExecSplits(leftNodeData.nodeFunction)
                if latents:
                    keys = list(latents.keys())
                    out_name = portOut.name()
                    if out_name.startswith("out_"):
                        key = out_name[4:]
                        if key in keys:
                            leftOutPin = keys.index(key)
                elif execSplits:
                    keys = list(execSplits.keys())
                    out_name = portOut.name()
                    if out_name.startswith("out_"):
                        key = out_name[4:]
                        if key in keys:
                            leftOutPin = keys.index(key)
                rightInPin = 0
            elif linkType == "Params":
                if leftNodeData is None:
                    # Default parameter node — output is always "value", pin 0
                    leftOutPin = 0
                else:
                    returnTypes = getReturnTypes(leftNodeData.nodeFunction)
                    if returnTypes:
                        keys = list(returnTypes.keys())
                        if portOut.name() in keys:
                            leftOutPin = keys.index(portOut.name())

                if rightNodeData is not None:
                    param_keys = [k for k in rightNodeData.getParamList().keys()]
                    if portIn.name() in param_keys:
                        rightInPin = param_keys.index(portIn.name())

            if leftOutPin != -1 and rightInPin != -1:
                link_data = {
                    "left": left,
                    "right": right,
                    "leftOutPin": leftOutPin,
                    "rightInPin": rightInPin,
                    "linkType": linkType,
                }
                if link_data not in self.nodeGraph.links[self.key]:
                    self.nodeGraph.links[self.key].append(link_data)
                    self.nodeGraph.genNodesFromDataNodes()
                    self.nodeGraph.genRelationsFromLinks()
                    GameData.recordSnapshot()
                    self._refreshCallable(self.name, self.nodeGraph.asDict())
                    self.MODIFIED.emit()

        if portIn.type_() == "in":
            node = portIn.node()
            name = portIn.name()
            widgetName = self.resolveWidgetName(node, name)
            if node.get_widget(widgetName):
                node.hide_widget(widgetName, push_undo=False)
            if node in self.nodes:
                self._applyNodeRely(self.nodes.index(node))

    def _onLiveConnectionPrompt(self, start_port_view, scene_pos):
        node_out = self.graph.get_node_by_id(start_port_view.node.id)
        type_out = getattr(node_out, "_port_types", {}).get(start_port_view.name)
        if not type_out:
            self.graph.viewer().end_live_connection()
            return
        if node_out not in self.nodes and node_out not in self.defaultNodes:
            self.graph.viewer().end_live_connection()
            return
        left = self._getNodeRef(node_out)
        leftNodeData = None if isinstance(left, str) else self.nodeGraph.nodes[self.key][left]
        leftOutPin = -1
        r_type = None
        out_name = start_port_view.name
        if type_out == "Exec":
            if leftNodeData is None:
                self.graph.viewer().end_live_connection()
                return  # Default nodes have no exec outputs
            execSplits = getExecSplits(leftNodeData.nodeFunction)
            if execSplits:
                keys = list(execSplits.keys())
                if out_name.startswith("out_"):
                    key = out_name[4:]
                    if key in keys:
                        leftOutPin = keys.index(key)
        elif type_out == "Params":
            if leftNodeData is None:
                leftOutPin = 0  # Default param node — single output
            else:
                returnTypes = getReturnTypes(leftNodeData.nodeFunction)
                if returnTypes:
                    keys = list(returnTypes.keys())
                    if out_name in keys:
                        leftOutPin = keys.index(out_name)
                        r_type = returnTypes.get(out_name)
        if leftOutPin == -1:
            self.graph.viewer().end_live_connection()
            return
        self._pending_conn = {
            "left": left,
            "leftOutPin": leftOutPin,
            "linkType": type_out,
            "scene_pos": scene_pos,
            "r_type": r_type,
        }
        sources = {}
        for module in self.nodeGraph.modules_:
            sources[module.__name__] = module
        if self.nodeGraph.parentClass:
            sources["Parent"] = self.nodeGraph.parentClass
        if (type_out == "Params") and isinstance(r_type, type) and getattr(r_type, "__module__", "") != "builtins":
            sources["Parent"] = r_type
        self._closeFunctionPickerPopup("_connectionPickerPopup")
        popup = FunctionPickerPopup(getIndependentDialogParent(self), sources, filterExecOnly=(type_out == "Exec"))
        self._connectionPickerPopup = popup
        popup.FUNCTION_SELECTED.connect(self._onFunctionSelectedFromPrompt)
        popup.destroyed.connect(self._onFunctionPickerClosed)
        popup.destroyed.connect(lambda: setattr(self, "_connectionPickerPopup", None))
        popup.move(QtGui.QCursor.pos())
        QtCore.QTimer.singleShot(0, lambda p=popup: p.show())

    def _onFunctionSelectedFromPrompt(self, path: str, is_parent: bool):
        from NodeGraph import EditorDataNode

        if not self._pending_conn:
            return
        func = None
        if is_parent and self.nodeGraph.parentClass:
            func = getattr(self.nodeGraph.parentClass, path, None)
        if not func:
            for module in self.nodeGraph.modules_:
                f = self.nodeGraph.getFunctionFromModule(module, path)
                if f:
                    func = f
                    break
        if not func:
            self.graph.viewer().end_live_connection()
            self._pending_conn = None
            return
        params = makeNodeParamsFromSignature(func)
        posf = self._pending_conn["scene_pos"]
        pos = (posf.x(), posf.y())
        node_data = EditorDataNode(path, params, pos)
        if self.key not in self.nodeGraph.dataNodes:
            self.nodeGraph.dataNodes[self.key] = []
        self.nodeGraph.dataNodes[self.key].append(node_data)
        self.nodeGraph.genNodesFromDataNodes()
        self.nodeGraph.genRelationsFromLinks()
        left = self._pending_conn["left"]
        leftOutPin = self._pending_conn["leftOutPin"]
        linkType = self._pending_conn["linkType"]
        right = len(self.nodeGraph.nodes[self.key]) - 1
        rightInPin = 0
        if linkType == "Params":
            sig2 = inspect.signature(func)
            param_order = []
            match_idx = None
            for pname, p in sig2.parameters.items():
                if pname == "self":
                    continue
                param_order.append(pname)
                ann = p.annotation
                if ann is not inspect._empty and ann == self._pending_conn.get("r_type"):
                    match_idx = len(param_order) - 1
            if match_idx is not None:
                rightInPin = match_idx
        link_data = {
            "left": left,
            "right": right,
            "leftOutPin": leftOutPin,
            "rightInPin": rightInPin,
            "linkType": linkType,
        }
        if link_data not in self.nodeGraph.links[self.key]:
            self.nodeGraph.links[self.key].append(link_data)
        self.nodeGraph.genRelationsFromLinks()
        GameData.recordSnapshot()
        self._refreshCallable(self.name, self.nodeGraph.asDict())
        self.MODIFIED.emit()
        self._refreshPanel()
        self.graph.viewer().end_live_connection()
        self._pending_conn = None

    def _onFunctionPickerClosed(self):
        self.graph.viewer().end_live_connection()
        self._pending_conn = None

    def onPortDisconnected(self, portIn, portOut):
        if not self._isLoading and portIn and portOut:
            node_in = portIn.node()
            node_out = portOut.node()
            left = self._getNodeRef(node_out)
            right = self._getNodeRef(node_in)
            leftNodeData = None if isinstance(left, str) else self.nodeGraph.nodes[self.key][left]
            rightNodeData = None if isinstance(right, str) else self.nodeGraph.nodes[self.key][right]

            linkType = getattr(node_out, "_port_types", {}).get(portOut.name())
            leftOutPin = -1
            rightInPin = -1

            if linkType == "Exec":
                if leftNodeData is None:
                    return  # Default nodes have no exec outputs
                latents = getLatents(leftNodeData.nodeFunction)
                execSplits = getExecSplits(leftNodeData.nodeFunction)
                if latents:
                    keys = list(latents.keys())
                    out_name = portOut.name()
                    if out_name.startswith("out_"):
                        key = out_name[4:]
                        if key in keys:
                            leftOutPin = keys.index(key)
                elif execSplits:
                    keys = list(execSplits.keys())
                    out_name = portOut.name()
                    if out_name.startswith("out_"):
                        key = out_name[4:]
                        if key in keys:
                            leftOutPin = keys.index(key)
                rightInPin = 0
            elif linkType == "Params":
                if leftNodeData is None:
                    leftOutPin = 0
                else:
                    returnTypes = getReturnTypes(leftNodeData.nodeFunction)
                    if returnTypes:
                        keys = list(returnTypes.keys())
                        if portOut.name() in keys:
                            leftOutPin = keys.index(portOut.name())

                if rightNodeData is not None:
                    param_keys = [k for k in rightNodeData.getParamList().keys()]
                    if portIn.name() in param_keys:
                        rightInPin = param_keys.index(portIn.name())

            if leftOutPin != -1 and rightInPin != -1:
                link_data = {
                    "left": left,
                    "right": right,
                    "leftOutPin": leftOutPin,
                    "rightInPin": rightInPin,
                    "linkType": linkType,
                }
                if link_data in self.nodeGraph.links[self.key]:
                    self.nodeGraph.links[self.key].remove(link_data)
                    self.nodeGraph.genNodesFromDataNodes()
                    self.nodeGraph.genRelationsFromLinks()
                    GameData.recordSnapshot()
                    self._refreshCallable(self.name, self.nodeGraph.asDict())
                    self.MODIFIED.emit()

        if portIn.type_() == "in":
            if not portIn.connected_ports():
                node = portIn.node()
                name = portIn.name()
                widgetName = self.resolveWidgetName(node, name)
                if node.get_widget(widgetName):
                    node.show_widget(widgetName, push_undo=False)
                if node in self.nodes:
                    self._applyNodeRely(self.nodes.index(node))

    def onNodesMoved(self, movedInfo):
        if self._isLoading:
            return
        for node, pos in movedInfo.items():
            nodeInst = self.graph.get_node_by_id(node.id)
            if nodeInst in self.defaultNodes:
                continue  # Default param node positions are not persisted
            idx = self.nodes.index(nodeInst)
            nodeInst = self.nodeGraph.nodes[self.key][idx]
            nodeInst.position = [node.pos().x(), node.pos().y()]

            if self.key in self.nodeGraph.dataNodes and 0 <= idx < len(self.nodeGraph.dataNodes[self.key]):
                dataNode = self.nodeGraph.dataNodes[self.key][idx]
                from NodeGraph import EditorDataNode

                if isinstance(dataNode, EditorDataNode):
                    dataNode.pos = [node.pos().x(), node.pos().y()]
        GameData.recordSnapshot()
        self._refreshCallable(self.name, self.nodeGraph.asDict())
        self.MODIFIED.emit()

    def _shouldEmulatePan(self, event: QtGui.QMouseEvent) -> bool:
        if self._isSpacePressed:
            return True
        modifiers = event.modifiers()
        return bool(modifiers & PAN_EMULATION_MODIFIER)

    def _applyZoomAtCursor(self, viewer: QtWidgets.QGraphicsView, scaleFactor: float) -> None:
        if scaleFactor <= 0:
            return
        currentScale = float(viewer.transform().m11())
        if currentScale <= 0:
            return
        nextScale = currentScale * scaleFactor
        if nextScale < MIN_VIEW_SCALE:
            scaleFactor = MIN_VIEW_SCALE / currentScale
        elif nextScale > MAX_VIEW_SCALE:
            scaleFactor = MAX_VIEW_SCALE / currentScale

        if scaleFactor == 1.0:
            return

        prevAnchor = viewer.transformationAnchor()
        viewer.setTransformationAnchor(QtWidgets.QGraphicsView.AnchorUnderMouse)
        viewer.scale(scaleFactor, scaleFactor)
        viewer.setTransformationAnchor(prevAnchor)

    def _sendMouseEventAsMiddleButton(
        self, target: QtWidgets.QWidget, originalEvent: QtGui.QMouseEvent, eventType: QtCore.QEvent.Type
    ) -> None:
        localPos = originalEvent.localPos()
        windowPos = originalEvent.windowPos()
        screenPos = originalEvent.screenPos()

        if eventType == QtCore.QEvent.MouseMove:
            button = QtCore.Qt.NoButton
            buttons = QtCore.Qt.MiddleButton
        elif eventType == QtCore.QEvent.MouseButtonPress:
            button = QtCore.Qt.MiddleButton
            buttons = QtCore.Qt.MiddleButton
        else:
            button = QtCore.Qt.MiddleButton
            buttons = QtCore.Qt.NoButton

        modifiers = originalEvent.modifiers() & ~(QtCore.Qt.AltModifier | QtCore.Qt.MetaModifier)
        emulatedEvent = QtGui.QMouseEvent(eventType, localPos, windowPos, screenPos, button, buttons, modifiers)
        QtCore.QCoreApplication.sendEvent(target, emulatedEvent)

    def eventFilter(self, watched: QtCore.QObject, event: QtCore.QEvent) -> bool:
        viewer = self.graph.viewer()
        viewport = viewer.viewport()

        if watched == viewer and isinstance(event, QtGui.QKeyEvent) and event.type() == QtCore.QEvent.KeyPress:
            if event.key() == QtCore.Qt.Key_Space:
                self._isSpacePressed = True
        if watched == viewer and isinstance(event, QtGui.QKeyEvent) and event.type() == QtCore.QEvent.KeyRelease:
            if event.key() == QtCore.Qt.Key_Space:
                self._isSpacePressed = False
        if watched == viewer and event.type() == QtCore.QEvent.FocusOut:
            self._isSpacePressed = False
            self._isPanEmulationActive = False

        if (
            watched == viewport
            and isinstance(event, QtGui.QMouseEvent)
            and event.type() == QtCore.QEvent.MouseButtonPress
        ):
            if event.button() == QtCore.Qt.RightButton:
                item = viewer.itemAt(event.pos())
                node_found = None
                while item:
                    if isinstance(item, NodeItem):
                        node = self.graph.get_node_by_id(item.id)
                        if node:
                            if not node.selected():
                                self.graph.clear_selection()
                                node.set_selected(True)
                            node_found = node
                            break
                    item = item.parentItem()

                if node_found:
                    self._showNodeContextMenu(event.globalPos())
                    return True
                else:
                    self._showGeneralContextMenu(event.globalPos())
                    return True

            if event.button() == QtCore.Qt.LeftButton and self._shouldEmulatePan(event):
                self._isPanEmulationActive = True
                self._sendMouseEventAsMiddleButton(viewport, event, QtCore.QEvent.MouseButtonPress)
                return True

        if (
            watched == viewport
            and isinstance(event, QtGui.QMouseEvent)
            and event.type() == QtCore.QEvent.MouseMove
            and self._isPanEmulationActive
        ):
            if event.buttons() & QtCore.Qt.LeftButton:
                self._sendMouseEventAsMiddleButton(viewport, event, QtCore.QEvent.MouseMove)
                return True

        if (
            watched == viewport
            and isinstance(event, QtGui.QMouseEvent)
            and event.type() == QtCore.QEvent.MouseButtonRelease
            and self._isPanEmulationActive
        ):
            if event.button() == QtCore.Qt.LeftButton:
                self._isPanEmulationActive = False
                self._sendMouseEventAsMiddleButton(viewport, event, QtCore.QEvent.MouseButtonRelease)
                return True

        if watched == viewport and isinstance(event, QtGui.QWheelEvent) and event.type() == QtCore.QEvent.Wheel:
            source = event.source()
            mouseNotSynthesized = getattr(QtCore.Qt, "MouseEventNotSynthesized", None)
            if mouseNotSynthesized is None:
                mouseEventSourceEnum = getattr(QtCore.Qt, "MouseEventSource", None)
                if mouseEventSourceEnum is not None:
                    mouseNotSynthesized = getattr(mouseEventSourceEnum, "MouseEventNotSynthesized", None)
            if mouseNotSynthesized is not None:
                try:
                    isRealMouseWheel = int(source) == int(mouseNotSynthesized)
                except Exception:
                    isRealMouseWheel = source == mouseNotSynthesized
                if isRealMouseWheel:
                    return super(NodePanel, self).eventFilter(watched, event)

            modifiers = event.modifiers()
            if modifiers & (QtCore.Qt.ControlModifier | QtCore.Qt.AltModifier | QtCore.Qt.MetaModifier):
                return super(NodePanel, self).eventFilter(watched, event)

            if self._trackpadZoomWheelBlockUntilMs:
                nowMs = int(QtCore.QDateTime.currentMSecsSinceEpoch())
                if nowMs < self._trackpadZoomWheelBlockUntilMs:
                    event.accept()
                    return True
            pixelDelta = event.pixelDelta()
            if isinstance(pixelDelta, QtCore.QPoint) and not pixelDelta.isNull():
                h = viewer.horizontalScrollBar()
                v = viewer.verticalScrollBar()
                dx = int(pixelDelta.x())
                dy = int(pixelDelta.y())
                h.setValue(h.value() - dx)
                v.setValue(v.value() - dy)
                if (
                    h.minimum() == 0
                    and h.maximum() == 0
                    and v.minimum() == 0
                    and v.maximum() == 0
                    and (dx != 0 or dy != 0)
                ):
                    currentScale = float(viewer.transform().m11()) or 1.0
                    viewer.translate(-dx / currentScale, -dy / currentScale)
                event.accept()
                return True

        if watched == viewport and event.type() == QtCore.QEvent.NativeGesture:
            if isinstance(event, QtGui.QNativeGestureEvent):
                zoomGestureType = getattr(QtCore.Qt, "ZoomNativeGesture", None)
                if zoomGestureType is None:
                    nativeGestureTypeEnum = getattr(QtCore.Qt, "NativeGestureType", None)
                    if nativeGestureTypeEnum is not None:
                        zoomGestureType = getattr(nativeGestureTypeEnum, "ZoomNativeGesture", None)

                smartZoomGestureType = getattr(QtCore.Qt, "SmartZoomNativeGesture", None)
                if smartZoomGestureType is None:
                    nativeGestureTypeEnum = getattr(QtCore.Qt, "NativeGestureType", None)
                    if nativeGestureTypeEnum is not None:
                        smartZoomGestureType = getattr(nativeGestureTypeEnum, "SmartZoomNativeGesture", None)

                gestureType = event.gestureType()
                isZoom = False
                isSmartZoom = False
                if zoomGestureType is not None:
                    try:
                        isZoom = int(gestureType) == int(zoomGestureType)
                    except Exception:
                        isZoom = gestureType == zoomGestureType
                if smartZoomGestureType is not None:
                    try:
                        isSmartZoom = int(gestureType) == int(smartZoomGestureType)
                    except Exception:
                        isSmartZoom = gestureType == smartZoomGestureType

                if isZoom:
                    value = float(event.value())
                    scaleFactor = max(0.01, 1.0 + value)
                    self._applyZoomAtCursor(viewer, scaleFactor)
                    nowMs = int(QtCore.QDateTime.currentMSecsSinceEpoch())
                    self._trackpadZoomWheelBlockUntilMs = nowMs + TRACKPAD_ZOOM_WHEEL_BLOCK_MS
                    event.accept()
                    return True

                if isSmartZoom:
                    event.accept()
                    return True

        return super(NodePanel, self).eventFilter(watched, event)

    def _showNodeContextMenu(self, global_pos):
        menu = QtWidgets.QMenu(self)

        is_start_node = False
        is_default_node = False
        selectedNodes = self._getSelectedNodes()
        if selectedNodes:
            selectedNode = selectedNodes[0]
            if selectedNode in self.defaultNodes:
                is_default_node = True
            elif selectedNode in self.nodes:
                idx = self.nodes.index(selectedNode)
                if idx == self.nodeGraph.startNodes.get(self.key):
                    is_start_node = True

        if is_default_node:
            # Default param nodes have no editable actions
            pass
        elif is_start_node:
            cancelStartNode_action = menu.addAction(ELOC("CANCEL_START_NODE"))
            if cancelStartNode_action is None:
                return
            cancelStartNode_action.triggered.connect(self._onCancelStartNode)
        else:
            setAsStart_action = menu.addAction(ELOC("SET_AS_START"))
            if setAsStart_action is None:
                return
            setAsStart_action.triggered.connect(self._onSetAsStart)

        if not is_default_node:
            copy_action = menu.addAction(ELOC("COPY"))
            delete_action = menu.addAction(ELOC("DELETE"))

            if copy_action is None or delete_action is None:
                return

            copy_action.setShortcut(QtGui.QKeySequence.Copy)
            copy_action.triggered.connect(self._onCopy)

            delete_action.setShortcut(QtGui.QKeySequence.Delete)
            delete_action.triggered.connect(self._onDelete)

        menu.exec_(global_pos)

    def _showGeneralContextMenu(self, global_pos):
        menu = QtWidgets.QMenu(self)

        create_action = menu.addAction(ELOC("ADD_NODE"))
        paste_action = menu.addAction(ELOC("PASTE"))

        if create_action is None or paste_action is None:
            return

        create_action.setShortcut(QtGui.QKeySequence.New)
        create_action.triggered.connect(lambda: self._onCreate(global_pos))

        paste_action.setShortcut(QtGui.QKeySequence.Paste)
        paste_action.triggered.connect(self._onPaste)

        if NodePanel._COPY_BUFFER is None:
            paste_action.setEnabled(False)

        menu.exec_(global_pos)

    def _onCreate(self, global_pos=None):
        if global_pos is None:
            global_pos = QtGui.QCursor.pos()

        view = self.graph.viewer()
        view_pos = view.mapFromGlobal(global_pos)
        self._createNodeScenePos = view.mapToScene(view_pos)

        sources = {}
        if self.nodeGraph.parentClass:
            sources["Parent"] = self.nodeGraph.parentClass

        for module in self.nodeGraph.modules_:
            sources[module.__name__] = module

        self._closeFunctionPickerPopup("_functionPickerPopup")
        popup = FunctionPickerPopup(getIndependentDialogParent(self), sources)
        self._functionPickerPopup = popup
        popup.FUNCTION_SELECTED.connect(self._onFunctionSelected)
        popup.destroyed.connect(lambda: setattr(self, "_functionPickerPopup", None))
        popup.move(global_pos)
        QtCore.QTimer.singleShot(0, lambda p=popup: p.show())

    def _onFunctionSelected(self, path: str, is_parent: bool):
        from NodeGraph import EditorDataNode

        func = None
        if is_parent and self.nodeGraph.parentClass:
            func = getattr(self.nodeGraph.parentClass, path, None)

        if not func:
            for module in self.nodeGraph.modules_:
                func = self.nodeGraph.getFunctionFromModule(module, path)
                if func:
                    break

        if not func:
            return

        params = makeNodeParamsFromSignature(func)

        if self._createNodeScenePos is not None:
            scene_pos = self._createNodeScenePos
            self._createNodeScenePos = None
        else:
            view = self.graph.viewer()
            global_pos = QtGui.QCursor.pos()
            view_pos = view.mapFromGlobal(global_pos)
            scene_pos = view.mapToScene(view_pos)

        pos = (scene_pos.x(), scene_pos.y())

        node_data = EditorDataNode(path, params, pos)

        if self.key not in self.nodeGraph.dataNodes:
            self.nodeGraph.dataNodes[self.key] = []

        self.nodeGraph.dataNodes[self.key].append(node_data)
        self.nodeGraph.genNodesFromDataNodes()
        self.nodeGraph.genRelationsFromLinks()

        GameData.recordSnapshot()
        self._refreshCallable(self.name, self.nodeGraph.asDict())
        self.MODIFIED.emit()

        self._refreshPanel()

    def _onSetAsStart(self):
        selectedNodes = self._getSelectedNodes()
        if selectedNodes is None:
            return
        selectedNode = selectedNodes[0]
        if selectedNode in self.defaultNodes:
            return  # Default param nodes cannot be start nodes
        if selectedNode in self.nodes:
            idx = self.nodes.index(selectedNode)
            dataNode = self.nodeGraph.nodes[self.key][idx]
            if not hasExecOutputs(dataNode.nodeFunction):
                QtWidgets.QMessageBox.information(self, "Hint", ELOC("UNABLE_TO_SET_START_NODE"))
                return

            self.nodeGraph.startNodes[self.key] = idx
            GameData.recordSnapshot()
            self._refreshCallable(self.name, self.nodeGraph.asDict())
            self.MODIFIED.emit()
            self._refreshPanel()

    def _onCancelStartNode(self):
        self.nodeGraph.startNodes[self.key] = None
        GameData.recordSnapshot()
        self._refreshCallable(self.name, self.nodeGraph.asDict())
        self.MODIFIED.emit()
        self._refreshPanel()

    def _onCopy(self):
        nowNodes = self._getSelectedNodes()
        if nowNodes is None:
            return

        data_nodes = []
        for node in nowNodes:
            if node in self.defaultNodes:
                continue  # Don't copy default param nodes
            if node in self.nodes:
                idx = self.nodes.index(node)
                dataList = self.nodeGraph.dataNodes.get(self.key, [])
                if 0 <= idx < len(dataList):
                    data_nodes.append(copy.deepcopy(dataList[idx]))

        NodePanel._COPY_BUFFER = data_nodes or None

    def _onPaste(self):
        from NodeGraph import EditorDataNode

        if NodePanel._COPY_BUFFER is None:
            return
        if self.key not in self.nodeGraph.dataNodes:
            self.nodeGraph.dataNodes[self.key] = []
        for node in NodePanel._COPY_BUFFER:
            pos = copy.copy(getattr(node, "pos", (0, 0)))
            if isinstance(pos, tuple):
                pos = (pos[0] + 10, pos[1] + 10)
            else:
                pos[0] += 10
                pos[1] += 10
            self.nodeGraph.dataNodes[self.key].append(
                EditorDataNode(node.nodeFunction, copy.deepcopy(node.params), pos)
            )
        self.nodeGraph.genNodesFromDataNodes()
        self.nodeGraph.genRelationsFromLinks()
        GameData.recordSnapshot()
        self._refreshCallable(self.name, self.nodeGraph.asDict())
        self.MODIFIED.emit()
        self._refreshPanel()

    def _onDelete(self):
        nowNodes = self._getSelectedNodes()
        if not nowNodes:
            return
        # Filter out non-deletable default parameter nodes
        nowNodes = [n for n in nowNodes if n not in self.defaultNodes]
        if not nowNodes:
            return
        originNodeMap = {}
        for i, node in enumerate(self.nodeGraph.dataNodes[self.key]):
            originNodeMap[i] = node
        for node in nowNodes:
            if node in self.nodes:
                idx = self.nodes.index(node)
                dataNode = self.nodeGraph.dataNodes[self.key][idx]
                self.nodes.remove(node)
                self.nodeGraph.dataNodes[self.key].remove(dataNode)
        links = []
        for link in self.nodeGraph.links[self.key]:
            cpLink = copy.deepcopy(link)
            left = cpLink["left"]
            right = cpLink["right"]
            if isinstance(left, int):
                leftNode = originNodeMap[left]
            else:
                leftNode = left
            if isinstance(right, int):
                rightNode = originNodeMap[right]
            else:
                rightNode = right
            leftIndex = None
            rightIndex = None
            try:
                if isinstance(leftNode, str):
                    leftIndex = leftNode
                else:
                    leftIndex = self.nodeGraph.dataNodes[self.key].index(leftNode)
                if isinstance(rightNode, str):
                    rightIndex = rightNode
                else:
                    rightIndex = self.nodeGraph.dataNodes[self.key].index(rightNode)
            except ValueError:
                print(f"Link {cpLink} not found in dataNodes, which means the link is not connected to any node.")
                continue
            cpLink["left"] = leftIndex
            cpLink["right"] = rightIndex
            links.append(cpLink)
        if self.nodeGraph.startNodes[self.key] is not None:
            startNode = originNodeMap[self.nodeGraph.startNodes[self.key]]
            try:
                startNodeIndex = self.nodeGraph.dataNodes[self.key].index(startNode)
                self.nodeGraph.startNodes[self.key] = startNodeIndex
            except ValueError:
                print(
                    f"Start node {startNode} not found in dataNodes, which means the start node is not connected to any node."
                )
                self.nodeGraph.startNodes[self.key] = None

        self.nodeGraph.links[self.key] = links
        self.nodeGraph.genNodesFromDataNodes()
        self.nodeGraph.genRelationsFromLinks()
        GameData.recordSnapshot()
        self._refreshCallable(self.name, self.nodeGraph.asDict())
        self.MODIFIED.emit()
        self._refreshPanel()

    def _onParamChanged(self, nodeIndex: int, paramIndex: int, widget: QtWidgets.QWidget):
        text = getWidgetValue(widget)

        changed = False
        if self.key in self.nodeGraph.dataNodes and 0 <= nodeIndex < len(self.nodeGraph.dataNodes[self.key]):
            dataNode = self.nodeGraph.dataNodes[self.key][nodeIndex]
            if self.key in self.nodeGraph.nodes and 0 <= nodeIndex < len(self.nodeGraph.nodes[self.key]):
                node = self.nodeGraph.nodes[self.key][nodeIndex]
                paramNames = list(node.getParamList().keys())
                while len(dataNode.params) <= paramIndex and len(dataNode.params) < len(paramNames):
                    fillIndex = len(dataNode.params)
                    dataNode.params.append(getNodeParamValue(node, fillIndex, paramNames[fillIndex]))
            if 0 <= paramIndex < len(dataNode.params):
                if dataNode.params[paramIndex] != text:
                    dataNode.params[paramIndex] = text
                    changed = True
        if self.key in self.nodeGraph.nodes and 0 <= nodeIndex < len(self.nodeGraph.nodes[self.key]):
            node = self.nodeGraph.nodes[self.key][nodeIndex]
            paramNames = list(node.getParamList().keys())
            while len(node.params) <= paramIndex and len(node.params) < len(paramNames):
                fillIndex = len(node.params)
                node.params.append(getNodeParamValue(node, fillIndex, paramNames[fillIndex]))
            if 0 <= paramIndex < len(node.params):
                if node.params[paramIndex] != text:
                    node.params[paramIndex] = text
                    changed = True
        if changed:
            GameData.recordSnapshot()
            self._refreshCallable(self.name, self.nodeGraph.asDict())
            self.MODIFIED.emit()
        self._applyNodeRely(nodeIndex)

    def _getSelectedNodes(self):
        sels = self.graph.selected_nodes()
        if sels:
            return sels
        return None

    def _refreshPanel(self):
        self._isLoading = True
        viewer = self.graph.viewer()
        viewport = viewer.viewport() if viewer else None
        if viewer:
            viewer.setUpdatesEnabled(False)
        if viewport:
            viewport.setUpdatesEnabled(False)
        try:
            self.graph.delete_nodes(self.graph.all_nodes(), push_undo=False)
            self.graph.node_factory.clear_registered_nodes()
            self.classDict.clear()
            self.nodes.clear()
            self.defaultNodes.clear()
            self._registerNodes()
            self._createNodes()
            self._createLinks()
            self._applyAllNodeRely()
        finally:
            if viewport:
                viewport.setUpdatesEnabled(True)
                viewport.update()
            if viewer:
                viewer.setUpdatesEnabled(True)
                viewer.update()
            self._isLoading = False

    def _onUndo(self):
        self._parent._onUndo()

    def _onRedo(self):
        self._parent._onRedo()
