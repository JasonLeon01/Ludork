# -*- encoding: utf-8 -*-

from typing import Any
from Engine import ExecSplit, ReturnType


@ExecSplit(TRUE=(0,), FALSE=(1,))
def IF(condition: bool) -> int:
    return 0 if condition else 1


@ExecSplit(default=(None,))
def SetLocalValue(valueName: str, value: Any) -> None:
    SetLocalValue._refLocal[valueName] = value


@ReturnType(value=Any)
def GetLocalValue(valueName: str, default: Any = None) -> Any:
    return SetLocalValue._refLocal.get(valueName, default)


@ExecSplit(default=(None,))
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
            try:
                getattr(parent_cls, key)(obj, *SUPER._refLocal[f"__{key}__"])
            except:
                raise RuntimeError("Parent class graph not found")
        else:
            graph.localGraph = SUPER._refLocal
            graph.execute(key)
    else:
        method = getattr(obj, key, None)
        if callable(method):
            method()
        else:
            raise AttributeError(f"Method '{key}' not found on object")


@ReturnType(value=object)
def SELF() -> object:
    return SELF._refLocal["__graph__"].parent


@ReturnType(value=object)
def GetAttr(obj: object, attrName: str) -> Any:
    return getattr(obj, attrName)


@ExecSplit(default=(None,))
def SetAttr(obj: object, attrName: str, value: Any) -> None:
    setattr(obj, attrName, value)


@ReturnType(value=bool)
def IsValidValue(value: Any) -> bool:
    return value is not None


@ExecSplit(default=(None,))
def Print(message: Any) -> None:
    print(message)


@ExecSplit(default=(None,))
def EXEC(script: str) -> None:
    exec(script)
