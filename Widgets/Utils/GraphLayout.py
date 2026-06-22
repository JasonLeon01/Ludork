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


def computeGraphLayoutPositions(
    nodeCount: int,
    links: List[Dict[str, Any]],
    nodeRely: GraphLayoutRelyMap,
    startIdx: Optional[int],
    defaultParamCount: int = 0,
    nodeHeights: Optional[List[float]] = None,
    options: Optional[GraphLayoutOptions] = None,
) -> GraphLayoutPositions:
    r"""Compute organised node positions for a blueprint or common-function graph.

    - \param nodeCount  Number of persisted graph nodes in the event/function
    - \param links      Link dictionaries for the graph key
    - \param nodeRely   Parameter dependency map for the graph key
    - \param startIdx   Entry node index, or None when unset
    - \param defaultParamCount  Number of visual default parameter nodes
    - \param nodeHeights  Estimated node heights used to resolve same-column overlaps
    - \param options    Layout spacing options
    - \return Mapping of node index or ``default_N`` key to ``(x, y)`` positions
    """
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
