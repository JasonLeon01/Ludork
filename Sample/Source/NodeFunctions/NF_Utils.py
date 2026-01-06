# -*- encoding: utf-8 -*-

from typing import Any
from Engine import ExecSplit, ReturnType


@ExecSplit(TRUE=(0,), FALSE=(1,))
def IF(condition: bool) -> int:
    return 0 if condition else 1


@ExecSplit(default=(0,))
def SetLocalValue(valueName: str, value: Any) -> None:
    SetLocalValue._refLocal[valueName] = value
    return 0


@ReturnType(value=Any)
def GetLocalValue(valueName: str, default: Any = None) -> Any:
    return SetLocalValue._refLocal.get(valueName, default)


@ExecSplit(default=(0,))
def SUPER(obj: object) -> None:
    key = SUPER._refLocal.get("__key__")
    assert isinstance(key, str) and key
    cls = type(obj)
    parent_cls = getattr(cls, "__base__", None)
    if parent_cls is None or parent_cls is object:
        return
    if hasattr(cls, "GENERATED_CLASS") and getattr(cls, "GENERATED_CLASS"):
        graph = getattr(parent_cls, "_graph", None)
        if graph is None:
            raise RuntimeError("Parent class graph not found")
        graph.localGraph = SUPER._refLocal
        graph.execute(key)
    else:
        method = getattr(obj, key, None)
        if callable(method):
            method()
        else:
            raise AttributeError(f"Method '{key}' not found on object")
    return 0


@ReturnType(value=object)
def SELF() -> object:
    return SELF._refLocal["__graph__"].parent


@ReturnType(value=object)
def GetAttr(obj: object, attrName: str) -> Any:
    return getattr(obj, attrName)


@ExecSplit(default=(0,))
def SetAttr(obj: object, attrName: str, value: Any) -> None:
    setattr(obj, attrName, value)
    return 0


@ExecSplit(default=(0,))
def Print(message: Any) -> None:
    print(message)
    return 0


@ExecSplit(default=(0,))
def EXEC(script: str) -> None:
    exec(script)
    return 0
