# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, TYPE_CHECKING
from Engine import RegisterEvent
from Engine.Gameplay.InfoBase import InfoBase
from Source.Configs.GeneralEnum import GeneralDataKey

if TYPE_CHECKING:
    from ..Battler import Battler


class StateInfo(InfoBase):
    r"""
    \brief State data + logic layer.

    A `StateInfo` is the data + blueprint container for a battler status effect
    (poisoned, burning, blessed, etc). Each active state is owned by exactly one
    `Battler` (the host). State blueprints expose walking behaviour and one
    developer-triggered hook; combat resolution is handled directly by the
    battle system.

    Defines state-related blueprint events:
        onWalk, onHookTriggered.
    Independent of Actor; can be used standalone in inventory/shop UI.
    """

    _infoType: str = GeneralDataKey.State

    def __init__(self) -> None:
        r"""\brief Construct a state info with no host yet."""
        super().__init__()
        self._owner: Optional[Battler] = None  #: The hosting battler (set by Battler.addState)
        self.stacks: int = 0  #: Stack count applied when the state was added

    def getStacks(self) -> int:
        r"""\brief Get the current stack count for this state.

        - \return Active stack count.
        """
        return self.stacks

    def getOwner(self) -> Optional[Battler]:
        r"""\brief Get the battler currently affected by this state.

        - \return The hosting `Battler` or None if not attached.
        """
        return self._owner

    def setOwner(self, owner: Optional[Battler]) -> None:
        r"""\brief Bind this state to a host battler.

        - \param owner The hosting `Battler` instance, or None to detach.
        """
        self._owner = owner

    @RegisterEvent
    def onWalk(self, battler: Battler = None) -> None:
        r"""\brief Blueprint event: called each step the affected battler takes.

        - \param battler The hosting battler.
        """
        pass

    @RegisterEvent
    def onHookTriggered(self, battler: Battler = None) -> None:
        r"""\brief Blueprint event: called when the hosting battler explicitly triggers its state hook.

        - \param battler The hosting battler.
        """
        pass
