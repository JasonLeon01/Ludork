# -*- encoding: utf-8 -*-

from dataclasses import dataclass
from typing import Any, Dict, List, Optional
from .States import StateInfo, State


@dataclass
class _BaseBattler:
    """Base combat attributes for any battling entity."""

    HP: int = 1000   #: Hit points
    ATK: int = 10    #: Attack power
    DEF: int = 10    #: Defense power
    EXP: int = 0     #: Experience points
    GOLD: int = 0    #: Currency


class Battler(_BaseBattler):
    """Mixin providing combat stats and state management.

    Attach to any Actor via multiple inheritance to give it battle capabilities.
    Manages a list of active `State` objects that can modify behavior.
    """

    def __init__(self, attrs: Dict[str, Any] = {}) -> None:
        super().__init__(**attrs)
        self._states: List[State] = []  #: Active state effects

    @ReturnType(state=bool)
    def hasStateByInfo(self, stateInfo: StateInfo) -> Optional[State]:
        """Find an active state matching the given info descriptor, or `None`."""
        info = ["name", "icon", "description"]
        for s in self._states:
            if all([getattr(s, i) == getattr(stateInfo, i) for i in info]):
                return s
        return None

    @ReturnType(state=bool)
    def hasState(self, state: State) -> bool:
        """Check whether a state (or one matching its info) is currently active."""
        if state in self._states:
            return True
        return not self.hasStateByInfo(state) is None

    @ExecSplit(default=(None,))
    def addState(self, state: State) -> None:
        """Apply a state to this battler if not already present."""
        if not self.hasState(state):
            self._states.append(state)

    @ExecSplit(default=(None,))
    def removeStateByInfo(self, state: StateInfo) -> None:
        """Remove an active state by its info descriptor."""
        if self.hasState(state):
            self._states.remove(state)

    @ExecSplit(default=(None,))
    def removeState(self, state: State) -> None:
        """Remove a specific state instance."""
        if self.hasState(state):
            self._states.remove(state)

    @ReturnType(state=State)
    def getState(self, state: StateInfo) -> Optional[State]:
        """Get the active state matching the given info, or `None`."""
        return self.hasStateByInfo(state)
