# -*- encoding: utf-8 -*-

from __future__ import annotations
import inspect
from typing import Any
from Engine import Pair, Vector2f, degrees
from Global import System, SceneBase, Animation
from .. import Data


class _attrRef:
    def __init__(self, obj: object, name: str) -> None:
        self.obj = obj
        self.name = name

    def get(self) -> Any:
        return getattr(self.obj, self.name)

    def set(self, value: Any) -> Any:
        setattr(self.obj, self.name, value)
        return value

    def _val(self) -> Any:
        return getattr(self.obj, self.name)

    def __int__(self) -> int:
        return int(self._val())

    def __float__(self) -> float:
        return float(self._val())

    def __str__(self) -> str:
        return str(self._val())

    def __repr__(self) -> str:
        return f"_attrRef({self.obj!r}, {self.name!r}, value={self._val()!r})"

    def __add__(self, other) -> Any:
        return self._val() + (other._val() if isinstance(other, _attrRef) else other)

    def __radd__(self, other) -> Any:
        return (other._val() if isinstance(other, _attrRef) else other) + self._val()

    def __sub__(self, other) -> Any:
        return self._val() - (other._val() if isinstance(other, _attrRef) else other)

    def __rsub__(self, other) -> Any:
        return (other._val() if isinstance(other, _attrRef) else other) - self._val()

    def __mul__(self, other) -> Any:
        return self._val() * (other._val() if isinstance(other, _attrRef) else other)

    def __rmul__(self, other) -> Any:
        return (other._val() if isinstance(other, _attrRef) else other) * self._val()

    def __truediv__(self, other) -> Any:
        return self._val() / (other._val() if isinstance(other, _attrRef) else other)

    def __rtruediv__(self, other) -> Any:
        return (other._val() if isinstance(other, _attrRef) else other) / self._val()

    def __mod__(self, other) -> Any:
        return self._val() % (other._val() if isinstance(other, _attrRef) else other)

    def __rmod__(self, other) -> Any:
        return (other._val() if isinstance(other, _attrRef) else other) % self._val()

    def __pow__(self, other) -> Any:
        return self._val() ** (other._val() if isinstance(other, _attrRef) else other)

    def __rpow__(self, other) -> Any:
        return (other._val() if isinstance(other, _attrRef) else other) ** self._val()

    def __eq__(self, other) -> Any:
        return self._val() == (other._val() if isinstance(other, _attrRef) else other)

    def __ne__(self, other) -> Any:
        return self._val() != (other._val() if isinstance(other, _attrRef) else other)

    def __lt__(self, other) -> Any:
        return self._val() < (other._val() if isinstance(other, _attrRef) else other)

    def __le__(self, other) -> Any:
        return self._val() <= (other._val() if isinstance(other, _attrRef) else other)

    def __gt__(self, other) -> Any:
        return self._val() > (other._val() if isinstance(other, _attrRef) else other)

    def __ge__(self, other) -> Any:
        return self._val() >= (other._val() if isinstance(other, _attrRef) else other)


