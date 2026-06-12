# -*- encoding: utf-8 -*-

from __future__ import annotations
from enum import IntEnum
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple, Union
from Engine.Gameplay.Components import Component, componentFromData, setComponentFieldValue
from . import Data
from .Infos import StateInfo


class DamageType(IntEnum):
    NORMAL = 0
    UNDEFEATABLE = 1


@dataclass
class BattlerInfoComponent(Component):
    r"""\brief Editable battle attributes for any battling entity."""

    MAXHP: int = 1000  #: Hit points
    ATK: int = 10  #: Attack power
    DEF: int = 10  #: Defense power
    EXP: int = 0  #: Experience points
    GOLD: int = 0  #: Currency
    ANIMATION_KEY: str = ""  #: Animation key for this battler
    HP: int = 0  #: Current hit points


@dataclass
class PlayerInfoComponent(BattlerInfoComponent):
    r"""\brief Editable player identity and battle attributes."""

    LEVEL: int = 1  #: Current level
    CLASS: str = ""  #: Current class


@dataclass
class EnemyInfoComponent(BattlerInfoComponent):
    r"""\brief Editable enemy battle attributes."""

    name: str = ""
    desc: str = ""
    special: Dict[str, Any] = field(default_factory=dict)
    drops: List[str] = field(default_factory=list)
    MAXHP: int = -1
    ATK: int = -1
    DEF: int = -1
    EXP: int = -1
    GOLD: int = -1
    HP: int = -1

    def __post_init__(self) -> None:
        self._initAttrs: Dict[str, Any] = {}

    def setInitAttrs(self, attrs: Dict[str, Any]) -> None:
        r"""
        \brief Store actor-instance attribute overrides for component initialisation.

        - \param attrs Attribute values that should be applied after actor creation.
        """
        self._initAttrs = dict(attrs)

    def init(self, owner: Any) -> List[Any]:
        r"""
        \brief Apply stored enemy attribute overrides during component initialisation.

        - \param owner Actor that owns this component.
        - \return No spawned actors.
        """
        initAttrs = self._initAttrs
        if isinstance(initAttrs, dict):
            for attr, value in initAttrs.items():
                if hasattr(self, attr):
                    setattr(self, attr, value)
        return []


