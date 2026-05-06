# -*- encoding: utf-8 -*-
"""GameInstance: persistent game state container surviving across scene transitions."""

from typing import Any, Dict, List, Optional, Type, cast
from Engine import Vector2u
from Engine.Gameplay.Actors import Actor
from Global import Manager
from . import Data
from .Player import Player


class GameInstance:
    def __init__(self) -> None:
        self._variables: Dict[str, Any] = {}
        self._player: Player = self._initPlayer()
        self._cachedMap: Optional[str] = None
        self._cachedDestroyedActors: Dict[str, List[str]] = {}

    def getVariables(self) -> Dict[str, Any]:
        return self._variables

    def getVariable(self, name: str) -> Any:
        if not name in self._variables:
            return None
        return self._variables[name]

    def setVariable(self, name: str, value: Any) -> None:
        self._variables[name] = value

    def getPlayer(self) -> Player:
        return self._player

    def applyMapInfo(self, mapPath: str, pos: Vector2u) -> None:
        if mapPath:
            self._cachedMap = mapPath
        self._player.setMapPosition(pos)

    def recordDestroyedActor(self, mapPath: str, actor: Actor) -> None:
        if not mapPath in self._cachedDestroyedActors:
            self._cachedDestroyedActors[mapPath] = []
        self._cachedDestroyedActors[mapPath].append(actor.tag)

    def getDestroyedActors(self, mapPath: str) -> List[str]:
        if not mapPath in self._cachedDestroyedActors:
            return []
        return self._cachedDestroyedActors[mapPath]

    def _initPlayer(self) -> Player:
        playerPath = "Data.Blueprints.Actors.BP_Actor_Braver"
        actorClass: Type[Player] = Data.getClass(playerPath)
        texturePath = getattr(actorClass, "texturePath")
        defaultRect = getattr(actorClass, "defaultRect")
        actor: Player = cast(
            Player, actorClass.GenActor(actorClass, Manager.loadCharacter(texturePath), defaultRect, "yongshi")
        )
        actor.setAnimatable(True, True)
        actor.setCollisionEnabled(True)
        actor.setPosition((608, 256))
        actor.setGraph(
            Data.genGraphFromData(
                Data.getClassData(playerPath)["graph"],
                actor,
                Data.getClass(playerPath),
            )
        )
        return actor
