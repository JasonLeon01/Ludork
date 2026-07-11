# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
from typing import TYPE_CHECKING, Any, Optional

from PyQt5 import QtCore, QtWidgets

from EditorGlobal import GameData
from EditorGlobal.QmlDialogHost import QmlDialogHost

if TYPE_CHECKING:
    from Widgets.FileExplorer import FileExplorer


_REFERENCE_GRAPH_MAX_DEPTH = 5
_REFERENCE_GRAPH_X_STEP = 360
_REFERENCE_GRAPH_Y_STEP = 125
_REFERENCE_NODE_WIDTH = 240
_REFERENCE_NODE_HEIGHT = 74
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


class ReferenceTreeDialog(QmlDialogHost):
    def __init__(
        self, owner: FileExplorer, nodeId: str, parent: Optional[QtWidgets.QWidget] = None
    ) -> None:
        self._owner = owner
        self._nodeId = nodeId
        self._nodes: list[dict[str, object]] = []
        self._edges: list[dict[str, str]] = []
        self._visualSerial = 0
        title = ELOC("REFERENCE_TREE_TITLE").format(name=self._formatNode(nodeId))
        super().__init__(parent, title, QtCore.QSize(980, 620), QtCore.QSize(640, 420))
        self._buildGraph()
        graphWidth, graphHeight = self._normaliseGraphPositions()
        self.loadQml(
            "Dialogs/ReferenceTreeDialog.qml",
            {
                "referenceGraphNodes": self._nodes,
                "referenceGraphEdges": self._edges,
                "referenceGraphWidth": graphWidth,
                "referenceGraphHeight": graphHeight,
            },
        )

    @QtCore.pyqtSlot(str)
    def openNode(self, nodeId: str) -> None:
        path = GameData.GetReferenceNodePath(nodeId)
        if path and os.path.exists(path):
            self._owner._openSystemFile(path)

    def _buildGraph(self) -> None:
        rootVisual = self._createNode(self._nodeId, 0, 0, None, False)
        leftItems = self._treeItems("referencedBy")
        rightItems = self._treeItems("references")
        if leftItems:
            leftVisuals: list[str] = []
            self._createBranch(leftItems, rootVisual, "referencedBy", -1, 1, [0], leftVisuals)
            self._centerNodes(leftVisuals)
        else:
            self._createEmptyNode(ELOC("REFERENCE_REFERENCED_BY"), -_REFERENCE_GRAPH_X_STEP, -80)
        if rightItems:
            rightVisuals: list[str] = []
            self._createBranch(rightItems, rootVisual, "references", 1, 1, [0], rightVisuals)
            self._centerNodes(rightVisuals)
        else:
            self._createEmptyNode(ELOC("REFERENCE_REFERENCES"), _REFERENCE_GRAPH_X_STEP, -80)

    def _treeItems(self, direction: str) -> list[object]:
        tree = GameData.GetReferenceTree(self._nodeId, direction, maxDepth=_REFERENCE_GRAPH_MAX_DEPTH)
        items = tree.get("items", []) if isinstance(tree, dict) else []
        return items if isinstance(items, list) else []

    def _createBranch(
        self,
        items: list[object],
        parentVisual: str,
        direction: str,
        xSign: int,
        depth: int,
        yCursor: list[int],
        branchVisuals: list[str],
    ) -> None:
        for rawItem in items:
            if not isinstance(rawItem, dict):
                continue
            child = rawItem.get("child")
            record = rawItem.get("reference")
            if not isinstance(child, dict) or not isinstance(record, dict):
                continue
            nodeId = child.get("node")
            if not isinstance(nodeId, str):
                continue
            visual = self._createNode(
                nodeId,
                xSign * depth * _REFERENCE_GRAPH_X_STEP,
                yCursor[0],
                record,
                bool(child.get("cycle")),
            )
            branchVisuals.append(visual)
            yCursor[0] += _REFERENCE_GRAPH_Y_STEP
            if direction == "referencedBy":
                self._edges.append({"source": visual, "target": parentVisual})
            else:
                self._edges.append({"source": parentVisual, "target": visual})
            childItems = child.get("items", [])
            if not child.get("cycle") and isinstance(childItems, list) and childItems:
                self._createBranch(
                    childItems, visual, direction, xSign, depth + 1, yCursor, branchVisuals
                )

    def _createNode(
        self,
        nodeId: str,
        x: float,
        y: float,
        record: Optional[dict[str, Any]],
        cycle: bool,
    ) -> str:
        visualId = f"reference_{self._visualSerial}"
        self._visualSerial += 1
        node = GameData.GetReferenceNode(nodeId)
        nodeType = str(node.get("type", "unknown"))
        color = _REFERENCE_NODE_COLORS.get(nodeType, _REFERENCE_NODE_COLORS["unknown"])
        current = nodeId == self._nodeId
        if cycle:
            color = (112, 78, 78)
        elif current:
            color = tuple(min(255, channel + 28) for channel in color)
        content = self._formatNodeContent(nodeId)
        if cycle:
            content = f"{content} ({ELOC('REFERENCE_CYCLE')})"
        self._nodes.append(
            {
                "visualId": visualId,
                "nodeId": nodeId,
                "x": x,
                "y": y,
                "width": _REFERENCE_NODE_WIDTH,
                "height": _REFERENCE_NODE_HEIGHT,
                "title": self._formatNodeTitle(nodeId),
                "content": content,
                "color": "#{:02x}{:02x}{:02x}".format(*color),
                "current": current,
                "tooltip": self._formatNodeTooltip(nodeId, record),
            }
        )
        return visualId

    def _createEmptyNode(self, title: str, x: float, y: float) -> str:
        visualId = f"reference_{self._visualSerial}"
        self._visualSerial += 1
        self._nodes.append(
            {
                "visualId": visualId,
                "nodeId": "",
                "x": x,
                "y": y,
                "width": _REFERENCE_NODE_WIDTH,
                "height": _REFERENCE_NODE_HEIGHT,
                "title": title,
                "content": ELOC("REFERENCE_NONE"),
                "color": "#3a3a3a",
                "current": False,
                "tooltip": f"{title}: {ELOC('REFERENCE_NONE')}",
            }
        )
        return visualId

    def _centerNodes(self, visualIds: list[str]) -> None:
        nodes = [node for node in self._nodes if node["visualId"] in visualIds]
        if not nodes:
            return
        minimum = min(float(node["y"]) for node in nodes)
        maximum = max(float(node["y"]) for node in nodes)
        offset = -((minimum + maximum) / 2.0)
        for node in nodes:
            node["y"] = float(node["y"]) + offset

    def _normaliseGraphPositions(self) -> tuple[int, int]:
        padding = 80
        minimumX = min(float(node["x"]) for node in self._nodes)
        minimumY = min(float(node["y"]) for node in self._nodes)
        maximumX = max(float(node["x"]) + float(node["width"]) for node in self._nodes)
        maximumY = max(float(node["y"]) + float(node["height"]) for node in self._nodes)
        for node in self._nodes:
            node["x"] = float(node["x"]) - minimumX + padding
            node["y"] = float(node["y"]) - minimumY + padding
        return int(maximumX - minimumX + padding * 2), int(maximumY - minimumY + padding * 2)

    def _formatNode(self, nodeId: str) -> str:
        return f"{self._formatNodeTitle(nodeId)}: {self._formatNodeContent(nodeId)}"

    def _formatNodeTitle(self, nodeId: str) -> str:
        node = GameData.GetReferenceNode(nodeId)
        nodeType = str(node.get("type", "unknown"))
        return ELOC(_REFERENCE_TYPE_KEYS.get(nodeType, "REFERENCE_TYPE_UNKNOWN"))

    def _formatNodeContent(self, nodeId: str) -> str:
        node = GameData.GetReferenceNode(nodeId)
        return str(node.get("key", nodeId))

    def _formatNodeTooltip(
        self, nodeId: str, record: Optional[dict[str, Any]]
    ) -> str:
        lines = [self._formatNode(nodeId)]
        if record:
            kind = str(record.get("kind", ""))
            kindText = ELOC(_REFERENCE_KIND_KEYS.get(kind, "REFERENCE_KIND_REFERENCE"))
            recordPath = str(record.get("path", ""))
            lines.append(f"{kindText} - {recordPath}" if recordPath else kindText)
        path = GameData.GetReferenceNodePath(nodeId)
        if path:
            lines.append(path)
        return "\n".join(lines)