class Battler:
    r"""\brief Mixin providing combat stats and state management.

    Attach to any Actor via multiple inheritance to give it battle capabilities.
    Manages a list of active `StateInfo` objects whose blueprint events can
    drive non-combat behaviours such as walking effects and explicit hooks.
    """

    _componentTypes = {"infoComp": BattlerInfoComponent}
    infoComp: BattlerInfoComponent = BattlerInfoComponent()

    def __init__(self, attrs: Optional[Dict[str, Any]] = None) -> None:
        r"""\brief Construct a battler with optional attribute overrides.

        - \param attrs Optional dictionary of attribute overrides.
        """
        self._normaliseInfoComp()
        if attrs:
            for key, value in attrs.items():
                if not setComponentFieldValue(self, key, value):
                    setattr(self, key, value)
        self._states: List[StateInfo] = []  #: Active state effects

    def _normaliseInfoComp(self) -> None:
        value = self.infoComp
        componentType = self._getInfoCompType()
        if "infoComp" not in self.__dict__ or not isinstance(value, componentType):
            self.infoComp = componentFromData(componentType, value)

    def _getInfoCompType(self) -> type[BattlerInfoComponent]:
        componentTypes = type(self)._componentTypes
        componentType = componentTypes.get("infoComp", BattlerInfoComponent)
        if not isinstance(componentType, type) or not issubclass(componentType, BattlerInfoComponent):
            return BattlerInfoComponent
        return componentType

    def _getInfoField(self, key: str, default: Any) -> Any:
        self._normaliseInfoComp()
        return getattr(self.infoComp, key, default)

    def _setInfoField(self, key: str, value: Any) -> None:
        self._normaliseInfoComp()
        if hasattr(self.infoComp, key):
            setattr(self.infoComp, key, value)

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

    def getStateStacks(self) -> Dict[str, int]:
        r"""\brief Get active state IDs mapped to stack counts (for serialization).

        - \return Dictionary of state ID to stack count.
        """
        return {s.ID: s.stacks for s in self._states}

    def _getStateStackCount(self, stateID: str) -> int:
        state = self.getStateByID(stateID)
        if state is None:
            return 0
        return state.stacks

    def hasSpecial(self, specialID: str) -> bool:
        r"""\brief Check whether this battler has the given special flag.

        - \param specialID Special identifier.
        - \return True if the battler's info component contains the special.
        """
        self._normaliseInfoComp()
        special = getattr(self.infoComp, "special", None)
        if not isinstance(special, dict):
            return False
        return specialID in special

    def _getSpecialIntValue(self, specialID: str, default: int = 0, minValue: int = 0) -> int:
        self._normaliseInfoComp()
        special = getattr(self.infoComp, "special", None)
        if not isinstance(special, dict) or specialID not in special:
            return max(minValue, default)
        value = special.get(specialID, default)
        if isinstance(value, str):
            try:
                value = Eval(value)
            except Exception:
                value = default
        try:
            return max(minValue, int(value))
        except (TypeError, ValueError):
            return max(minValue, default)

    def getATK(self, opponent: Optional[Battler] = None) -> int:
        r"""\brief Get the battler's effective attack value.

        - \param opponent Optional opposing battler used for special calculations.
        - \return Current attack value.
        """
        self._normaliseInfoComp()
        attackerAtk = int(self.infoComp.ATK)
        if self.hasState("Weak"):
            attackerAtk = max(0, attackerAtk - 2 * self._getStateStackCount("Weak"))
        if opponent is not None and self.hasSpecial("Compete"):
            attackerAtk = max(attackerAtk, opponent.getATK())
        return attackerAtk

    def getDEF(self, attacker: Optional[Battler] = None) -> int:
        r"""\brief Get the battler's effective defense value.

        - \param attacker Optional opposing battler used for special calculations.
        - \return Current defense value.
        """
        self._normaliseInfoComp()
        defenderDef = int(self.infoComp.DEF)
        if self.hasState("Weak"):
            defenderDef = max(0, defenderDef - 2 * self._getStateStackCount("Weak"))
        if attacker is not None and self.hasSpecial("Hard"):
            defenderDef = max(defenderDef, attacker.getATK(self) - 1)
        return defenderDef

    def getStateNames(self) -> List[str]:
        r"""\brief Get the names of all active states.

        - \return List of state names.
        """
        return [LOC(s.name) for s in self._states]

    @ExecSplit(default=(None,))
    def addState(self, state: Union[str, StateInfo], stacks: int) -> None:
        r"""\brief Apply a state to this battler with the given stack count.

        Accepts either a state ID (string) or a pre-built `StateInfo`.
        When given an ID the corresponding `StateInfo` is built from
        GeneralData. If the state is already active and its GeneralData
        marks it as stackable, the stack count is increased instead.

        - \param state State ID string or `StateInfo` instance.
        - \param stacks Stack count to apply or add.
        """
        info = self._buildStateInfo(state)
        if info is None:
            return
        stackCount = max(0, int(stacks))
        if stackCount <= 0:
            return
        existing = self.getStateByID(info.ID)
        if existing is not None:
            stateData = Data.getGeneralStateData(info.ID)
            if stateData.get("stackable"):
                existing.stacks += stackCount
            return
        info.stacks = stackCount
        info.setOwner(self)
        self._states.append(info)

    @ExecSplit(default=(None,))
    def removeState(self, state: Union[str, StateInfo]) -> None:
        r"""\brief Remove an active state by ID or instance.

        - \param state State ID string or `StateInfo` instance.
        """
        existing = self.getStateByID(self._resolveStateID(state))
        if existing is None:
            return
        existing.setOwner(None)
        self._states.remove(existing)

    def clearStates(self) -> None:
        r"""\brief Remove all active states."""
        for s in list(self._states):
            s.setOwner(None)
        self._states.clear()

    def setStateIDs(self, stateIDs: List[str]) -> None:
        r"""\brief Replace active states by ID list (used during load).

        Existing states are cleared first. Each state receives one stack.

        - \param stateIDs List of state ID strings.
        """
        self.clearStates()
        for sid in stateIDs or []:
            self.addState(sid, 1)

    def setStateStacks(self, stateStacks: Dict[str, int]) -> None:
        r"""\brief Replace active states from an ID-to-stacks map (used during load).

        Existing states are cleared first.

        - \param stateStacks Mapping of state ID to stack count.
        """
        self.clearStates()
        for sid, stacks in (stateStacks or {}).items():
            self.addState(sid, stacks)

    def _triggerStateEvent(self, eventName: str, **kwargs) -> None:
        for s in list(self._states):
            s.triggerEvent(eventName, battler=self, **kwargs)

    @ExecSplit(default=(None,))
    def triggerStateWalk(self) -> None:
        r"""\brief Trigger the walking event on every active state."""
        self._triggerStateEvent("onWalk")

    @ExecSplit(default=(None,))
    def triggerStateHook(self, stateKey: str) -> None:
        r"""\brief Trigger the developer-controlled hook event on one active state.

        - \param stateKey State ID to trigger.
        """
        state = self.getStateByID(stateKey)
        if state is None:
            return
        state.triggerEvent("onHookTriggered", battler=self)

    def getDamagePerRound(self, defender: Battler) -> int:
        r"""\brief Calculate one attacker-to-defender exchange.

        Combat damage no longer invokes state blueprint events.

        - \param attacker The battler dealing damage.
        - \param defender The battler receiving damage.
        - \return Damage per round dealt to the defender.
        """
        self._normaliseInfoComp()
        defender._normaliseInfoComp()
        attackerAtk = self.getATK(defender)
        defenderDef = defender.getDEF(self)

        if self.hasSpecial("Magic"):
            defenderDef = 0

        hitCount = self._getSpecialIntValue("MultiHit", 1, 1)
        basicDamage = max(0, attackerAtk - defenderDef) * hitCount
        if defender.hasState("Poisoned"):
            basicDamage += 10 * defender._getStateStackCount("Poisoned")
        return basicDamage

    def getDamage(self, battler: Battler) -> Tuple[DamageType, int]:
        r"""\brief Calculate accumulated damage taken by `battler` if it fights `self`.

        Models a round-by-round duel where `battler` is the attacker and
        `self` is the defender; returns how much damage `battler` ultimately
        suffers from `self`'s counter-attacks before defeating `self`.

        Pipeline:
            1. attackDamage  = battler.getDamagePerRound(self)
            2. counterDamage = self.getDamagePerRound(battler)
            3. counterRounds = self.infoComp.MAXHP // attackDamage
            4. totalDamage = counterRounds * counterDamage

        - \param battler The opposing battler (the attacker).
        - \return Tuple of (DamageType, accumulated damage on `battler`).
        """
        self._normaliseInfoComp()
        battler._normaliseInfoComp()
        attackDamage = battler.getDamagePerRound(self)
        counterDamage = self.getDamagePerRound(battler)
        if attackDamage <= 0:
            return (DamageType.UNDEFEATABLE, -1)
        counterRounds = max(0, int(self.infoComp.MAXHP) // attackDamage)
        return (DamageType.NORMAL, max(0, counterRounds * counterDamage))

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
            stateData = Data.getGeneralStateData(state)
            if not stateData:
                return None
            info = StateInfo()
            info.ID = state
            info.initInfo(Data)
            return info
        return None