class _localRef:
    def __init__(self, loc: Dict[str, Any], name: str, default: Any = None) -> None:
        self.loc = loc
        self.name = name
        self.default = default

    def get(self) -> Any:
        return self.loc.get(self.name, self.default)

    def set(self, value: Any) -> Any:
        self.loc[self.name] = value
        return value

    def _val(self) -> Any:
        return self.loc.get(self.name, self.default)

    def __int__(self) -> int:
        return int(self._val())

    def __float__(self) -> float:
        return float(self._val())

    def __str__(self) -> str:
        return str(self._val())

    def __repr__(self) -> str:
        return f"_localRef({self.loc!r}, {self.name!r}, value={self._val()!r})"

    def __add__(self, other) -> Any:
        return self._val() + (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __radd__(self, other) -> Any:
        return (other._val() if isinstance(other, (_attrRef, _localRef)) else other) + self._val()

    def __sub__(self, other) -> Any:
        return self._val() - (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __rsub__(self, other) -> Any:
        return (other._val() if isinstance(other, (_attrRef, _localRef)) else other) - self._val()

    def __mul__(self, other) -> Any:
        return self._val() * (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __rmul__(self, other) -> Any:
        return (other._val() if isinstance(other, (_attrRef, _localRef)) else other) * self._val()

    def __truediv__(self, other) -> Any:
        return self._val() / (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __rtruediv__(self, other) -> Any:
        return (other._val() if isinstance(other, (_attrRef, _localRef)) else other) / self._val()

    def __mod__(self, other) -> Any:
        return self._val() % (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __rmod__(self, other) -> Any:
        return (other._val() if isinstance(other, (_attrRef, _localRef)) else other) % self._val()

    def __pow__(self, other) -> Any:
        return self._val() ** (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __rpow__(self, other) -> Any:
        return (other._val() if isinstance(other, (_attrRef, _localRef)) else other) ** self._val()

    def __eq__(self, other) -> Any:
        return self._val() == (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __ne__(self, other) -> Any:
        return self._val() != (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __lt__(self, other) -> Any:
        return self._val() < (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __le__(self, other) -> Any:
        return self._val() <= (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __gt__(self, other) -> Any:
        return self._val() > (other._val() if isinstance(other, (_attrRef, _localRef)) else other)

    def __ge__(self, other) -> Any:
        return self._val() >= (other._val() if isinstance(other, (_attrRef, _localRef)) else other)


@Meta(DisplayName='LOC("IF")', DisplayDesc='LOC("IF_DESC")')
@ExecSplit(TRUE=(0,), FALSE=(1,))
def IF(condition: bool) -> int:
    return 0 if condition else 1


@Meta(DisplayName='LOC("SET_LOCAL_VALUE")', DisplayDesc='LOC("SET_LOCAL_VALUE_DESC")')
@ExecSplit(default=(None,))
def SetLocalValue(valueName: str, value: Any) -> None:
    SetLocalValue._refLocal[valueName] = value


@Meta(DisplayName='LOC("GET_LOCAL_VALUE")', DisplayDesc='LOC("GET_LOCAL_VALUE_DESC")')
@ReturnType(value=Any)
def GetLocalValue(valueName: str, default: Any = None) -> Any:
    return GetLocalValue._refLocal.get(valueName, default)


@Meta(DisplayName='LOC("GET_LOCAL_VALUE_REF")', DisplayDesc='LOC("GET_LOCAL_VALUE_REF_DESC")')
@ReturnType(value=Any)
def GetLocalValueRef(valueName: str, default: Any = None) -> Any:
    return _localRef(GetLocalValueRef._refLocal, valueName, default)


@Meta(DisplayName='LOC("ADD_ANIM")', DisplayDesc='LOC("ADD_ANIM_DESC")')
@ExecSplit(default=(None,))
def AddAnim(animName: str, position: Pair[float], rotation: float, scale: Pair[float]) -> None:
    from Source import Data

    animData = Data.getAnimation(animName)
    if animData is None:
        raise ValueError(f"Animation '{animName}' not found")
    anim = Animation(animData)
    anim.setPosition(Vector2f(*position))
    anim.setRotation(degrees(rotation))
    anim.setScale(Vector2f(*scale))
    System.getScene().addAnim(anim)


@Meta(DisplayName='LOC("GET_ANIM_LENGTH")', DisplayDesc='LOC("GET_ANIM_LENGTH_DESC")')
@ReturnType(value=float)
def GetAnimLength(animName: str) -> float:
    from Source import Data

    animData = Data.getAnimation(animName)
    if animData is None:
        raise ValueError(f"Animation '{animName}' not found")
    duration = animData.get("duration", None)
    return float(duration)


@Meta(DisplayName='LOC("SUPER")', DisplayDesc='LOC("SUPER_DESC")')
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


@Meta(DisplayName='LOC("SELF")', DisplayDesc='LOC("SELF_DESC")')
@ReturnType(value=object)
def SELF() -> object:
    return SELF._refLocal["__graph__"].parent


@Meta(DisplayName='LOC("GET_ATTR_REF")', DisplayDesc='LOC("GET_ATTR_REF_DESC")')
@ReturnType(value=object)
def GetAttrRef(obj: object, attrName: str) -> Any:
    return _attrRef(obj, attrName)


@Meta(DisplayName='LOC("GET_ATTR")', DisplayDesc='LOC("GET_ATTR_DESC")')
@ReturnType(value=object)
def GetAttr(obj: object, attrName: str) -> Any:
    return getattr(obj, attrName)


@Meta(DisplayName='LOC("SET_ATTR")', DisplayDesc='LOC("SET_ATTR_DESC")')
@ExecSplit(default=(None,))
def SetAttr(obj: object, attrName: str, value: Any) -> None:
    setattr(obj, attrName, value)


@Meta(DisplayName='LOC("GET_SCENE")', DisplayDesc='LOC("GET_SCENE_DESC")')
@ReturnType(value=SceneBase)
def GetScene() -> SceneBase:
    return System.getScene()


@Meta(DisplayName='LOC("IS_VALID_VALUE")', DisplayDesc='LOC("IS_VALID_VALUE_DESC")')
@ReturnType(value=bool)
def IsValidValue(value: Any) -> bool:
    return value is not None


@Meta(DisplayName='LOC("RUN_COMMON_FUNCTION")', DisplayDesc='LOC("RUN_COMMON_FUNCTION_DESC")')
@ExecSplit(default=(None,))
def RunCommonFunction(commonFunctionName: str) -> Any:
    callerGraph = RunCommonFunction._refLocal.get("__graph__")
    commonGraph = Data.getCommonFunction(commonFunctionName)
    if callerGraph is not None:
        commonGraph.localGraph = callerGraph.localGraph
    if commonGraph.hasKey("common"):
        return commonGraph.execute("common")
    if commonGraph.startNodes and len(commonGraph.startNodes) > 0:
        firstKey = sorted(commonGraph.startNodes.keys())[0]
        return commonGraph.execute(firstKey)
    raise KeyError(f"Common function '{commonFunctionName}' has no start nodes")


@Meta(DisplayName='LOC("PRINT")', DisplayDesc='LOC("PRINT_DESC")')
@ExecSplit(default=(None,))
def Print(message: Any) -> None:
    print(message)


@Meta(DisplayName='LOC("EXEC")', DisplayDesc='LOC("EXEC_DESC")')
@ExecSplit(default=(None,))
def EXEC(script: str) -> None:
    exec(script)
