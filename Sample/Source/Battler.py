# -*- encoding: utf-8 -*-

from enum import IntEnum
from dataclasses import dataclass
from typing import Any, Dict, List, Optional
from .States import StateInfo, State


class DamageType(IntEnum):
    NORMAL = 0
    UNDEFEATABLE = 1


@dataclass
class _BaseBattler:
    r"""\brief Base combat attributes for any battling entity."""

    MAXHP: int = 1000  #: Hit points
    ATK: int = 10  #: Attack power
    DEF: int = 10  #: Defense power
    EXP: int = 0  #: Experience points
    GOLD: int = 0  #: Currency


class Battler(_BaseBattler):
    r"""\brief Mixin providing combat stats and state management.

    Attach to any Actor via multiple inheritance to give it battle capabilities.
    Manages a list of active `State` objects that can modify behavior.
    """

    def __init__(self, attrs: Dict[str, Any] = {}) -> None:
        r"""\brief Construct a battler with optional attribute overrides.

        - \param attrs Optional dictionary of attribute overrides.
        """
        super().__init__(**attrs)
        self._states: List[State] = []  #: Active state effects

    @ReturnType(state=bool)
    def hasStateByInfo(self, stateInfo: StateInfo) -> Optional[State]:
        r"""\brief Find an active state matching the given info descriptor.

        - \param stateInfo The state info descriptor to search for.
        - \return The matching state, or None.
        """
        info = ["name", "icon", "description"]
        for s in self._states:
            if all([getattr(s, i) == getattr(stateInfo, i) for i in info]):
                return s
        return None

    @ReturnType(state=bool)
    def hasState(self, state: State) -> bool:
        r"""\brief Check whether a state (or one matching its info) is currently active.

        - \param state The state or state info to check.
        - \return True if the state is active.
        """
        if state in self._states:
            return True
        return not self.hasStateByInfo(state) is None

    @ExecSplit(default=(None,))
    def addState(self, state: State) -> None:
        r"""\brief Apply a state to this battler if not already present.

        - \param state The state to apply.
        """
        if not self.hasState(state):
            self._states.append(state)

    @ExecSplit(default=(None,))
    def removeStateByInfo(self, state: StateInfo) -> None:
        r"""\brief Remove an active state by its info descriptor.

        - \param state The state info to remove.
        """
        if self.hasState(state):
            self._states.remove(state)

    @ExecSplit(default=(None,))
    def removeState(self, state: State) -> None:
        r"""\brief Remove a specific state instance.

        - \param state The state to remove.
        """
        if self.hasState(state):
            self._states.remove(state)

    @ReturnType(state=State)
    def getState(self, state: StateInfo) -> Optional[State]:
        r"""\brief Get the active state matching the given info.

        - \param state The state info to look up.
        - \return The matching state, or None.
        """
        return self.hasStateByInfo(state)
