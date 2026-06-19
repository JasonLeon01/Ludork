# -*- encoding: utf-8 -*-

from __future__ import annotations
import logging
from typing import Any, Dict, Optional
from Engine import Pair, Vector2f, degrees
from Engine.Gameplay.Components import getComponentFieldValue, setComponentFieldValue
from Engine.Utils import Event
from Global import System, SceneBase, Animation
from .. import Data


_MISSING = object()


class _SuperProxy:
    def __init__(self, obj: object, cls: type, localGraph: Dict[str, Any]) -> None:
        self._obj = obj
        self._cls = cls
        self._localGraph = localGraph

    def _callSuperFunction(self, name: str, *args, **kwargs) -> Any:
        from Engine.BPBase import BPBase

        BPBase.ExecuteParentEvent(
            self._obj,
            self._cls,
            name,
            args=args,
            kwargs=kwargs,
            localGraph=self._localGraph,
        )

    def __getattr__(self, name: str) -> Any:
        parentCls = getattr(self._cls, "__base__", None)
        if parentCls is None:
            raise AttributeError(name)
        attr = getattr(parentCls, name)
        if callable(attr):
            return lambda *args, **kwargs: self._callSuperFunction(name, *args, **kwargs)
        return attr


class _attrRef:
    def __init__(self, obj: object, name: str) -> None:
        self.obj = obj
        self.name = name

    def get(self) -> Any:
        value = getComponentFieldValue(self.obj, self.name, _MISSING)
        if value is not _MISSING:
            return value
        return getattr(self.obj, self.name)

    def set(self, value: Any) -> Any:
        if not setComponentFieldValue(self.obj, self.name, value):
            setattr(self.obj, self.name, value)
        return value

    def _val(self) -> Any:
        return self.get()

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
def IF(condition: bool = False) -> int:
    r"""\brief Blueprint conditional branch.

    - \param condition The condition to evaluate.
    - \return 0 if True, 1 if False.
    """
    return 0 if condition else 1


@Meta(DisplayName='LOC("SET_LOCAL_VALUE")', DisplayDesc='LOC("SET_LOCAL_VALUE_DESC")')
@ExecSplit(default=(None,))
def SetLocalValue(valueName: str, value: Any = None) -> None:
    r"""\brief Set a local variable value.

    - \param valueName The variable name.
    - \param value The value to set.
    """
    SetLocalValue._refLocal[valueName] = value


@Meta(DisplayName='LOC("GET_LOCAL_VALUE")', DisplayDesc='LOC("GET_LOCAL_VALUE_DESC")')
@ReturnType(value=Any)
def GetLocalValue(valueName: str, default: Any = None) -> Any:
    r"""\brief Get a local variable value.

    - \param valueName The variable name.
    - \param default Default value if the variable is not found.
    - \return The variable value.
    """
    return GetLocalValue._refLocal.get(valueName, default)


@Meta(DisplayName='LOC("GET_LOCAL_VALUE_REF")', DisplayDesc='LOC("GET_LOCAL_VALUE_REF_DESC")')
@ReturnType(value=Any)
def GetLocalValueRef(valueName: str, default: Any = None) -> Any:
    r"""\brief Get a reference wrapper for a local variable.

    - \param valueName The variable name.
    - \param default Default value if the variable is not found.
    - \return A _localRef wrapper.
    """
    return _localRef(GetLocalValueRef._refLocal, valueName, default)


@Meta(DisplayName='LOC("SET_GAME_VARIABLE")', DisplayDesc='LOC("SET_GAME_VARIABLE_DESC")')
@ExecSplit(default=(None,))
def SetGameVariable(valueName: str, value: Any = None) -> None:
    r"""\brief Set a game variable on the current scene's game instance.

    - \param valueName The variable name.
    - \param value The value to set.
    """
    scene = System.getScene()
    if scene and hasattr(scene, "inst"):
        scene.inst.setVariable(valueName, value)


