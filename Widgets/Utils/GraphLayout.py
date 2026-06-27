# -*- encoding: utf-8 -*-

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Tuple, Union

GraphLayoutPosition = Tuple[float, float]
GraphLayoutPositions = Dict[Union[int, str], GraphLayoutPosition]
GraphLayoutRelyMap = Dict[int, Dict[Any, Any]]


@dataclass(frozen=True)
class GraphLayoutOptions:
    xStep: float = 720.0
    yStep: float = 320.0
    defaultParamYStep: float = 64.0
    defaultParamStartY: float = 64.0
    startGap: float = 250.0
    columnPadding: float = 24.0


DEFAULT_GRAPH_LAYOUT_OPTIONS = GraphLayoutOptions()


def ComputeGraphLayoutPositions(
    nodeCount: int,
    links: List[Dict[str, Any]],
    nodeRely: GraphLayoutRelyMap,
    startIdx: Optional[int],
    defaultParamCount: int = 0,
    nodeHeights: Optional[List[float]] = None,
    options: Optional[GraphLayoutOptions] = None,
) -> GraphLayoutPositions:
    from EditorExtensions.EditorExt import C_ComputeGraphLayoutPositions

    layoutOptions = options or DEFAULT_GRAPH_LAYOUT_OPTIONS
    return C_ComputeGraphLayoutPositions(
        nodeCount,
        links,
        nodeRely,
        startIdx,
        defaultParamCount,
        nodeHeights or [],
        layoutOptions.xStep,
        layoutOptions.yStep,
        layoutOptions.defaultParamYStep,
        layoutOptions.defaultParamStartY,
        layoutOptions.startGap,
        layoutOptions.columnPadding,
    )
