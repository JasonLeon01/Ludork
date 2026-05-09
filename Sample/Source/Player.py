# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Optional, Dict, Any, Tuple, Type, Union
from Engine import Texture, Input, Vector2u
from Engine.Gameplay.Actors import Character
from Global import Manager
from . import Data
from .Battler import Battler


class Player(Character, Battler):
    r"""\brief Player-controlled character with input bindings and battle stats.

    Combines `Character` (directional movement/animation) with `Battler`
    (HP, ATK, DEF, states). Registers arrow-key input mappings on construction.
    """

    LEVEL: int = 1  #: Current level
    HP: int = 0  #: Current hit points, initialized to `MAXHP`

    def __init__(self, texture: Optional[Texture] = None, tag: str = "") -> None:
        Character.__init__(self, texture, tag)
        Battler.__init__(self)
        self.tickable = True
        self.collisionEnabled = True
        self.animatable = True
        self.speed = 96
        self.HP = self.MAXHP
        self._items: Dict[str, int] = {}
        self._classPath: str = ""
        Input.registerActionMapping(
            self, "playerMoveUp", Input.getUpKeys(), lambda obj, delta: obj.MapMove((0, -1)), triggerOnHold=True
        )
        Input.registerActionMapping(
            self, "playerMoveDown", Input.getDownKeys(), lambda obj, delta: obj.MapMove((0, 1)), triggerOnHold=True
        )
        Input.registerActionMapping(
            self, "playerMoveLeft", Input.getLeftKeys(), lambda obj, delta: obj.MapMove((-1, 0)), triggerOnHold=True
        )
        Input.registerActionMapping(
            self, "playerMoveRight", Input.getRightKeys(), lambda obj, delta: obj.MapMove((1, 0)), triggerOnHold=True
        )

    def asDict(self) -> Dict[str, Any]:
        r"""
        \brief Serialize player information for serialization.

        - \return A dictionary containing player class path, tag, position, attributes, and inventory.
        """
        return {
            "playerClass": self._classPath,
            "tag": self.tag,
            "position": self.getMapPosition().unpack(),
            "attr": {k: getattr(self, k) for k in ["LEVEL", "HP", "MAXHP", "ATK", "DEF", "EXP", "GOLD"]},
            "items": self._items,
        }

    @staticmethod
    def InitPlayer(playerPath: str) -> Player:
        r"""
        \brief Initialize a player character from a class path.

        - \param playerPath  Path to the player class.

        - \return A new `Player` instance initialized with the provided class path.
        """
        actorClass: Type[Player] = Data.getClass(playerPath)
        texturePath = getattr(actorClass, "texturePath")
        defaultRect = getattr(actorClass, "defaultRect")
        actor: Player = Cast(
            Player, actorClass.GenActor(actorClass, Manager.loadCharacter(texturePath), defaultRect, "yongshi")
        )
        actor._classPath = playerPath
        actor.setAnimatable(True, True)
        actor.setCollisionEnabled(True)
        actor.setGraph(
            Data.genGraphFromData(
                Data.getClassData(playerPath)["graph"],
                actor,
                Data.getClass(playerPath),
            )
        )
        return actor

    @staticmethod
    def FromDict(data: Dict[str, Any]) -> Player:
        r"""
        \brief Deserialize player attributes and inventory from a dictionary.

        - \param data  A dictionary containing player attributes and inventory.

        - \return A new `Player` instance initialized with the provided data.
        """
        assert "playerClass" in data and "tag" in data and "position" in data and "attr" in data and "items" in data
        assert isinstance(data["playerClass"], str) and isinstance(data["tag"], str)
        AssertType(data["position"], Union[List[int], Tuple[int, int]])
        AssertType(data["attr"], Dict[str, Any])
        AssertType(data["items"], Dict[str, int])
        player = Player.initPlayer(data["playerClass"])
        player.tag = data["tag"]
        player.setMapPosition(Vector2u(*data["position"]))
        for k, v in data["attr"].items():
            setattr(player, k, v)

        player._items = data["items"]
        return player

    def addItem(self, itemID: str, count: int = 1) -> None:
        r"""\brief Add item(s) to the player's inventory.

        - `itemID` - Item identifier.
        - `count` - Number of items to add, default is 1.
        """
        if itemID in self._items:
            self._items[itemID] += count
        else:
            self._items[itemID] = count

    def removeItem(self, itemID: str, count: int = 1) -> bool:
        r"""\brief Remove item(s) from the player's inventory.

        - `itemID` - Item identifier.
        - `count` - Number of items to remove, default is 1.

        - \return `True` if removal succeeded, `False` otherwise.
        """
        if itemID not in self._items or self._items[itemID] < count:
            return False
        self._items[itemID] -= count
        if self._items[itemID] == 0:
            del self._items[itemID]
        return True

    def getItemCount(self, itemID: str) -> int:
        r"""\brief Get the count of a specific item in the player's inventory.

        - `itemID` - Item identifier.

        - \return Number of items owned, or 0 if not found.
        """
        return self._items.get(itemID, 0)

    def hasItem(self, itemID: str) -> bool:
        r"""\brief Check whether the player owns at least one of the specified item.

        - `itemID` - Item identifier.

        - \return `True` if the item is owned and count > 0, `False` otherwise.
        """
        return itemID in self._items and self._items[itemID] > 0