@Meta(DisplayName='LOC("GET_GAME_VARIABLE")', DisplayDesc='LOC("GET_GAME_VARIABLE_DESC")')
@ReturnType(value=Any)
def GetGameVariable(valueName: str, default: Any = None) -> Any:
    r"""\brief Get a game variable from the current scene's game instance.

    - \param valueName The variable name.
    - \param default Default value if the variable is not found.
    - \return The variable value.
    """
    scene = System.getScene()
    if scene and hasattr(scene, "inst"):
        return scene.inst.getVariable(valueName)
    return default


@Meta(DisplayName='LOC("GET_GAME_VARIABLE_REF")', DisplayDesc='LOC("GET_GAME_VARIABLE_REF_DESC")')
@ReturnType(value=Any)
def GetGameVariableRef(valueName: str, default: Any = None) -> Any:
    r"""\brief Get a reference wrapper for a game variable.

    - \param valueName The variable name.
    - \param default Default value if the variable is not found.
    - \return A _localRef wrapper.
    """
    scene = System.getScene()
    if scene and hasattr(scene, "inst"):
        return _localRef(scene.inst.getVariables(), valueName, default)
    return _localRef(GetGameVariableRef._fallbackVariables, valueName, default)


GetGameVariableRef._fallbackVariables: Dict[str, Any] = {}


@Meta(DisplayName='LOC("ADD_PLAYER_BY_CLASS")', DisplayDesc='LOC("ADD_PLAYER_BY_CLASS_DESC")')
@ExecSplit(default=(None,))
def AddPlayerByClass(playerClass: str) -> None:
    r"""\brief Add a new player by class path.

    - \param playerClass The class path for the player blueprint.
    """
    scene = System.getScene()
    if scene and hasattr(scene, "inst"):
        scene.inst.addPlayerByClass(playerClass)


@Meta(DisplayName='LOC("REMOVE_PLAYER_BY_CLASS")', DisplayDesc='LOC("REMOVE_PLAYER_BY_CLASS_DESC")')
@ExecSplit(default=(None,))
def RemovePlayerByClass(playerClass: str) -> None:
    r"""\brief Remove a player by class path.

    - \param playerClass The class path to remove.
    """
    scene = System.getScene()
    if scene and hasattr(scene, "inst"):
        scene.inst.removePlayerByClass(playerClass)


def _spawnAnim(animName: str, position: Vector2f, rotation: float, scale: Pair[float]) -> None:
    from Source import Data

    animData = Data.getAnimation(animName)
    if animData is None:
        raise ValueError(f"Animation '{animName}' not found")
    anim = Animation(animData)
    anim.setPosition(position)
    anim.setRotation(degrees(rotation))
    anim.setScale(Vector2f(*scale))
    scene = System.getScene()
    if scene:
        scene.addAnim(anim)


def _getActorByTag(actorTag: str):
    scene = System.getScene()
    if scene and hasattr(scene, "getGameMap"):
        gameMap = scene.getGameMap()
        if gameMap is not None and actorTag:
            return gameMap.getActorByTag(actorTag)
    return None


@Meta(DisplayName='LOC("ADD_ANIM")', DisplayDesc='LOC("ADD_ANIM_DESC")', Vector2fVars=["position", "scale"])
@ExecSplit(default=(None,))
def AddAnim(
    animName: str,
    position: Pair[float] = (0.0, 0.0),
    rotation: float = 0.0,
    scale: Pair[float] = (1.0, 1.0),
) -> None:
    r"""\brief Spawn an animation at a given position.

    - \param animName The animation name.
    - \param position The world position to spawn at.
    - \param rotation The rotation in degrees.
    - \param scale The scale as (x, y).
    """
    _spawnAnim(animName, Vector2f(*position), rotation, scale)


