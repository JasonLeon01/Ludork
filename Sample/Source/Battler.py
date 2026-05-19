# -*- encoding: utf-8 -*-

from __future__ import annotations
from enum import IntEnum
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple, Union
from . import Data
from .Infos import StateInfo


class DamageType(IntEnum):
    NORMAL = 0
    UNDEFEATABLE = 1


@dataclass
class DamageContext:
    r"""\brief Mutable context shared with state blueprints during damage resolution.

    State graphs may read/modify these fields in onBeforeAttack/onAfterAttack/
    onBeforeDefense/onAfterDefense/onResolveDamage to alter the final damage
    outcome (incl. damage-over-time effects such as poison).
    """

    attacker: Optional[Battler] = None  #: The battler dealing damage
    defender: Optional[Battler] = None  #: The battler receiving damage
    atk: int = 0  #: Effective ATK after modifiers
    deF: int = 0  #: Effective DEF after modifiers
    damagePerRound: int = 0  #: Damage per round dealt to the defender
    rounds: int = 0  #: Number of rounds it takes the attacker to fell the defender
    totalDamage: int = 0  #: Final accumulated damage on the attacker side
    extra: Dict[str, Any] = field(default_factory=dict)  #: Free-form payload for blueprints


@dataclass
class _BaseBattler:
    r"""\brief Base combat attributes for any battling entity."""

    MAXHP: int = 1000  #: Hit points
    ATK: int = 10  #: Attack power
    DEF: int = 10  #: Defense power
    EXP: int = 0  #: Experience points
    GOLD: int = 0  #: Currency
    ANIMATION_KEY: str = ""  #: Animation key for this battler


