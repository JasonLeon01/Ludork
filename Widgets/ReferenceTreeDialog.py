# -*- encoding: utf-8 -*-

import os
from typing import TYPE_CHECKING, Any, Dict, Optional
from PyQt5 import QtCore, QtGui, QtWidgets
from NodeGraphQt import BaseNode, NodeGraph
from NodeGraphQt.constants import PipeLayoutEnum
from NodeGraphQt.qgraphics.node_base import NodeItem
from NodeGraphQt.widgets.node_widgets import NodeBaseWidget
from EditorGlobal import GameData

if TYPE_CHECKING:
    from Widgets.FileExplorer import FileExplorer


_REFERENCE_NODE_TYPE = "Ludork.Reference.ReferenceGraphNode"
_REFERENCE_GRAPH_MAX_DEPTH = 5
_REFERENCE_GRAPH_X_STEP = 360
_REFERENCE_GRAPH_Y_STEP = 125
_REFERENCE_NODE_CONTENT = "content"
_REFERENCE_CURRENT_NODE_BORDER = (245, 198, 92, 255)
_REFERENCE_CURRENT_NODE_BORDER_WIDTH = 3.0
_REFERENCE_CURRENT_NODE_TEXT = (255, 235, 176, 255)
_REFERENCE_NODE_COLORS = {
    "asset": (70, 98, 118),
    "autoTile": (102, 120, 70),
    "blueprint": (86, 92, 142),
    "commonFunction": (120, 88, 138),
    "config": (118, 105, 72),
    "general": (112, 88, 72),
    "generalMember": (124, 96, 80),
    "map": (72, 118, 96),
    "animation": (128, 84, 102),
    "tileset": (82, 118, 118),
    "unknown": (82, 82, 82),
}
_REFERENCE_TYPE_KEYS = {
    "asset": "REFERENCE_TYPE_ASSET",
    "autoTile": "REFERENCE_TYPE_AUTOTILE",
    "blueprint": "REFERENCE_TYPE_BLUEPRINT",
    "commonFunction": "REFERENCE_TYPE_COMMON_FUNCTION",
    "config": "REFERENCE_TYPE_CONFIG",
    "general": "REFERENCE_TYPE_GENERAL",
    "generalMember": "REFERENCE_TYPE_GENERAL_MEMBER",
    "map": "REFERENCE_TYPE_MAP",
    "animation": "REFERENCE_TYPE_ANIMATION",
    "tileset": "REFERENCE_TYPE_TILESET",
}
_REFERENCE_KIND_KEYS = {
    "animationAsset": "REFERENCE_KIND_ANIMATION_ASSET",
    "asset": "REFERENCE_KIND_ASSET",
    "autoTile": "REFERENCE_KIND_AUTOTILE",
    "blueprintPath": "REFERENCE_KIND_BLUEPRINT_PATH",
    "configFile": "REFERENCE_KIND_CONFIG_FILE",
    "mapActor": "REFERENCE_KIND_MAP_ACTOR",
    "member": "REFERENCE_KIND_MEMBER",
    "nodeParam": "REFERENCE_KIND_NODE_PARAM",
    "parent": "REFERENCE_KIND_PARENT",
    "tileset": "REFERENCE_KIND_TILESET",
}


class ReferenceGraphNodeItem(NodeItem):
    def __init__(self, name: str = "node", parent: Optional[QtWidgets.QGraphicsItem] = None):
        super(ReferenceGraphNodeItem, self).__init__(name, parent)
        self._ludorkCurrentReference = False

    def paint(
        self,
        painter: QtGui.QPainter,
        option: QtWidgets.QStyleOptionGraphicsItem,
        widget: Optional[QtWidgets.QWidget] = None,
    ) -> None:
        super(ReferenceGraphNodeItem, self).paint(painter, option, widget)
        if not self._ludorkCurrentReference:
            return
        painter.save()
        painter.setRenderHint(QtGui.QPainter.Antialiasing, True)
        pen = QtGui.QPen(QtGui.QColor(*_REFERENCE_CURRENT_NODE_BORDER), _REFERENCE_CURRENT_NODE_BORDER_WIDTH)
        pen.setCosmetic(True)
        painter.setPen(pen)
        painter.setBrush(QtCore.Qt.NoBrush)
        inset = _REFERENCE_CURRENT_NODE_BORDER_WIDTH / 2.0
        rect = self.boundingRect().adjusted(inset, inset, -inset, -inset)
        painter.drawRoundedRect(rect, 6.0, 6.0)
        painter.restore()


class ReferenceGraphNode(BaseNode):
    __identifier__ = "Ludork.Reference"
    NODE_NAME = "Reference"

    def __init__(self):
        super(ReferenceGraphNode, self).__init__(ReferenceGraphNodeItem)
        self.add_input("in", multi_input=True, display_name=False)
        self.add_output("out", multi_output=True, display_name=False)
        self.add_text_input(_REFERENCE_NODE_CONTENT, text="")


