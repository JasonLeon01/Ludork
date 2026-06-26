# -*- encoding: utf-8 -*-

from typing import Any, Optional
from Engine.Gameplay.Components import getComponentFieldValue, setComponentFieldValue


_MISSING = object()


def _getOwnerFromGraph(refLocal) -> Optional[Any]:
    graph = refLocal.get("__graph__")
    if graph is None:
        return None
    parent = getattr(graph, "parent", None)
    if parent is None:
        return None
    if hasattr(parent, "getOwner"):
        return parent.getOwner()
    return parent


@Meta(DisplayName='LOC("GET_STATE_OWNER")', DisplayDesc='LOC("GET_STATE_OWNER_DESC")')
@ReturnType(battler=object)
def GetStateOwner() -> Optional[object]:
    r"""\brief Get the battler currently hosting this state's blueprint execution.

    - \return The hosting `Battler` instance, or None if not attached.
    """
    return _getOwnerFromGraph(GetStateOwner._refLocal)


@Meta(DisplayName='LOC("GET_EVENT_ARG")', DisplayDesc='LOC("GET_EVENT_ARG_DESC")')
@ReturnType(value=object)
def GetEventArg(name: str, default: Any = None) -> Any:
    r"""\brief Get a keyword argument injected into the current blueprint event.

    Convenience wrapper that reads `__<name>__` from the local graph context.
    Useful keys for state events: "battler".

    - \param name Argument name (without surrounding underscores).
    - \param default Value to return when the key is missing.
    - \return The argument value or `default`.
    """
    key = f"__{name}__"
    return GetEventArg._refLocal.get(key, default)


@Meta(DisplayName='LOC("GET_BATTLER_ATTR")', DisplayDesc='LOC("GET_BATTLER_ATTR_DESC")')
@ReturnType(value=object)
def GetBattlerAttr(battler: Any, attrName: str, default: Any = None) -> Any:
    r"""\brief Read a named attribute from any battler (e.g. HP, MAXHP, ATK, DEF).

    - \param battler The target battler.
    - \param attrName Attribute name.
    - \param default Default when missing.
    - \return Attribute value or default.
    """
    if battler is None:
        return default
    value = getComponentFieldValue(battler, attrName, _MISSING)
    if value is not _MISSING:
        return value
    return getattr(battler, attrName, default)


@Meta(DisplayName='LOC("SET_BATTLER_ATTR")', DisplayDesc='LOC("SET_BATTLER_ATTR_DESC")')
@ExecSplit(default=(None,))
def SetBattlerAttr(battler: Any, attrName: str, value: Any) -> None:
    r"""\brief Write a named attribute on any battler.

    - \param battler The target battler.
    - \param attrName Attribute name.
    - \param value New value.
    """
    if battler is None:
        return
    if not setComponentFieldValue(battler, attrName, value):
        setattr(battler, attrName, value)


@Meta(DisplayName='LOC("DAMAGE_BATTLER")', DisplayDesc='LOC("DAMAGE_BATTLER_DESC")')
@ExecSplit(default=(None,))
def DamageBattler(battler: Any, amount: int = 1) -> None:
    r"""\brief Subtract HP from any battler (floored at 0).

    - \param battler The target battler.
    - \param amount HP to remove.
    """
    if battler is None:
        return
    hp = getComponentFieldValue(battler, "HP", None)
    if hp is None:
        return
    setComponentFieldValue(battler, "HP", int(hp) - int(amount))


@Meta(DisplayName='LOC("HEAL_BATTLER")', DisplayDesc='LOC("HEAL_BATTLER_DESC")')
@ExecSplit(default=(None,))
def HealBattler(battler: Any, amount: int = 1) -> None:
    r"""\brief Restore HP on any battler (capped at MAXHP when present).

    - \param battler The target battler.
    - \param amount HP to restore.
    """
    if battler is None:
        return
    hp = getComponentFieldValue(battler, "HP", None)
    if hp is None:
        return
    setComponentFieldValue(battler, "HP", int(hp) + int(amount))


@Meta(
    DisplayName='LOC("BATTLER_HAS_STATE")',
    DisplayDesc='LOC("BATTLER_HAS_STATE_DESC")',
    GeneralDataVars=[("stateID", "State")],
)
@ReturnType(value=bool)
def BattlerHasState(battler: Any, stateID: str) -> bool:
    r"""\brief Check whether a battler currently carries the given state.

    - \param battler The target battler.
    - \param stateID State identifier.
    - \return True if active.
    """
    if battler is None or not hasattr(battler, "hasState"):
        return False
    return bool(battler.hasState(stateID))


@Meta(
    DisplayName='LOC("ADD_STATE_TO")',
    DisplayDesc='LOC("ADD_STATE_TO_DESC")',
    GeneralDataVars=[("stateID", "State")],
)
@ExecSplit(default=(None,))
def AddStateTo(battler: Any, stateID: str, stacks: int = 1) -> None:
    r"""\brief Apply a state (by ID) to any battler.

    - \param battler The target battler.
    - \param stateID State identifier.
    - \param stacks Stack count to apply or add.
    """
    if battler is None or not hasattr(battler, "addState"):
        return
    battler.addState(stateID, stacks)


@Meta(
    DisplayName='LOC("REMOVE_STATE_FROM")',
    DisplayDesc='LOC("REMOVE_STATE_FROM_DESC")',
    GeneralDataVars=[("stateID", "State")],
)
@ExecSplit(default=(None,))
def RemoveStateFrom(battler: Any, stateID: str) -> None:
    r"""\brief Remove a state (by ID) from any battler.

    - \param battler The target battler.
    - \param stateID State identifier.
    """
    if battler is None or not hasattr(battler, "removeState"):
        return
    battler.removeState(stateID)


@Meta(
    DisplayName='LOC("REDUCE_STATE_FROM")',
    DisplayDesc='LOC("REDUCE_STATE_FROM_DESC")',
    GeneralDataVars=[("stateID", "State")],
)
@ExecSplit(default=(None,))
def ReduceStateFrom(battler: Any, stateID: str, stacks: int = 1) -> None:
    r"""\brief Reduce a state stack count on any battler and remove it at zero.

    - \param battler The target battler.
    - \param stateID State identifier.
    - \param stacks Stack count to reduce.
    """
    if battler is None or not hasattr(battler, "reduceStateStacks"):
        return
    battler.reduceStateStacks(stateID, stacks)
