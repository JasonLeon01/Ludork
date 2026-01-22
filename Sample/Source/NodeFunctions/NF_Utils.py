# -*- encoding: utf-8 -*-

from __future__ import annotations
import inspect
from typing import Any
from Engine import System, ExecSplit, ReturnType
from Engine import SceneBase


class _attrRef:
    def __init__(self, obj: object, name: str):
        self.obj = obj
        self.name = name

    def get(self) -> Any:
        return getattr(self.obj, self.name)

    def set(self, value: Any) -> Any:
        setattr(self.obj, self.name, value)
        return value

    def _val(self):
        return getattr(self.obj, self.name)

    def __int__(self):
        return int(self._val())

    def __float__(self):
        return float(self._val())

    def __str__(self):
        return str(self._val())

    def __repr__(self):
        return f"_attrRef({self.obj!r}, {self.name!r}, value={self._val()!r})"

    def __add__(self, other):
        return self._val() + (other._val() if isinstance(other, _attrRef) else other)

    def __radd__(self, other):
        return (other._val() if isinstance(other, _attrRef) else other) + self._val()

    def __sub__(self, other):
        return self._val() - (other._val() if isinstance(other, _attrRef) else other)

    def __rsub__(self, other):
        return (other._val() if isinstance(other, _attrRef) else other) - self._val()

    def __mul__(self, other):
        return self._val() * (other._val() if isinstance(other, _attrRef) else other)

    def __rmul__(self, other):
        return (other._val() if isinstance(other, _attrRef) else other) * self._val()

    def __truediv__(self, other):
        return self._val() / (other._val() if isinstance(other, _attrRef) else other)

    def __rtruediv__(self, other):
        return (other._val() if isinstance(other, _attrRef) else other) / self._val()

    def __mod__(self, other):
        return self._val() % (other._val() if isinstance(other, _attrRef) else other)

    def __rmod__(self, other):
        return (other._val() if isinstance(other, _attrRef) else other) % self._val()

    def __pow__(self, other):
        return self._val() ** (other._val() if isinstance(other, _attrRef) else other)

    def __rpow__(self, other):
        return (other._val() if isinstance(other, _attrRef) else other) ** self._val()

    def __eq__(self, other):
        return self._val() == (other._val() if isinstance(other, _attrRef) else other)

    def __ne__(self, other):
        return self._val() != (other._val() if isinstance(other, _attrRef) else other)

    def __lt__(self, other):
        return self._val() < (other._val() if isinstance(other, _attrRef) else other)

    def __le__(self, other):
        return self._val() <= (other._val() if isinstance(other, _attrRef) else other)

    def __gt__(self, other):
        return self._val() > (other._val() if isinstance(other, _attrRef) else other)

    def __ge__(self, other):
        return self._val() >= (other._val() if isinstance(other, _attrRef) else other)


class _localRef:
    def __init__(self, loc: dict, name: str, default: Any = None):
        self.loc = loc
        self.name = name
        self.default = default

    def get(self) -> Any:
        return self.loc.get(self.name, self.default)

    def set(self, value: Any) -> Any:
        self.loc[self.name] = value
        return value

    def _val(self):
        return self.loc.get(self.name, self.default)

    def __int__(self):
        return int(self._val())

    def __float__(self):
        return float(self._val())

    def __str__(self):
        return str(self._val())

    def __repr__(self):
        return f"_localRef({self.loc!r}, {self.name!r}, value={self._val()!r})"

    def __add__(self, other):
        return self._val() + (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __radd__(self, other):
        return (other._val() if isinstance(other, (_attrRef, _localRef)) else other) + self._val()

    def __sub__(self, other):
        return self._val() - (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __rsub__(self, other):
        return (other._val() if isinstance(other, (_attrRef, _localRef)) else other) - self._val()

    def __mul__(self, other):
        return self._val() * (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __rmul__(self, other):
        return (other._val() if isinstance(other, (_attrRef, _localRef)) else other) * self._val()

    def __truediv__(self, other):
        return self._val() / (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __rtruediv__(self, other):
        return (other._val() if isinstance(other, (_attrRef, _localRef)) else other) / self._val()

    def __mod__(self, other):
        return self._val() % (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __rmod__(self, other):
        return (other._val() if isinstance(other, (_attrRef, _localRef)) else other) % self._val()

    def __pow__(self, other):
        return self._val() ** (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __rpow__(self, other):
        return (other._val() if isinstance(other, (_attrRef, _localRef)) else other) ** self._val()

    def __eq__(self, other):
        return self._val() == (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __ne__(self, other):
        return self._val() != (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __lt__(self, other):
        return self._val() < (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __le__(self, other):
        return self._val() <= (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __gt__(self, other):
        return self._val() > (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __ge__(self, other):
        return self._val() >= (other._val() if isinstance(other, (_attrRef, _localRef)) else other)


@ExecSplit(TRUE=(0,), FALSE=(1,))
def IF(condition: bool) -> int:
    return 0 if condition else 1


@ExecSplit(default=(None,))
def SetLocalValue(valueName: str, value: Any) -> None:
    SetLocalValue._refLocal[valueName] = value


@ReturnType(value=Any)
def GetLocalValue(valueName: str, default: Any = None) -> Any:
    return GetLocalValue._refLocal.get(valueName, default)


@ReturnType(value=Any)
def GetLocalValueRef(valueName: str, default: Any = None) -> Any:
    return _localRef(GetLocalValueRef._refLocal, valueName, default)


@ExecSplit(default=(None,))
def SUPER(obj: object) -> None:
    key = SUPER._refLocal.get("__key__")
    assert isinstance(key, str) and key
    cls = type(obj)
    parent_cls = getattr(cls, "__base__", None)
    if parent_cls is None or parent_cls is object:
        return
    if hasattr(cls, "_GENERATED_CLASS") and getattr(cls, "_GENERATED_CLASS"):
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
        if inspect.isfunction(method):
            method()
        else:
            raise AttributeError(f"Method '{key}' not found on object")


@ReturnType(value=object)
def SELF() -> object:
    return SELF._refLocal["__graph__"].parent


@ReturnType(value=object)
def GetAttrRef(obj: object, attrName: str) -> Any:
    return _attrRef(obj, attrName)


@ReturnType(value=object)
def GetAttr(obj: object, attrName: str) -> Any:
    return getattr(obj, attrName)


@ExecSplit(default=(None,))
def SetAttr(obj: object, attrName: str, value: Any) -> None:
    setattr(obj, attrName, value)


@ReturnType(value=SceneBase)
def GetScene() -> SceneBase:
    return System.getScene()


@ReturnType(value=bool)
def IsValidValue(value: Any) -> bool:
    return value is not None


@ExecSplit(default=(None,))
def Print(message: Any) -> None:
    print(message)


@ExecSplit(default=(None,))
def EXEC(script: str) -> None:
    exec(script)
