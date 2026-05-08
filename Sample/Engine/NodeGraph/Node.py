# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
import inspect
from typing import Any, Callable, List, Dict, Optional, TYPE_CHECKING

if TYPE_CHECKING:
    from Engine.NodeGraph import Graph


@dataclass
class DataNode:
    """Serialized representation of a node (function path + parameter strings)."""

    nodeFunction: str  #: Dot-path to the callable (e.g. "NodeFunctions.Utils.Print")
    params: List[str]  #: List of parameter expressions as strings


class Node:
    """Runtime representation of a blueprint node.

    Wraps a callable function with its parameter expressions.
    On `execute`, evaluates parameter strings and invokes the function.
    """

    def __init__(
        self,
        parentGraph: Graph,
        parent: Optional[object],
        functionName: str,
        nodeFunction: Callable,
        params: List[str],
    ) -> None:
        r"""Construct a node bound to a graph, a parent object, and a callable.

        - \param parentGraph    Owning graph instance
        - \param parent         Actor/Info that owns this graph
        - \param functionName   Original dot-path string of the function
        - \param nodeFunction   Resolved callable reference
        - \param params         List of parameter expressions (evaluated at execute time)
        """
        self.parentGraph = parentGraph  # Owning graph instance
        self.parent = parent  # Actor/Info that owns this graph
        self.functionName = functionName  # Original dot-path string of the function
        self.nodeFunction = nodeFunction  # Resolved callable reference
        self.params = params  # List of parameter expressions
        self._funcInfo: str = ""  # Function name for display
        self._paramList: Dict[str, type] = {}  # Parameter name-to-type mapping
        self._paramDefaults: Dict[str, Any] = {}  # Parameter name-to-default mapping
        self._isSelfFunction: bool = isinstance(self.functionName, str) and self.functionName.startswith(
            "self."
        )  # Whether the function is a method of parent
        self._analyzeFunction()

    def getParamList(self) -> Dict[str, type]:
        """Get the parameter name-to-type mapping extracted from the function signature."""
        return self._paramList

    def getParamDefaults(self) -> Dict[str, Any]:
        """Get the default values for each parameter."""
        return self._paramDefaults

    def execute(self, inputPinReplace: Dict[int, Any] = {}) -> Any:
        r"""Execute this node's function with resolved parameters.

        Evaluates each parameter expression string, applies any input-pin
        overrides from connected nodes, and invokes the underlying callable.

        - \param inputPinReplace  Map of param index -> value from upstream node outputs
        - \return Tuple of return values from the function
        """
        actualParams = []
        eval_locals = {"self": self.parent} if self._isSelfFunction else None
        for i in range(len(self.params)):
            if i in inputPinReplace:
                actualParams.append(inputPinReplace[i])
                continue
            paramKey = list(self._paramList.keys())[i]
            if self.params[i] == "self":
                actualParams.append(self.parent)
            elif self._paramList[paramKey] == str:
                param = None
                try:
                    param = eval(self.params[i], {}, eval_locals) if eval_locals is not None else eval(self.params[i])
                except:
                    param = self.params[i]
                actualParams.append(param)
            else:
                if isinstance(self.params[i], str):
                    try:
                        actualParams.append(
                            eval(self.params[i], {}, eval_locals) if eval_locals is not None else eval(self.params[i])
                        )
                    except:
                        actualParams.append(self.params[i])
                else:
                    actualParams.append(self.params[i])
        if hasattr(self.nodeFunction, "_refLocal"):
            self.parentGraph.localGraph["__key__"] = self.parentGraph.doingPartKey
            self.nodeFunction._refLocal = self.parentGraph.localGraph
        result = self.nodeFunction(*actualParams)
        if not isinstance(result, tuple):
            result = (result,)
        return result

    def asDict(self) -> Dict[str, Any]:
        """Serialize this node back to a dictionary for storage."""
        result = {}
        result["nodeFunction"] = self.functionName
        result["params"] = self.params
        if hasattr(self, "position"):
            result["pos"] = self.position
        return result

    def _analyzeFunction(self) -> None:
        sig = inspect.signature(self.nodeFunction)
        self._funcInfo = self.nodeFunction.__name__
        self._paramList.clear()
        self._paramDefaults.clear()
        for paramName, paramObj in sig.parameters.items():
            paramType = paramObj.annotation
            if paramType == inspect.Parameter.empty:
                paramType = type(None)
            self._paramList[paramName] = paramType
            if paramObj.default != inspect.Parameter.empty:
                self._paramDefaults[paramName] = paramObj.default

    def __repr__(self) -> str:
        return f"{self._funcInfo}({self.params})"
