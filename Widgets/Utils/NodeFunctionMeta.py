# -*- encoding: utf-8 -*-

from __future__ import annotations

import inspect
from collections.abc import Callable
from typing import Any, Dict, Optional


NodeFunction = Callable[..., Any]
NodeMetaMap = Dict[str, Any]
_NODE_META_ATTRS = (
    "_meta",
    "_latents",
    "_execSplits",
    "_returnTypes",
    "_refLocal",
    "_eventSignature",
)
_BOUND_NODE_FUNCTION_META: Dict[int, NodeMetaMap] = {}


def _getBoundMeta(value: Any) -> Optional[NodeMetaMap]:
    meta = _BOUND_NODE_FUNCTION_META.get(id(value))
    if isinstance(meta, dict):
        return meta
    return None


def _getOwnerMeta(owner: Any, name: str) -> Optional[NodeMetaMap]:
    registry = getattr(owner, "__nodeFunctionMeta__", None)
    if not isinstance(registry, dict):
        return None
    meta = registry.get(name)
    if isinstance(meta, dict):
        return meta
    return None


def BindNodeFunctionMetadata(value: Any, owner: Any, name: str) -> Any:
    meta = _getOwnerMeta(owner, name)
    if meta is not None:
        _BOUND_NODE_FUNCTION_META[id(value)] = meta
    return value


def IsNodeFunction(value: Any) -> bool:
    if inspect.isfunction(value) or inspect.ismethod(value):
        return True
    if not callable(value):
        return False
    return _getBoundMeta(value) is not None or any(hasattr(value, attrName) for attrName in _NODE_META_ATTRS)


def GetNodeMetaMap(value: Any, attrName: str) -> NodeMetaMap:
    if not IsNodeFunction(value):
        return {}
    meta = getattr(value, attrName, None)
    if isinstance(meta, dict):
        return meta
    boundMeta = _getBoundMeta(value)
    if isinstance(boundMeta, dict):
        meta = boundMeta.get(attrName)
        if isinstance(meta, dict):
            return meta
    return {}


def GetLatents(value: Any) -> NodeMetaMap:
    return GetNodeMetaMap(value, "_latents")


def GetExecSplits(value: Any) -> NodeMetaMap:
    return GetNodeMetaMap(value, "_execSplits")


def GetReturnTypes(value: Any) -> NodeMetaMap:
    return GetNodeMetaMap(value, "_returnTypes")


def GetRefLocal(value: Any) -> Optional[NodeMetaMap]:
    if not IsNodeFunction(value):
        return None
    refLocal = getattr(value, "_refLocal", None)
    if isinstance(refLocal, dict):
        return refLocal
    boundMeta = _getBoundMeta(value)
    if isinstance(boundMeta, dict):
        refLocal = boundMeta.get("_refLocal")
        if isinstance(refLocal, dict):
            return refLocal
    return None


def HasExecOutputs(value: Any) -> bool:
    return bool(GetLatents(value) or GetExecSplits(value))


def IsSelectableNodeFunction(value: Any, requireExecSplit: bool = False) -> bool:
    if GetRefLocal(value) is None:
        return False
    return not requireExecSplit or bool(GetExecSplits(value))