@Meta(DisplayName='LOC("ADD_ANIM_ON")', DisplayDesc='LOC("ADD_ANIM_ON_DESC")', Vector2fVars=["scale"])
@ExecSplit(default=(None,))
def AddAnimOn(animName: str, actorTag: str, rotation: float = 0.0, scale: Pair[float] = (1.0, 1.0)) -> None:
    r"""\brief Spawn an animation at an actor's current position.

    - \param animName The animation name.
    - \param actorTag The target actor tag.
    - \param rotation The rotation in degrees.
    - \param scale The scale as (x, y).
    """
    actor = _getActorByTag(actorTag)
    if actor is None:
        raise ValueError(f"Actor with tag '{actorTag}' not found")
    _spawnAnim(animName, actor.getPosition(), rotation, scale)


@Meta(DisplayName='LOC("GET_ANIM_LENGTH")', DisplayDesc='LOC("GET_ANIM_LENGTH_DESC")')
@ReturnType(value=float)
def GetAnimLength(animName: str) -> float:
    r"""\brief Get the duration of an animation.

    - \param animName The animation name.
    - \return The duration in seconds.
    """
    from Source import Data

    animData = Data.getAnimation(animName)
    if animData is None:
        raise ValueError(f"Animation '{animName}' not found")
    duration = animData.get("duration", None)
    return float(duration)


@Meta(DisplayName='LOC("GET_ANIM_VISUAL_LENGTH")', DisplayDesc='LOC("GET_ANIM_VISUAL_LENGTH_DESC")')
@ReturnType(value=float)
def GetAnimVisualLength(animName: str) -> float:
    r"""\brief Get the visual duration of an animation, excluding sound track length.

    - \param animName The animation name.
    - \return The visual duration in seconds.
    """
    from Engine.Animation import getAnimationVisualDuration
    from Source import Data

    animData = Data.getAnimation(animName)
    if animData is None:
        raise ValueError(f"Animation '{animName}' not found")
    return getAnimationVisualDuration(animData)


@Meta(DisplayName='LOC("SUPER")', DisplayDesc='LOC("SUPER_DESC")')
@ReturnType(value=object)
def SUPER(obj: object) -> object:
    graphContext = SUPER._refLocal.get("__graph__")
    cls = getattr(graphContext, "parentClass", None)
    if cls is None:
        cls = type(obj)
    return _SuperProxy(obj, cls, SUPER._refLocal)


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
    value = getComponentFieldValue(obj, attrName, _MISSING)
    if value is not _MISSING:
        return value
    return getattr(obj, attrName)


@Meta(DisplayName='LOC("SET_ATTR")', DisplayDesc='LOC("SET_ATTR_DESC")')
@ExecSplit(default=(None,))
def SetAttr(obj: object, attrName: str, value: Any) -> None:
    if not setComponentFieldValue(obj, attrName, value):
        setattr(obj, attrName, value)


@Meta(DisplayName='LOC("GET_SCENE")', DisplayDesc='LOC("GET_SCENE_DESC")')
@ReturnType(value=SceneBase)
def GetScene() -> Optional[SceneBase]:
    return System.getScene()


@Meta(DisplayName='LOC("IS_VALID_VALUE")', DisplayDesc='LOC("IS_VALID_VALUE_DESC")')
@ReturnType(value=bool)
def IsValidValue(value: Any = None) -> bool:
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


def _coerceEventBusKwargs(kwargs: Any) -> Dict[str, Any]:
    if kwargs is None or kwargs == "":
        return {}
    if isinstance(kwargs, dict):
        return kwargs
    raise TypeError("EventBus kwargs must be a dict or None")


@Meta(DisplayName='LOC("REGISTER_EVENT_BUS")', DisplayDesc='LOC("REGISTER_EVENT_BUS_DESC")')
@ExecSplit(default=(None,))
def RegisterEventBus(key: str, obj: object, functionName: str) -> None:
    r"""\brief Subscribe an object's event method to the shared EventBus.

    - \param key EventBus key to subscribe to.
    - \param obj Target object that owns the event or method.
    - \param functionName Name of the event or method to invoke.
    """
    if obj is None:
        raise ValueError("EventBus target object is None")
    if not hasattr(obj, functionName) or not callable(getattr(obj, functionName)):
        raise AttributeError(f"Object has no callable event '{functionName}'")

    def handler(payload: Any) -> None:
        from Engine.BPBase import BPBase

        BPBase.BlueprintEvent(obj, type(obj), functionName, _coerceEventBusKwargs(payload))

    Event.subscribeObjectHandler(key, obj, handler)


