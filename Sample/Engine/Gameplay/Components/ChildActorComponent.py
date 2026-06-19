# -*- encoding: utf-8 -*-

from __future__ import annotations
import builtins
from dataclasses import dataclass
from typing import Any, List, Optional, Tuple, TYPE_CHECKING

from .Component import Component

if TYPE_CHECKING:
    from Engine.Gameplay.Actors import Actor


@Meta(Vector2fVars=["relativePosition"])
@dataclass
class ChildActorComponent(Component):
    r"""\brief Spawn one child actor relative to the owning actor."""

    className: str = ""  #: Actor class path or generated blueprint class name
    relativePosition: Tuple[float, float] = (0.0, 0.0)  #: Spawn position relative to the parent actor

    def __post_init__(self) -> None:
        self._childActor: Optional[Actor] = None

    def init(self, owner: Actor) -> List[Any]:
        r"""
        \brief Spawn and attach the configured child actor.

        - \param owner Actor that owns this component.
        - \return The spawned child actor, or an empty list.
        """
        className = self.className.strip() if isinstance(self.className, str) else ""
        if not className:
            return []

        actorMap = owner.getMap()
        if actorMap is None:
            return []

        childActor = self._childActor
        if childActor is not None and not childActor.isDestroyed():
            return []

        from Source import Data

        childActor = Data.genActorFromClassName(className, self._makeChildTag(owner))
        if childActor is None:
            return []

        owner.addChild(childActor)
        childActor.setRelativePosition(self._normaliseRelativePosition())
        layer = actorMap.getActorLayer(owner)
        if layer is None:
            layer = "default"
        actorMap.spawnActor(childActor, layer, emitCreateEvent=False)
        self._childActor = childActor
        return [childActor]

    def _normaliseRelativePosition(self) -> Tuple[float, float]:
        value = self.relativePosition
        if isinstance(value, str):
            try:
                evaluator = getattr(builtins, "Eval", eval)
                value = evaluator(value)
            except Exception:
                value = (0.0, 0.0)
        if hasattr(value, "x") and hasattr(value, "y"):
            return (float(value.x), float(value.y))
        if isinstance(value, (list, tuple)) and len(value) >= 2:
            try:
                return (float(value[0]), float(value[1]))
            except (TypeError, ValueError):
                pass
        return (0.0, 0.0)

    @staticmethod
    def _makeChildTag(owner: Actor) -> str:
        parentTag = owner.getMapTag()
        if parentTag is None:
            parentTag = ""
        if not isinstance(parentTag, str):
            parentTag = str(parentTag)
        return f"{parentTag}_child"
