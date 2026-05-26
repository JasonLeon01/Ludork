# -*- encoding: utf-8 -*-

from __future__ import annotations
import weakref
from typing import Any, Callable, Dict, List, Optional, Tuple, TYPE_CHECKING

if TYPE_CHECKING:
    from Engine.NodeGraph import Graph


class LatentManager:
    r"""Singleton manager for latent (delayed) node execution.

    Latent nodes poll a condition for one or more result values,
    then resume every connected branch matching those values.
    """

    _instance = None  #: Singleton instance
    _instanceInitialized = False

    def __new__(cls) -> LatentManager:
        r"""
        \brief __new__.
        """

        if cls._instance is None:
            cls._instance = super(LatentManager, cls).__new__(cls)
        return cls._instance

    def __init__(self) -> None:
        r"""(Re)initialise the LatentManager.

        - \brief Ensures `_latents` list exists on the singleton instance.
        """
        if type(self)._instanceInitialized:
            return
        # (graph_ref, key, condition, localRef, index)
        self._latents: List[Tuple[weakref.ReferenceType[Graph], str, Callable, Dict[str, Any], int]] = []
        type(self)._instanceInitialized = True

    def add(self, graph: Graph, key: str, condition: Callable, localRef: Dict[str, Any], index: int) -> None:
        r"""Register a latent node to be checked each frame.

        - \param graph      Graph containing the latent node
        - \param key        Event key of the graph
        - \param condition  Callable that returns the condition value
        - \param localRef   Local graph execution context
        - \param index      Index of the latent node in the graph
        """
        graph.onLatentAdded(key)
        self._latents.append((weakref.ref(graph), key, condition, localRef, index))

    def update(self) -> None:
        r"""Check all registered latent nodes and resume execution if conditions are met.

        - \brief Called each frame to poll latent node conditions.
        """
        for latent in self._latents[:]:
            if latent not in self._latents:
                continue

            graph_ref, key, condition, localRef, index = latent
            graph = graph_ref()
            if graph is None:
                self._removeLatentsForNode(graph, key, index)
                continue
            locals().update(localRef)
            results = self._normaliseResults(condition())
            node = graph.getNodes(key)[index]
            nodeFunction = node.nodeFunction
            execIndexes = self._getLatentExecIndexes(nodeFunction, results)
            if execIndexes:
                isFinished = self._isConditionFinished(condition)
                nextMap = graph.nodeNexts.get(key, {}).get(index, {})
                for execIndex in execIndexes:
                    if execIndex not in nextMap:
                        continue
                    nextNodeIndex = nextMap[execIndex][0]
                    graph.execute(key, nextNodeIndex)
                if isFinished:
                    self._removeLatentsForNode(graph, key, index)
                    graph.onLatentResolved(key)
                    graph.completeExecution(key)

    def _normaliseResults(self, result: Any) -> List[Any]:
        if result is None:
            return []
        if isinstance(result, (list, tuple, set)):
            return list(result)
        return [result]

    def _getLatentExecIndexes(self, nodeFunction: Callable, results: List[Any]) -> List[int]:
        execIndexes: List[int] = []
        keys = list(nodeFunction._latents.keys())
        for result in results:
            for i, latentKey in enumerate(keys):
                if result in nodeFunction._latents[latentKey] and i not in execIndexes:
                    execIndexes.append(i)
        return execIndexes

    def _isConditionFinished(self, condition: Callable) -> bool:
        isFinished = getattr(condition, "isFinished", None)
        if callable(isFinished):
            return bool(isFinished())
        return True

    def _removeLatentsForNode(self, graph: Optional[Graph], key: str, index: int) -> None:
        if graph is None:
            self._latents = [l for l in self._latents if not (l[0]() is None)]
        else:
            self._latents = [l for l in self._latents if not (l[0]() == graph and l[1] == key and l[4] == index)]
