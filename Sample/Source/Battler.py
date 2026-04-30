# -*- encoding: utf-8 -*-

from dataclasses import dataclass
from typing import Any, Dict, List, Optional
from .States import StateInfo, State


@dataclass
class _BaseBattler:
    HP: int = 1000
    ATK: int = 10
    DEF: int = 10
    EXP: int = 0
    GOLD: int = 0


class Battler(_BaseBattler):
    def __init__(self, attrs: Dict[str, Any] = {}) -> None:
        super().__init__(**attrs)
        self._states: List[State] = []

    @ReturnType(state=bool)
    def hasStateByInfo(self, stateInfo: StateInfo) -> Optional[State]:
        info = ["name", "icon", "description"]
        for s in self._states:
            if all([getattr(s, i) == getattr(stateInfo, i) for i in info]):
                return s
        return None

    @ReturnType(state=bool)
    def hasState(self, state: State) -> bool:
        if state in self._states:
            return True
        return not self.hasStateByInfo(state) is None

    @ExecSplit(default=(None,))
    def addState(self, state: State) -> None:
        if not self.hasState(state):
            self._states.append(state)

    @ExecSplit(default=(None,))
    def removeStateByInfo(self, state: StateInfo) -> None:
        if self.hasState(state):
            self._states.remove(state)

    @ExecSplit(default=(None,))
    def removeState(self, state: State) -> None:
        if self.hasState(state):
            self._states.remove(state)

    @ReturnType(state=State)
    def getState(self, state: StateInfo) -> Optional[State]:
        return self.hasStateByInfo(state)
