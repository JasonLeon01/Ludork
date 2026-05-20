# -*- encoding: utf-8 -*-
r"""\brief GameInstance: persistent game state container surviving across scene transitions."""

from __future__ import annotations
from typing import Any, Dict, List, Optional, Tuple
from Engine import Vector2u
from Engine.Gameplay.Actors import Actor
from Source import System
from .Player import Player


class GameInstance:
    r"""\brief Persistent game state container that survives across scene transitions.

    Holds player data, game variables, cached map information,
    and destroyed actor tracking.
    """

    REGION_DICT: Dict[str, Dict[str, str]] = {"Mota": {"Map_01": "魔塔 1 层", "Map_02": "魔塔 2 层"}}

    def __init__(self) -> None:
        r"""\brief Construct a new game instance with a default player."""
        self._players: List[Player] = []
        firstPlayer = Player.InitPlayer("Data.Blueprints.Actors.BP_Actor_Braver")
        firstPlayer.setMapPosition(System.getStartPos())
        self._currentRegion = "Mota"
        self._players.append(firstPlayer)
        self._variables: Dict[str, Any] = {}
        self._cachedMap: Optional[str] = None
        self._cachedNewItem: Dict[str, bool] = {}
        self._cachedDestroyedActors: Dict[str, List[str]] = {}
        self._cachedTelepoints: Dict[str, List[Tuple[int, int]]] = {}
        self._screenshot: Optional[List[int]] = None

    def asDict(self) -> Dict[str, Any]:
        r"""\brief Serialize the game instance to a dictionary.

        - \return A dictionary containing players, variables, map, destroyed actors, and screenshot.
        """
        return {
            "region": self._currentRegion,
            "players": [p.asDict() for p in self._players],
            "variables": self._variables,
            "map": Cast(str, self._cachedMap),
            "destroyedActors": self._cachedDestroyedActors,
            "telepoints": self._cachedTelepoints,
            "screenshot": self._screenshot,
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
        inst._currentRegion = data["region"]
        inst._players = [Player.FromDict(p) for p in data["players"]]
        inst._variables = data["variables"]
        inst._cachedMap = data["map"]
        inst._cachedDestroyedActors = GameInstance._normaliseDestroyedActors(data["destroyedActors"])
        inst._cachedNewItem = {}
        inst._cachedTelepoints = GameInstance._normaliseTelepoints(data.get("telepoints", {}))
        inst._screenshot = data.get("screenshot")
        return inst

    def getCurrentRegion(self) -> str:
        r"""\brief Get the current region.

        - \return The current region.
        """
        return self._currentRegion

    def setCurrentRegion(self, region: str) -> None:
        r"""\brief Set the current region.

        - \param region The region to set.
        """
        self._currentRegion = region

    def setScreenshot(self, screenshot: Optional[List[int]]) -> None:
        r"""\brief Set the captured screenshot bytes used for save thumbnails.

        - \param screenshot Encoded image bytes (PNG) or None to clear.
        """
        self._screenshot = screenshot

    def getScreenshot(self) -> Optional[List[int]]:
        r"""\brief Get the captured screenshot bytes.

        - \return Encoded image bytes (PNG) or None.
        """
        return self._screenshot

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
        mapPath = self._normaliseMapPath(mapPath)
        actorTag = actor.tag
        if not actorTag:
            return
        if not mapPath in self._cachedDestroyedActors:
            self._cachedDestroyedActors[mapPath] = []
        if actorTag not in self._cachedDestroyedActors[mapPath]:
            self._cachedDestroyedActors[mapPath].append(actorTag)

    def getDestroyedActors(self, mapPath: str) -> List[str]:
        r"""\brief Get destroyed actor tags for a map.

        - \param mapPath The map path.
        - \return A list of destroyed actor tags.
        """
        mapPath = self._normaliseMapPath(mapPath)
        if not mapPath in self._cachedDestroyedActors:
            return []
        return self._cachedDestroyedActors[mapPath]

    def recordTelepoint(self, mapPath: str, telepoint: Vector2u) -> None:
        r"""\brief Record a telepoint for persistence.

        - \param mapPath The map path where the telepoint is located.
        - \param telepoint The telepoint position.
        """
        mapPath = self._normaliseMapPath(mapPath)
        if not mapPath in self._cachedTelepoints:
            self._cachedTelepoints[mapPath] = []
        telepointValue = telepoint.unpack()
        if telepointValue not in self._cachedTelepoints[mapPath]:
            self._cachedTelepoints[mapPath].append(telepointValue)

    def getTelepoints(self, mapPath: str) -> List[Tuple[int, int]]:
        r"""\brief Get telepoint positions for a map.

        - \param mapPath The map path.
        - \return A list of telepoint positions.
        """
        mapPath = self._normaliseMapPath(mapPath)
        if not mapPath in self._cachedTelepoints:
            return []
        return self._cachedTelepoints[mapPath]

    @staticmethod
    def _normaliseDestroyedActors(destroyedActors: Dict[str, List[str]]) -> Dict[str, List[str]]:
        result: Dict[str, List[str]] = {}
        for mapPath, actorTags in destroyedActors.items():
            normalisedMapPath = GameInstance._normaliseMapPath(mapPath)
            if normalisedMapPath not in result:
                result[normalisedMapPath] = []
            for actorTag in actorTags:
                if actorTag and actorTag not in result[normalisedMapPath]:
                    result[normalisedMapPath].append(actorTag)
        return result

    @staticmethod
    def _normaliseTelepoints(telepoints: Dict[str, List[Any]]) -> Dict[str, List[Tuple[int, int]]]:
        result: Dict[str, List[Tuple[int, int]]] = {}
        for mapPath, points in telepoints.items():
            normalisedMapPath = GameInstance._normaliseMapPath(mapPath)
            if normalisedMapPath not in result:
                result[normalisedMapPath] = []
            for point in points:
                if isinstance(point, Vector2u):
                    telepointValue = point.unpack()
                elif isinstance(point, (tuple, list)) and len(point) >= 2:
                    telepointValue = (int(point[0]), int(point[1]))
                else:
                    continue
                if telepointValue not in result[normalisedMapPath]:
                    result[normalisedMapPath].append(telepointValue)
        return result

    @staticmethod
    def _normaliseMapPath(mapPath: str) -> str:
        path = mapPath.replace("\\", "/")
        while path.startswith("./"):
            path = path[2:]
        marker = "Data/Maps/"
        markerIndex = path.find(marker)
        if markerIndex != -1:
            path = path[markerIndex + len(marker) :]
        return path

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