@Meta(DisplayName='LOC("REGISTER_EVENT_BUS_EVENT")', DisplayDesc='LOC("REGISTER_EVENT_BUS_EVENT_DESC")')
@ExecSplit(default=(None,))
def RegisterEventBusEvent(key: str, obj: object, eventName: str) -> None:
    r"""\brief Subscribe an object's blueprint event to the shared EventBus.

    EventBus payload is ignored; the blueprint event is invoked without arguments.

    - \param key EventBus key to subscribe to.
    - \param obj Target object that owns the blueprint event.
    - \param eventName Blueprint event name to invoke.
    """
    Event.subscribeBlueprintEvent(key, obj, eventName)


@Meta(DisplayName='LOC("UNREGISTER_EVENT_BUS")', DisplayDesc='LOC("UNREGISTER_EVENT_BUS_DESC")')
@ExecSplit(default=(None,))
@ReturnType(value=bool)
def UnregisterEventBus(key: str) -> bool:
    r"""\brief Unsubscribe handlers from the shared EventBus by key.

    - \param key EventBus key subscribed to.
    - \return True if any handler was found and removed, False otherwise.
    """
    return Event.unsubscribeEvent(key)


@Meta(DisplayName='LOC("UNREGISTER_EVENT_BUS_EVENT")', DisplayDesc='LOC("UNREGISTER_EVENT_BUS_EVENT_DESC")')
@ExecSplit(default=(None,))
@ReturnType(value=bool)
def UnregisterEventBusEvent(key: str, obj: Optional[object] = None) -> bool:
    r"""\brief Unsubscribe blueprint event handlers from the shared EventBus.

    - \param key EventBus key subscribed to.
    - \param obj Optional target object. If provided, only that object's handler is removed.
    - \return True if any handler was found and removed, False otherwise.
    """
    if obj is not None:
        return Event.unsubscribeObjectHandler(key, obj)
    return Event.unsubscribeEvent(key)


@Meta(DisplayName='LOC("TRIGGER_EVENT_BUS")', DisplayDesc='LOC("TRIGGER_EVENT_BUS_DESC")')
@ExecSplit(default=(None,))
def TriggerEventBus(key: str, kwargs: Optional[Dict[str, Any]] = None) -> None:
    r"""\brief Post an EventBus event with keyword arguments.

    - \param key EventBus key to trigger.
    - \param kwargs Keyword arguments passed to registered handlers.
    """
    Event.post(key, _coerceEventBusKwargs(kwargs))


@Meta(DisplayName='LOC("TRIGGER_BLUEPRINT_EVENT")', DisplayDesc='LOC("TRIGGER_BLUEPRINT_EVENT_DESC")')
@ExecSplit(default=(None,))
def TriggerBlueprintEvent(obj: object, eventName: str) -> None:
    r"""\brief Trigger a blueprint event on an object without arguments.

    - \param obj Target object that owns the blueprint event.
    - \param eventName Blueprint event name to invoke.
    """
    Event.triggerBlueprintEvent(obj, eventName)


@Meta(DisplayName='LOC("BACK_TO_TITLE")', DisplayDesc='LOC("BACK_TO_TITLE_DESC")')
@ExecSplit(default=(None,))
def BackToTitle() -> None:
    from Source.Scenes import Title

    System.setScene(Title())


@Meta(DisplayName='LOC("PRINT")', DisplayDesc='LOC("PRINT_DESC")')
@ExecSplit(default=(None,))
def Print(message: Any = "") -> None:
    logging.info("%s", message)


@Meta(DisplayName='LOC("EXEC")', DisplayDesc='LOC("EXEC_DESC")')
@ExecSplit(default=(None,))
def EXEC(script: str = "") -> None:
    exec(script)
