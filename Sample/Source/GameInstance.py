# -*- encoding: utf-8 -*-
"""GameInstance: persistent game state container surviving across scene transitions."""

from __future__ import annotations
from typing import Any, Dict, List, Optional
from Engine import Vector2u
from Engine.Gameplay.Actors import Actor
from .Player import Player


class GameInstance:
    def __init__(self) -> None:
        self._player: Player = Player.InitPlayer("Data.Blueprints.Actors.BP_Actor_Braver")
        self._player.setPosition((608, 256))
        self._variables: Dict[str, Any] = {}
        self._cachedMap: Optional[str] = None
        self._cachedDestroyedActors: Dict[str, List[str]] = {}

    def asDict(self) -> Dict[str, Any]:
        return {
            "player": self._player.asDict(),
            "variables": self._variables,
            "map": Cast(str, self._cachedMap),
            "destroyedActors": self._cachedDestroyedActors,
        }

    @staticmethod
    def FromDict(data: Dict[str, Any]) -> GameInstance:
        assert "player" in data and "variables" in data and "map" in data and "destroyedActors" in data
        AssertType(data["player"], Dict[str, Any])
        assert isinstance(data["map"], str)
        AssertType(data["destroyedActors"], Dict[str, List[str]])
        inst = GameInstance()
        inst.setPlayer(Player.FromDict(data["player"]))
        inst._cachedMap = data["map"]
        inst._cachedDestroyedActors = data["destroyedActors"]
        return inst

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

    def setPlayer(self, player: Player) -> None:
        self._player = player

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