class ReferenceTreeDialog(QtWidgets.QDialog):
    def __init__(self, owner: FileExplorer, nodeId: str, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self._owner = owner
        self._nodeId = nodeId
        self._nodeIdByVisualId: Dict[str, str] = {}
        self.setWindowTitle(ELOC("REFERENCE_TREE_TITLE").format(name=self._formatNode(nodeId)))
        self.resize(980, 620)
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        self._graph = NodeGraph()
        self._graph.register_node(ReferenceGraphNode)
        self._graph.disable_context_menu(True)
        self._graph.set_pipe_collision(False)
        self._graph.set_pipe_slicing(False)
        self._graph.set_pipe_style(PipeLayoutEnum.ANGLE.value)
        self._graph.node_double_clicked.connect(self._onNodeDoubleClicked)
        self._graphWidget = self._graph.widget
        self._graphWidget.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        tabBar = self._graphWidget.tabBar()
        if tabBar is not None:
            tabBar.hide()
        layout.addWidget(self._graphWidget, 1)
        self._populate()

    def _populate(self) -> None:
        self._graph.delete_nodes(self._graph.all_nodes(), push_undo=False)
        self._nodeIdByVisualId.clear()
        rootNode = self._createGraphNode(self._nodeId, 0, 0, None, False)

        leftItems = self._getTreeItems("referencedBy")
        rightItems = self._getTreeItems("references")
        if leftItems:
            leftNodes: list[BaseNode] = []
            self._createBranch(leftItems, rootNode, "referencedBy", -1, 1, {"value": 0}, leftNodes)
            self._centerNodes(leftNodes)
        else:
            self._createEmptyNode(ELOC("REFERENCE_REFERENCED_BY"), -_REFERENCE_GRAPH_X_STEP, -80)
        if rightItems:
            rightNodes: list[BaseNode] = []
            self._createBranch(rightItems, rootNode, "references", 1, 1, {"value": 0}, rightNodes)
            self._centerNodes(rightNodes)
        else:
            self._createEmptyNode(ELOC("REFERENCE_REFERENCES"), _REFERENCE_GRAPH_X_STEP, -80)

        self._lockGraph()
        QtCore.QTimer.singleShot(0, self._graph.fit_to_selection)

    def _getTreeItems(self, direction: str) -> list:
        tree = GameData.GetReferenceTree(self._nodeId, direction, maxDepth=_REFERENCE_GRAPH_MAX_DEPTH)
        items = tree.get("items", []) if isinstance(tree, dict) else []
        return items if isinstance(items, list) else []

    def _createBranch(
        self,
        items: list,
        parentNode: BaseNode,
        direction: str,
        xSign: int,
        depth: int,
        yCursor: Dict[str, int],
        branchNodes: list[BaseNode],
    ) -> None:
        for item in items:
            node = self._createReferenceItemNode(item, xSign * depth, yCursor["value"])
            if node is None:
                continue
            branchNodes.append(node)
            yCursor["value"] += _REFERENCE_GRAPH_Y_STEP
            if direction == "referencedBy":
                node.output(0).connect_to(parentNode.input(0), push_undo=False, emit_signal=False)
            else:
                parentNode.output(0).connect_to(node.input(0), push_undo=False, emit_signal=False)

            child = item.get("child")
            if not isinstance(child, dict) or child.get("cycle"):
                continue
            childItems = child.get("items", [])
            if isinstance(childItems, list) and childItems:
                self._createBranch(childItems, node, direction, xSign, depth + 1, yCursor, branchNodes)

    def _createReferenceItemNode(self, item: Dict[str, Any], xDepth: int, y: int) -> Optional[BaseNode]:
        child = item.get("child")
        record = item.get("reference")
        if not isinstance(child, dict) or not isinstance(record, dict):
            return None
        nodeId = child.get("node")
        if not isinstance(nodeId, str):
            return None
        return self._createGraphNode(nodeId, xDepth * _REFERENCE_GRAPH_X_STEP, y, record, bool(child.get("cycle")))

    def _createGraphNode(
        self, nodeId: str, x: float, y: float, record: Optional[Dict[str, Any]], cycle: bool
    ) -> BaseNode:
        name = self._formatNode(nodeId)
        title = self._formatNodeTitle(nodeId)
        content = self._formatNodeContent(nodeId)
        if cycle:
            content = f"{content} ({ELOC('REFERENCE_CYCLE')})"
        node = self._graph.create_node(_REFERENCE_NODE_TYPE, name=name, pos=[x, y], push_undo=False)
        refNode = GameData.GetReferenceNode(nodeId)
        nodeType = str(refNode.get("type", "unknown"))
        color = _REFERENCE_NODE_COLORS.get(nodeType, _REFERENCE_NODE_COLORS["unknown"])
        if cycle:
            color = (112, 78, 78)
        elif nodeId == self._nodeId:
            color = self._brightenColor(color)
        node.set_color(*color)
        self._setNodeTitle(node, title)
        self._setNodeContent(node, content, nodeId == self._nodeId)
        if nodeId == self._nodeId:
            node.view._ludorkCurrentReference = True
            node.set_property("border_color", _REFERENCE_CURRENT_NODE_BORDER, push_undo=False)
            node.set_property("text_color", _REFERENCE_CURRENT_NODE_TEXT, push_undo=False)
            node.view.draw_node()
        node.view.setToolTip(self._formatNodeTooltip(nodeId, record))
        self._nodeIdByVisualId[node.id] = nodeId
        return node

    def _createEmptyNode(self, title: str, x: float, y: float) -> BaseNode:
        name = f"{title}: {ELOC('REFERENCE_NONE')}"
        node = self._graph.create_node(_REFERENCE_NODE_TYPE, name=name, pos=[x, y], push_undo=False)
        node.set_color(58, 58, 58)
        self._setNodeTitle(node, title)
        self._setNodeContent(node, ELOC("REFERENCE_NONE"), False)
        return node

    def _centerNodes(self, nodes: list[BaseNode]) -> None:
        if not nodes:
            return
        minY = min(node.y_pos() for node in nodes)
        maxY = max(node.y_pos() for node in nodes)
        offset = -((minY + maxY) / 2.0)
        for node in nodes:
            x, y = node.pos()
            node.set_pos(x, y + offset)

    def _lockGraph(self) -> None:
        for node in self._graph.all_nodes():
            for port in list(node.inputs().values()) + list(node.outputs().values()):
                port.set_locked(True, connected_ports=False, push_undo=False)
            try:
                node.view._text_item.set_locked(True)
            except Exception:
                pass
            node.view.setFlag(QtWidgets.QGraphicsItem.ItemIsMovable, False)
        viewer = self._graph.viewer()
        viewer.setAcceptDrops(False)

    def _setNodeTitle(self, node: BaseNode, title: str) -> None:
        try:
            node.view._text_item.setPlainText(title)
            node.view.draw_node()
        except AttributeError:
            node.set_property("name", title, push_undo=False)

    def _setNodeContent(self, node: BaseNode, content: str, current: bool) -> None:
        node.set_property(_REFERENCE_NODE_CONTENT, content, push_undo=False)
        nodeWidget = node.get_widget(_REFERENCE_NODE_CONTENT)
        if not isinstance(nodeWidget, NodeBaseWidget):
            return
        groupWidget = nodeWidget.widget()
        if isinstance(groupWidget, QtWidgets.QWidget):
            groupWidget.setMaximumWidth(220)
        customWidget = nodeWidget.get_custom_widget()
        if not isinstance(customWidget, QtWidgets.QLineEdit):
            return
        customWidget.setReadOnly(True)
        customWidget.setFrame(False)
        customWidget.setFocusPolicy(QtCore.Qt.NoFocus)
        customWidget.setContextMenuPolicy(QtCore.Qt.NoContextMenu)
        customWidget.setCursor(QtCore.Qt.ArrowCursor)
        customWidget.setMinimumWidth(180)
        color = "rgba(255, 235, 176, 230)" if current else "rgba(235, 235, 235, 190)"
        customWidget.setStyleSheet(
            f"QLineEdit {{ background: transparent; border: 0px; color: {color}; padding: 1px 4px; }}"
        )
        node.view.draw_node()

    def _brightenColor(self, color: tuple[int, int, int]) -> tuple[int, int, int]:
        return (
            min(255, color[0] + 28),
            min(255, color[1] + 28),
            min(255, color[2] + 28),
        )

    def _formatNode(self, nodeId: str) -> str:
        return f"{self._formatNodeTitle(nodeId)}: {self._formatNodeContent(nodeId)}"

    def _formatNodeTitle(self, nodeId: str) -> str:
        node = GameData.GetReferenceNode(nodeId)
        nodeType = str(node.get("type", "unknown"))
        typeKey = _REFERENCE_TYPE_KEYS.get(nodeType, "REFERENCE_TYPE_UNKNOWN")
        return ELOC(typeKey)

    def _formatNodeContent(self, nodeId: str) -> str:
        node = GameData.GetReferenceNode(nodeId)
        return str(node.get("key", nodeId))

    def _formatNodeTooltip(self, nodeId: str, record: Optional[Dict[str, Any]]) -> str:
        lines = [self._formatNode(nodeId)]
        if record:
            lines.append(self._formatRecord(record))
        path = GameData.GetReferenceNodePath(nodeId)
        if path:
            lines.append(path)
        return "\n".join(lines)

    def _formatRecord(self, record: Dict[str, Any]) -> str:
        kind = str(record.get("kind", ""))
        kindKey = _REFERENCE_KIND_KEYS.get(kind, "REFERENCE_KIND_REFERENCE")
        path = str(record.get("path", ""))
        if path:
            return f"{ELOC(kindKey)} - {path}"
        return ELOC(kindKey)

    def _onNodeDoubleClicked(self, node: BaseNode) -> None:
        nodeId = self._nodeIdByVisualId.get(node.id)
        if not isinstance(nodeId, str):
            return
        path = GameData.GetReferenceNodePath(nodeId)
        if path and os.path.exists(path):
            self._owner._openSystemFile(path)
