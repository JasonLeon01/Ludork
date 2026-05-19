# -*- encoding: utf-8 -*-
r"""\brief Blueprint state nodes: damage context manipulation, host access, cross-battler state ops."""

from typing import Any, Optional


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
    Useful keys for state events: "battler", "context", "opponent", "won".

    - \param name Argument name (without surrounding underscores).
    - \param default Value to return when the key is missing.
    - \return The argument value or `default`.
    """
    key = f"__{name}__"
    return GetEventArg._refLocal.get(key, default)


@Meta(DisplayName='LOC("GET_DAMAGE_CONTEXT")', DisplayDesc='LOC("GET_DAMAGE_CONTEXT_DESC")')
@ReturnType(context=object)
def GetDamageContext() -> Any:
    r"""\brief Get the active `DamageContext` for the current event.

    - \return The `DamageContext`, or None if the event has no context.
    """
    return GetDamageContext._refLocal.get("__context__")


@Meta(DisplayName='LOC("GET_CTX_FIELD")', DisplayDesc='LOC("GET_CTX_FIELD_DESC")')
@ReturnType(value=object)
def GetCtxField(context: Any, name: str, default: Any = None) -> Any:
    r"""\brief Read a field from a `DamageContext` (e.g. atk, deF, damagePerRound, rounds, totalDamage).

    Falls back to `context.extra[name]` when the attribute does not exist.

    - \param context The damage context.
    - \param name Field name.
    - \param default Default value when the field is absent.
    - \return The field value or `default`.
    """
    if context is None:
        return default
    if hasattr(context, name):
        return getattr(context, name)
    extra = getattr(context, "extra", None)
    if isinstance(extra, dict):
        return extra.get(name, default)
    return default


@Meta(DisplayName='LOC("SET_CTX_FIELD")', DisplayDesc='LOC("SET_CTX_FIELD_DESC")')
@ExecSplit(default=(None,))
def SetCtxField(context: Any, name: str, value: Any) -> None:
    r"""\brief Write a field on a `DamageContext`.

    Known fields (atk, deF, damagePerRound, rounds, totalDamage, attacker,
    defender) are set as attributes; any other name is stored under
    `context.extra[name]`.

    - \param context The damage context.
    - \param name Field name.
    - \param value New value.
    """
    if context is None:
        return
    known = {"atk", "deF", "damagePerRound", "rounds", "totalDamage", "attacker", "defender"}
    if name in known:
        setattr(context, name, value)
        return
    extra = getattr(context, "extra", None)
    if isinstance(extra, dict):
        extra[name] = value
    else:
        setattr(context, name, value)


@Meta(DisplayName='LOC("ADD_CTX_FIELD")', DisplayDesc='LOC("ADD_CTX_FIELD_DESC")')
@ExecSplit(default=(None,))
def AddCtxField(context: Any, name: str, delta: Any) -> None:
    r"""\brief Add `delta` to a numeric field on the `DamageContext`.

    - \param context The damage context.
    - \param name Field name (e.g. "totalDamage", "damagePerRound", "atk", "deF").
    - \param delta Amount to add (negative to subtract).
    """
    current = GetCtxField(context, name, 0) or 0
    SetCtxField(context, name, current + delta)


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
    setattr(battler, attrName, value)


@Meta(DisplayName='LOC("DAMAGE_BATTLER")', DisplayDesc='LOC("DAMAGE_BATTLER_DESC")')
@ExecSplit(default=(None,))
def DamageBattler(battler: Any, amount: int) -> None:
    r"""\brief Subtract HP from any battler (floored at 0).

    - \param battler The target battler.
    - \param amount HP to remove.
    """
    if battler is None:
        return
    hp = getattr(battler, "HP", None)
    if hp is None:
        return
    battler.HP = max(0, int(hp) - int(amount))


@Meta(DisplayName='LOC("HEAL_BATTLER")', DisplayDesc='LOC("HEAL_BATTLER_DESC")')
@ExecSplit(default=(None,))
def HealBattler(battler: Any, amount: int) -> None:
    r"""\brief Restore HP on any battler (capped at MAXHP when present).

    - \param battler The target battler.
    - \param amount HP to restore.
    """
    if battler is None:
        return
    hp = getattr(battler, "HP", None)
    if hp is None:
        return
    cap = getattr(battler, "MAXHP", int(hp) + int(amount))
    battler.HP = min(int(hp) + int(amount), int(cap))


@Meta(DisplayName='LOC("BATTLER_HAS_STATE")', DisplayDesc='LOC("BATTLER_HAS_STATE_DESC")')
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


@Meta(DisplayName='LOC("ADD_STATE_TO")', DisplayDesc='LOC("ADD_STATE_TO_DESC")')
@ExecSplit(default=(None,))
def AddStateTo(battler: Any, stateID: str) -> None:
    r"""\brief Apply a state (by ID) to any battler, firing its onAdd blueprint.

    - \param battler The target battler.
    - \param stateID State identifier.
    """
    if battler is None or not hasattr(battler, "addState"):
        return
    battler.addState(stateID)


@Meta(DisplayName='LOC("REMOVE_STATE_FROM")', DisplayDesc='LOC("REMOVE_STATE_FROM_DESC")')
@ExecSplit(default=(None,))
def RemoveStateFrom(battler: Any, stateID: str) -> None:
    r"""\brief Remove a state (by ID) from any battler, firing its onRemove blueprint.

    - \param battler The target battler.
    - \param stateID State identifier.
    """
    if battler is None or not hasattr(battler, "removeState"):
        return
    battler.removeState(stateID)
