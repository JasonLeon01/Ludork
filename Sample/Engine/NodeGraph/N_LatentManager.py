# -*- encoding: utf-8 -*-

from __future__ import annotations
import weakref
from typing import Any, Callable, Dict, List, Optional, Tuple, TYPE_CHECKING

if TYPE_CHECKING:
    from Engine.NodeGraph import Graph


class LatentManager:
    _instance = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(LatentManager, cls).__new__(cls)
            cls._instance._latents = []
        return cls._instance

    def __init__(self) -> None:
        # (graph_ref, key, condition, localRef, index)
        if not hasattr(self, "_latents"):
            self._latents: List[Tuple[weakref.ReferenceType[Graph], str, Callable, Dict[str, Any], int]] = []

    def add(self, graph: Graph, key: str, condition: Callable, localRef: Dict[str, Any], index: int) -> None:
        self._latents.append((weakref.ref(graph), key, condition, localRef, index))

    def update(self) -> None:
        for latent in self._latents[:]:
            if latent not in self._latents:
                continue

            graph_ref, key, condition, localRef, index = latent
            graph = graph_ref()
            if graph is None:
                self._removeLatentsForNode(graph, key, index)
                continue
            locals().update(localRef)
            result = condition()
            node = graph.getNodes(key)[index]
            nodeFunction = node.nodeFunction
            matched = False
            execIndex = -1
            keys = list(nodeFunction._latents.keys())
            for i, latentKey in enumerate(keys):
                if result in nodeFunction._latents[latentKey]:
                    matched = True
                    execIndex = i
                    break
            if matched:
                self._removeLatentsForNode(graph, key, index)
                nextMap = graph.nodeNexts.get(key, {}).get(index, {})
                if execIndex in nextMap:
                    nextNodeIndex = nextMap[execIndex][0]
                    graph.execute(key, nextNodeIndex)

    def _removeLatentsForNode(self, graph: Optional[Graph], key: str, index: int) -> None:
        if graph is None:
            self._latents = [l for l in self._latents if not (l[0]() is None)]
        else:
            self._latents = [l for l in self._latents if not (l[0]() == graph and l[1] == key and l[4] == index)]
