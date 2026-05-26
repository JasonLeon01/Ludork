# -*- encoding: utf-8 -*-

from __future__ import annotations

import inspect
from collections.abc import Callable
from typing import Any, Dict, Optional


NodeFunction = Callable[..., Any]
NodeMetaMap = Dict[str, Any]


def isNodeFunction(value: Any) -> bool:
    return inspect.isfunction(value) or inspect.ismethod(value)


def getNodeMetaMap(value: Any, attrName: str) -> NodeMetaMap:
    if not isNodeFunction(value):
        return {}
    meta = getattr(value, attrName, None)
    if isinstance(meta, dict):
        return meta
    return {}


def getLatents(value: Any) -> NodeMetaMap:
    return getNodeMetaMap(value, "_latents")


def getExecSplits(value: Any) -> NodeMetaMap:
    return getNodeMetaMap(value, "_execSplits")


def getReturnTypes(value: Any) -> NodeMetaMap:
    return getNodeMetaMap(value, "_returnTypes")


def getRefLocal(value: Any) -> Optional[NodeMetaMap]:
    if not isNodeFunction(value):
        return None
    refLocal = getattr(value, "_refLocal", None)
    if isinstance(refLocal, dict):
        return refLocal
    return None


def hasExecOutputs(value: Any) -> bool:
    return bool(getLatents(value) or getExecSplits(value))


def isSelectableNodeFunction(value: Any, requireExecSplit: bool = False) -> bool:
    if getRefLocal(value) is None:
        return False
    return not requireExecSplit or bool(getExecSplits(value))
