# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, List, TYPE_CHECKING

if TYPE_CHECKING:
    from Engine.Gameplay.Actors import Actor


class Component:
    r"""\brief Base marker for editor-editable actor components."""

    def init(self, owner: Actor) -> List[Any]:
        r"""
        \brief Initialise this component after its owner actor is created.

        - \param owner Actor that owns this component.
        - \return Actors spawned by this component.
        """
        return []
