# -*- encoding: utf-8 -*-
r"""\brief GameInstance: persistent game state container surviving across scene transitions."""

from __future__ import annotations
from typing import Any, Dict, List, Optional
from Engine import Vector2u
from Engine.Gameplay.Actors import Actor
from Source import System
from .Player import Player


class GameInstance:
    r"""\brief Persistent game state container that survives across scene transitions.

    Holds player data, game variables, cached map information,
    and destroyed actor tracking.
    """

    def __init__(self) -> None:
        r"""\brief Construct a new game instance with a default player."""
        self._players: List[Player] = []
        firstPlayer = Player.InitPlayer("Data.Blueprints.Actors.BP_Actor_Braver")
        firstPlayer.setMapPosition(System.getStartPos())
        self._players.append(firstPlayer)
        self._variables: Dict[str, Any] = {}
        self._cachedMap: Optional[str] = None
        self._cachedNewItem: Dict[str, bool] = {}
        self._cachedDestroyedActors: Dict[str, List[str]] = {}

    def asDict(self) -> Dict[str, Any]:
        r"""\brief Serialize the game instance to a dictionary.

        - \return A dictionary containing players, variables, map, and destroyed actors.
        """
        return {
            "players": [p.asDict() for p in self._players],
            "variables": self._variables,
            "map": Cast(str, self._cachedMap),
            "destroyedActors": self._cachedDestroyedActors,
        }

    @staticmethod
    def FromDict(data: Dict[str, Any]) -> GameInstance:
        r"""\brief Deserialize a game instance from a dictionary.

        - \param data The serialised game instance data.
        - \return A restored GameInstance.
        """
        assert "players" in data and "variables" in data and "map" in data and "destroyedActors" in data
        AssertType(data["players"], List[Dict[str, Any]])
        assert isinstance(data["map"], str)
        AssertType(data["destroyedActors"], Dict[str, List[str]])
        inst = GameInstance.__new__(GameInstance)
        inst._players = [Player.FromDict(p) for p in data["players"]]
        inst._variables = data["variables"]
        inst._cachedMap = data["map"]
        inst._cachedDestroyedActors = data["destroyedActors"]
        return inst

    def getVariables(self) -> Dict[str, Any]:
        r"""\brief Get all game variables.

        - \return A dictionary of all game variables.
        """
        return self._variables

    def getVariable(self, name: str) -> Any:
        r"""\brief Get a game variable by name.

        - \param name The variable name.
        - \return The variable value, or None if not found.
        """
        if not name in self._variables:
            return None
        return self._variables[name]

    def setVariable(self, name: str, value: Any) -> None:
        r"""\brief Set a game variable.

        - \param name The variable name.
        - \param value The value to set.
        """
        self._variables[name] = value

    def getPlayer(self) -> Player:
        r"""\brief Get the first (primary) player.

        - \return The primary player.
        """
        return self._players[0]

    def setPlayer(self, player: Player) -> None:
        r"""\brief Set the primary player.

        - \param player The player to set as primary.
        """
        if self._players:
            self._players[0] = player
        else:
            self._players.append(player)

    def getPlayers(self) -> List[Player]:
        r"""\brief Get all players.

        - \return A list of all players.
        """
        return self._players

    def getPlayerByIndex(self, index: int) -> Player:
        r"""\brief Get a player by index.

        - \param index The player index.
        - \return The player at the given index.
        """
        return self._players[index]

    def getPlayerByTag(self, tag: str) -> Optional[Player]:
        r"""\brief Find a player by tag.

        - \param tag The player tag to search for.
        - \return The matching player, or None.
        """
        for p in self._players:
            if p.tag == tag:
                return p
        return None

    def addPlayerByClass(self, playerClass: str) -> None:
        r"""\brief Add a new player by class path.

        - \param playerClass The class path for the player blueprint.
        """
        player = Player.InitPlayer(playerClass)
        self._players.append(player)

    def removePlayerByClass(self, playerClass: str) -> None:
        r"""\brief Remove a player by class path.

        - \param playerClass The class path to remove.
        """
        if len(self._players) <= 1:
            return
        for i, p in enumerate(self._players):
            if p._classPath == playerClass:
                self._players.pop(i)
                return

    def applyMapInfo(self, mapPath: str, pos: Optional[Vector2u] = None) -> None:
        r"""\brief Apply map information for scene transitions.

        - \param mapPath The new map path to cache.
        - \param pos The position to set the primary player to.
        """
        if mapPath:
            self._cachedMap = mapPath
        if pos:
            self._players[0].setMapPosition(pos)

    def recordDestroyedActor(self, mapPath: str, actor: Actor) -> None:
        r"""\brief Record a destroyed actor for persistence.

        - \param mapPath The map path where the actor was destroyed.
        - \param actor The destroyed actor.
        """
        if not mapPath in self._cachedDestroyedActors:
            self._cachedDestroyedActors[mapPath] = []
        self._cachedDestroyedActors[mapPath].append(actor.tag)

    def getDestroyedActors(self, mapPath: str) -> List[str]:
        r"""\brief Get destroyed actor tags for a map.

        - \param mapPath The map path.
        - \return A list of destroyed actor tags.
        """
        if not mapPath in self._cachedDestroyedActors:
            return []
        return self._cachedDestroyedActors[mapPath]

    def getCachedNewItem(self, itemID: str) -> bool:
        r"""\brief Get the cached new item status for a player.

        - \param itemID The item ID.
        - \return True if the item is new, False otherwise.
        """
        return self._cachedNewItem.get(itemID, False)

    def setCachedNewItem(self, itemID: str) -> None:
        r"""\brief Set the cached new item status for a player.

        - \param itemID The item ID.
        - \param isNew True if the item is new, False otherwise.
        """
        self._cachedNewItem[itemID] = True
