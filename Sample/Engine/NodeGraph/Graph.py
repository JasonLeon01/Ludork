# -*- encoding: utf-8 -*-

from __future__ import annotations
import inspect
from typing import Any, Callable, Dict, List, Optional, Tuple, Union
from .. import Pair
from .Node import DataNode, Node


class Graph:
    """Blueprint node graph executor.

    Holds a collection of event-keyed node lists with their execution links.
    Resolves node functions from project modules, builds dependency/next
    relationships, and executes nodes in topological order from a start node.
    """

    def __init__(
        self,
        parentClassName: Optional[str],
        parentClass: Optional[type],
        parent: Optional[object],
        inNodes: Dict[str, List[DataNode]],
        links: Dict[str, List[Dict[str, Union[int, str]]]],
        nodeModel: Optional[type] = None,
        startNodes: Optional[Dict[str, int]] = None,
    ) -> None:
        r"""Construct a graph and immediately build nodes and relations.

        - \param parentClassName  Dot-path of the parent blueprint class
        - \param parentClass      Resolved parent class type
        - \param parent           Owner object instance (the actor/info)
        - \param inNodes          Event name -> list of DataNode definitions
        - \param links            Event name -> list of link dictionaries
        - \param nodeModel        Node class to instantiate (default: `Node`)
        - \param startNodes       Event name -> index of the entry node (or None)
        """
        import Engine, Global, Source

        self.modules_ = [Source, Global, Engine]  # Project modules to search functions from
        self.localGraph: Dict[str, Any] = {"__graph__": self}  # Local graph execution context
        self.parentClassName = parentClassName  # Dot-path of the parent blueprint class
        self.parentClass = parentClass  # Resolved parent class type
        self.parent = parent  # Owner object instance (the actor/info)
        self.dataNodes = inNodes  # Raw node data keyed by event name
        self.nodes: Dict[str, List[Node]] = {}  # Instantiated nodes keyed by event name
        self.links = links  # Connection links between nodes
        self.startNodes = startNodes  # Entry node index for each event
        self.nodeRely: Dict[str, Dict[int, Dict[int, Pair[int]]]] = {}  # Parameter dependency map
        self.nodeNexts: Dict[str, Dict[int, Dict[int, Pair[int]]]] = {}  # Execution order map
        if self.startNodes is None:
            self.startNodes = {}
        self.nodeModel = nodeModel  # Node class to instantiate
        if self.nodeModel is None:
            self.nodeModel = Node
        self.doingPartKey: Optional[str] = None  # Current event key being executed
        self._executionLocked: Dict[str, bool] = {}  # Re-entrancy lock per event
        self._latentPendingCount: Dict[str, int] = {}  # Pending latent node count per event
        self.genNodesFromDataNodes()
        self.genRelationsFromLinks()

    def genNodesFromDataNodes(self) -> None:
        """Instantiate Node objects from DataNode definitions, resolving function references."""
        for key, dataNodes in self.dataNodes.items():
            self.nodes[key] = []
            for dataNode in dataNodes:
                functionName = dataNode.nodeFunction
                functionAttr = None
                if isinstance(functionName, str) and functionName.startswith("self."):
                    functionAttr = self.getFunctionFromObject(self.parentClass, functionName[5:])
                else:
                    attr = getattr(self.parentClass, functionName, None)
                    if isinstance(attr, Callable):
                        functionAttr = attr
                if functionAttr is None:
                    functionAttr = None
                    for module_ in self.modules_:
                        functionAttr = self.getFunctionFromModule(module_, functionName)
                        if functionAttr is not None:
                            break
                    if functionAttr is None or not isinstance(functionAttr, Callable):
                        raise Exception(f"Function {functionName} not found in {module_.__name__}")
                paramList = [self, self.parent, dataNode.nodeFunction, functionAttr, dataNode.params]
                if hasattr(dataNode, "pos"):
                    paramList.append(dataNode.pos)
                if self.nodeModel:
                    self.nodes[key].append(self.nodeModel(*paramList))

    def genRelationsFromLinks(self) -> None:
        """Build execution order (nodeNexts) and parameter dependency (nodeRely) maps from links."""
        self.nodeRely.clear()
        self.nodeNexts.clear()
        self.nodeRely = {}
        self.nodeNexts = {}
        for key, linkList in self.links.items():
            self.nodeRely[key] = {}
            self.nodeNexts[key] = {}
            for link in linkList:
                left = link["left"]
                right = link["right"]
                leftOutPin = link["leftOutPin"]
                rightInPin = link["rightInPin"]
                linkType = link["linkType"]
                if not right in self.nodeRely[key]:
                    self.nodeRely[key][right] = {}
                if linkType == "Params":
                    self.nodeRely[key][right][rightInPin] = (left, leftOutPin)
                if linkType == "Exec":
                    if not left in self.nodeNexts[key]:
                        self.nodeNexts[key][left] = {}
                    self.nodeNexts[key][left][leftOutPin] = (right, rightInPin)

    def execute(self, key: str, startNode: Optional[int] = None, limit=1000000) -> Optional[Tuple[Any, ...]]:
        r"""Execute the node graph from a start node, following execution links.

        Handles latent nodes by delegating to LatentManager when encountered.

        - \param key         Event key to execute (e.g. "onUpdate")
        - \param startNode   Index of the entry node (uses startNodes[key] if None)
        - \param limit       Maximum number of steps before raising RuntimeError
        - \return Tuple of return values from the final node, or None
        """
        from . import latentManager

        self.doingPartKey = key
        if key not in self.nodes:
            raise KeyError(f"Graph key '{key}' not found")
        if key not in self.startNodes:
            raise KeyError(f"Start node for key '{key}' not set")
        if startNode is None and self.startNodes:
            startNode = self.startNodes[key]
        curr = startNode
        if curr is None:
            return None
        if not (0 <= curr < len(self.nodes[key])):
            raise IndexError(f"startIndex {curr} out of range for key '{key}'")
        cache: Dict[int, Tuple[Any, ...]] = {}
        steps = 0
        while True:
            result = self.executeNode(key, curr, cache)
            nodeFunc = self.nodes[key][curr].nodeFunction
            if hasattr(nodeFunc, "_latents"):
                condition = result[0] if isinstance(result, tuple) and len(result) > 0 else result
                latentManager.add(self, key, condition, self.localGraph, curr)
                return result
            nextMap = self.nodeNexts.get(key, {}).get(curr, {})
            if not nextMap:
                return result
            chosen = None
            splits = getattr(nodeFunc, "_execSplits", None)
            if splits and len(splits) > 0:
                for v in result:
                    matchFound = False
                    splitKeys = list(splits.keys())
                    for i, outPin in enumerate(splitKeys):
                        pinValues = splits[outPin]
                        for cv in pinValues:
                            if v == cv and i in nextMap:
                                chosen, _ = nextMap[i]
                                matchFound = True
                                break
                        if matchFound:
                            break
                    if matchFound:
                        break
            else:
                if len(nextMap) == 1:
                    chosen, _ = list(nextMap.values())[0]
                else:
                    outPin = sorted(nextMap.keys())[0]
                    chosen, _ = nextMap[outPin]
            if chosen is None:
                return result
            curr = chosen
            steps += 1
            if steps >= limit:
                raise RuntimeError(f"Max steps {limit} exceeded while executing graph '{key}'")

    def getRelyNodeIndexList(self, key: str, nodeIndex: int) -> List[int]:
        """Recursively collect all upstream dependency node indices for a given node."""
        rely = self.nodeRely.get(key, {})
        visited = set()
        order: List[int] = []

        def dfs(n: int) -> None:
            if n in visited:
                return
            visited.add(n)
            depMap = rely.get(n, {})
            if depMap:
                for inPin, src in sorted(depMap.items(), key=lambda item: item[0]):
                    left = src[0]
                    dfs(left)
                    if left not in order:
                        order.append(left)

        dfs(nodeIndex)
        return order

    def getNodes(self, key: str) -> List[Node]:
        r"""Get the list of nodes registered for a given event key.

        - \param key  Event key (e.g. "onUpdate")
        - \return List of Node objects for that event
        """
        return self.nodes[key]

    def executeNode(
        self, key: str, nodeIndex: int, _cache: Optional[Dict[int, Tuple[Any, ...]]] = None
    ) -> Tuple[Any, ...]:
        r"""Execute a single node, resolving its input dependencies first.

        Recursively evaluates upstream nodes on which this node depends.

        - \param key         Event key
        - \param nodeIndex   Index of the node to execute
        - \param _cache      Internal cache to avoid re-executing nodes
        - \return Tuple of return values from the node
        """
        if _cache is None:
            _cache = {}
        if nodeIndex in _cache:
            return _cache[nodeIndex]
        relyKey = self.nodeRely.get(key, {})
        depMap = relyKey.get(nodeIndex, {})
        inputPinReplace: Dict[int, Any] = {}
        if depMap:
            for rightInPin, src in sorted(depMap.items(), key=lambda item: item[0]):
                leftIndex, leftOutPin = src
                leftResult = self.executeNode(key, leftIndex, _cache)
                if not (0 <= leftOutPin < len(leftResult)):
                    raise IndexError(f"Output pin {leftOutPin} out of range for node {leftIndex}")
                inputPinReplace[rightInPin] = leftResult[leftOutPin]
        node = self.nodes[key][nodeIndex]
        result = node.execute(inputPinReplace)
        _cache[nodeIndex] = result
        return result

    def getFunctionFromModule(self, inModule, pathStr: str) -> Optional[Callable]:
        r"""Resolve a callable by dot-path from a module, recursively.

        - \param inModule  Root module to start searching from
        - \param pathStr   Dot-separated path to the function (e.g. "Utils.Print")
        - \return Resolved callable, or None if not found
        """
        nodes = pathStr.split(".")
        if len(nodes) > 0:
            if hasattr(inModule, nodes[0]):
                currentObj = getattr(inModule, nodes[0])
                if inspect.isfunction(currentObj):
                    return currentObj
                else:
                    return self.getFunctionFromModule(currentObj, ".".join(nodes[1:]))
        return None

    def getFunctionFromObject(self, obj: object, pathStr: str) -> Optional[Callable]:
        r"""Resolve a callable by dot-path from an object.

        - \param obj      Object to start searching from
        - \param pathStr  Dot-separated path to the function
        - \return Resolved callable, or None if not found
        """
        nodes = pathStr.split(".")
        currentObj = obj
        for node in nodes:
            currentObj = getattr(currentObj, node.strip())
        if inspect.isfunction(currentObj):
            return currentObj
        return None

    def hasKey(self, key: str) -> bool:
        """Check whether this graph has nodes registered for the given event key."""
        return key in self.nodes

    def tryLockExecution(self, key: str) -> bool:
        """Attempt to lock execution for re-entrancy protection. Returns `False` if already locked."""
        if self._executionLocked.get(key, False):
            return False
        self._executionLocked[key] = True
        return True

    def isExecutionLocked(self, key: str) -> bool:
        """Check whether execution is currently locked for the given event."""
        return self._executionLocked.get(key, False)

    def onLatentAdded(self, key: str) -> None:
        """Increment the pending latent node count for an event."""
        self._latentPendingCount[key] = self._latentPendingCount.get(key, 0) + 1

    def onLatentResolved(self, key: str) -> None:
        """Decrement the pending latent node count for an event."""
        count = self._latentPendingCount.get(key, 0)
        self._latentPendingCount[key] = max(0, count - 1)

    def completeExecution(self, key: str) -> None:
        """Unlock execution if no latent nodes are pending."""
        if self._latentPendingCount.get(key, 0) <= 0:
            self._executionLocked[key] = False

    def asDict(self) -> Dict[str, Any]:
        r"""Serialize the graph to a dictionary for storage.

        - \return Dictionary containing parent class, nodes, links, and start nodes
        """
        result = {}
        if self.parentClassName != "NOT_WRITTEN":
            result["parent"] = self.parentClassName
        result["nodeGraph"] = {}
        for key, nodes in self.nodes.items():
            result["nodeGraph"][key] = {}
            result["nodeGraph"][key]["nodes"] = []
            for i, node in enumerate(nodes):
                result["nodeGraph"][key]["nodes"].append(node.asDict())
            result["nodeGraph"][key]["links"] = self.links[key]
        result["startNodes"] = self.startNodes
        return result
