# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
import inspect
from typing import Callable, List, Dict, Optional


@dataclass
class DataNode:
    nodeFunction: str
    params: List[str]


class Node:
    def __init__(
        self,
        parent: Optional[object],
        nodeFunction: Callable,
        params: List[str],
        nexts: List[Node],
    ) -> None:
        self.parent = parent
        self.nodeFunction = nodeFunction
        self.params = params
        self.nexts = nexts
        self._funcInfo: str = ""
        self._paramList: Dict[str, type] = {}
        self._analyzeFunction()

    def getParamList(self) -> Dict[str, type]:
        return self._paramList

    def execute(self) -> None:
        actualParams = []
        for i in range(len(self.params)):
            paramKey = list(self._paramList.keys())[i]
            if self.params[i] == "self":
                actualParams.append(self.parent)
            elif self._paramList[paramKey] == str:
                param = None
                try:
                    param = eval(self.params[i])
                except:
                    param = self.params[i]
                actualParams.append(param)
            else:
                if isinstance(self.params[i], str):
                    actualParams.append(eval(self.params[i]))
                else:
                    actualParams.append(self.params[i])
        result: Optional[int] = self.nodeFunction(**actualParams)
        if len(self.nexts) > 0:
            if result is None:
                result = 0
            if result < len(self.nexts):
                self.nexts[result].execute()

    def _analyzeFunction(self) -> None:
        sig = inspect.signature(self.nodeFunction)
        self._funcInfo = self.nodeFunction.__name__
        self._paramList.clear()
        for paramName, paramObj in sig.parameters.items():
            paramType = paramObj.annotation
            if paramType == inspect.Parameter.empty:
                paramType = type(None)
            self._paramList[paramName] = paramType

    def __repr__(self) -> str:
        return f"{self._funcInfo}({self.params})"