class Battler(_BaseBattler):
    r"""\brief Mixin providing combat stats and state management.

    Attach to any Actor via multiple inheritance to give it battle capabilities.
    Manages a list of active `StateInfo` objects whose blueprint events drive
    extra behaviour during combat.
    """

    HP: int = 0  #: Current hit points (declared so all battlers expose it)

    def __init__(self, attrs: Dict[str, Any] = {}) -> None:
        r"""\brief Construct a battler with optional attribute overrides.

        - \param attrs Optional dictionary of attribute overrides.
        """
        super().__init__(**attrs)
        self._states: List[StateInfo] = []  #: Active state effects

    def hasState(self, state: Union[str, StateInfo]) -> bool:
        r"""\brief Check whether a state is currently active.

        - \param state State ID string or `StateInfo` instance.
        - \return True if the state is active.
        """
        return self.getStateByID(self._resolveStateID(state)) is not None

    def getStateByID(self, stateID: str) -> Optional[StateInfo]:
        r"""\brief Return the active `StateInfo` matching the given ID.

        - \param stateID State identifier.
        - \return The matching state, or None.
        """
        if not stateID:
            return None
        for s in self._states:
            if s.ID == stateID:
                return s
        return None

    def getStates(self) -> List[StateInfo]:
        r"""\brief Get a shallow copy of all active states.

        - \return List of currently active `StateInfo` instances.
        """
        return list(self._states)

    def getStateIDs(self) -> List[str]:
        r"""\brief Get the IDs of all active states (for serialization).

        - \return List of state ID strings.
        """
        return [s.ID for s in self._states]

    @ExecSplit(default=(None,))
    def addState(self, state: Union[str, StateInfo]) -> None:
        r"""\brief Apply a state to this battler if not already present.

        Accepts either a state ID (string) or a pre-built `StateInfo`.
        When given an ID the corresponding `StateInfo` is built from
        GeneralData and `onAdd(battler=self)` is fired on it.

        - \param state State ID string or `StateInfo` instance.
        """
        info = self._buildStateInfo(state)
        if info is None:
            return
        if self.getStateByID(info.ID) is not None:
            return
        info.setOwner(self)
        self._states.append(info)
        info.triggerEvent("onAdd", battler=self)

    @ExecSplit(default=(None,))
    def removeState(self, state: Union[str, StateInfo]) -> None:
        r"""\brief Remove an active state by ID or instance.

        Fires `onRemove(battler=self)` on the removed state.

        - \param state State ID string or `StateInfo` instance.
        """
        existing = self.getStateByID(self._resolveStateID(state))
        if existing is None:
            return
        existing.triggerEvent("onRemove", battler=self)
        existing.setOwner(None)
        self._states.remove(existing)

    def clearStates(self) -> None:
        r"""\brief Remove all active states, firing `onRemove` for each."""
        for s in list(self._states):
            s.triggerEvent("onRemove", battler=self)
            s.setOwner(None)
        self._states.clear()

    def setStateIDs(self, stateIDs: List[str]) -> None:
        r"""\brief Replace active states by ID list (used during load).

        Existing states are cleared first; `onAdd` is fired for each new one.

        - \param stateIDs List of state ID strings.
        """
        self.clearStates()
        for sid in stateIDs or []:
            self.addState(sid)

    def triggerStateEvent(self, eventName: str, **kwargs) -> None:
        r"""\brief Forward a blueprint event to every active state.

        The hosting battler is always injected as `battler=self`; callers may
        pass extra keyword arguments which are forwarded to each state.

        - \param eventName Name of the event registered on `StateInfo`.
        - \param kwargs Extra keyword arguments delivered to the event.
        """
        for s in list(self._states):
            s.triggerEvent(eventName, battler=self, **kwargs)

    def getDamagePerRound(self, attacker: Battler, defender: Battler) -> DamageContext:
        r"""\brief Build a `DamageContext` for one attacker->defender exchange.

        Fires `onBeforeAttack` on the attacker's states and `onBeforeDefense`
        on the defender's states (they may mutate atk/deF), computes
        damagePerRound, then fires `onAfterAttack`/`onAfterDefense` so states
        may further mutate damagePerRound (crit, shield, etc).

        - \param attacker The battler dealing damage.
        - \param defender The battler receiving damage.
        - \return The populated `DamageContext` for this exchange.
        """
        ctx = DamageContext(
            attacker=attacker,
            defender=defender,
            atk=attacker.ATK,
            deF=defender.DEF,
        )
        attacker.triggerStateEvent("onTurnStart")
        defender.triggerStateEvent("onTurnStart")
        attacker.triggerStateEvent("onBeforeAttack", context=ctx)
        defender.triggerStateEvent("onBeforeDefense", context=ctx)
        ctx.damagePerRound = max(0, ctx.atk - ctx.deF)
        attacker.triggerStateEvent("onAfterAttack", context=ctx)
        defender.triggerStateEvent("onAfterDefense", context=ctx)
        ctx.damagePerRound = max(0, ctx.damagePerRound)
        attacker.triggerStateEvent("onTurnEnd")
        defender.triggerStateEvent("onTurnEnd")
        return ctx

    def getDamage(self, battler: Battler) -> Tuple[DamageType, int]:
        r"""\brief Calculate accumulated damage taken by `battler` if it fights `self`.

        Models a round-by-round duel where `battler` is the attacker and
        `self` is the defender; returns how much damage `battler` ultimately
        suffers from `self`'s counter-attacks before defeating `self`.

        Pipeline (states can hook every stage):
            1. attackCtx  = battler.getDamagePerRound(battler, self)
            2. counterCtx = self.getDamagePerRound(self, battler)
            3. rounds = ceil(self.MAXHP / attackCtx.damagePerRound)
            4. totalDamage = rounds * counterCtx.damagePerRound
            5. fire onResolveDamage on both sides (poison/DoT can modify totalDamage)

        - \param battler The opposing battler (the attacker).
        - \return Tuple of (DamageType, accumulated damage on `battler`).
        """
        attackCtx = battler.getDamagePerRound(battler, self)
        counterCtx = self.getDamagePerRound(self, battler)
        if attackCtx.damagePerRound <= 0:
            return (DamageType.UNDEFEATABLE, -1)
        rounds = max(1, (self.MAXHP + attackCtx.damagePerRound - 1) // attackCtx.damagePerRound)
        attackCtx.rounds = rounds
        counterCtx.rounds = rounds
        counterCtx.totalDamage = rounds * counterCtx.damagePerRound
        attackCtx.totalDamage = counterCtx.totalDamage
        battler.triggerStateEvent("onResolveDamage", context=counterCtx)
        self.triggerStateEvent("onResolveDamage", context=counterCtx)
        return (DamageType.NORMAL, max(0, counterCtx.totalDamage))

    @staticmethod
    def _resolveStateID(state: Union[str, StateInfo, None]) -> str:
        if state is None:
            return ""
        if isinstance(state, str):
            return state
        return getattr(state, "ID", "")

    @staticmethod
    def _buildStateInfo(state: Union[str, StateInfo, None]) -> Optional[StateInfo]:
        if state is None:
            return None
        if isinstance(state, StateInfo):
            if not getattr(state, "_infoGraph", None) and state.ID:
                state.initInfo(Data)
            return state
        if isinstance(state, str) and state:
            members = Data.getGeneralData("State").get("members", {})
            if state not in members:
                return None
            info = StateInfo()
            info.ID = state
            info.initInfo(Data)
            return info
        return None
