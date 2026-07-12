# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Optional, Dict, Any, Tuple, Type, Union
from Engine import Pair, Texture, Input, Vector2i, Vector2u, RegisterEvent
from Engine.Gameplay.Components import setComponentFieldValue
from Engine.Gameplay.Actors import Character
from Engine.Utils.Monitor import monitor, _MISSING
from Global import Manager
from . import Data
from .Battler import Battler, PlayerInfoComponent
from .Configs.GeneralEnum import GeneralDataKey
from .Infos.EquipInfo import EquipInfo
from .Infos.PlayerInfo import PlayerInfo

_LEVEL_HP_GAIN = 400
_LEVEL_ATK_GAIN = 2
_LEVEL_DEF_GAIN = 2


@Meta(GeneralDataVars=[("ID", GeneralDataKey.Player)])
class Player(Character, PlayerInfo, Battler):
    r"""\brief Player-controlled character with input bindings and battle stats.

    Combines `Character` (directional movement/animation) with `Battler`
    (HP, ATK, DEF, states). Keyboard movement is polled during onFixedTick.
    """

    ID: str = "FILL_IT_BY_YOURSELF"
    tickable: bool = True
    collisionEnabled: bool = True
    animatable: bool = True
    speed: float = 96.0
    _componentTypes = {"infoComp": PlayerInfoComponent}
    infoComp: PlayerInfoComponent = PlayerInfoComponent()

    def __init__(self, texture: Optional[Texture] = None, tag: str = "") -> None:
        Character.__init__(self, texture, tag)
        Battler.__init__(self)
        self._loading = True
        self.initInfo(Data)
        self._syncInitialHP()
        self._loading = False

        def _onLevelChange(old, new):
            delta = new - (old if old is not _MISSING else 0)
            self.infoComp.HP += delta * _LEVEL_HP_GAIN
            self.infoComp.ATK += delta * _LEVEL_ATK_GAIN
            self.infoComp.DEF += delta * _LEVEL_DEF_GAIN

        monitor(self.infoComp, "LEVEL", _onLevelChange, [])
        self._items: Dict[str, int] = {}
        self._equips: Dict[str, int] = {}
        self._equipInfo: Dict[str, str] = {}
        self._classPath: str = ""
        self._forbiddenMoving: bool = False
        self._wasMovingOnLastFixedTick: bool = False
        self._movementSpecialPath: List[Vector2i] = []
        for slot, equipID in Cast(dict, Data.getGeneralClassData(self.infoComp.CLASS).get("slot")).items():
            if equipID:
                self.equip(equipID)

    def _onArrivedAtMapCell(self) -> None:
        pos = self.getMapPosition()
        self._movementSpecialPath.append(Vector2i(int(pos.x), int(pos.y)))

    def consumeMovementSpecialPath(self) -> List[Vector2i]:
        r"""\brief Take and clear the map cells arrived at during the current move.

        - \return Arrived map cells in order, excluding the movement start cell.
        """
        path = self._movementSpecialPath
        self._movementSpecialPath = []
        return path

    def getClassPath(self) -> str:
        r"""\brief Get the blueprint class path used to create this player.

        - \return Player class path string.
        """
        return self._classPath

    def setClassPath(self, classPath: str) -> None:
        r"""\brief Set the blueprint class path for this player.

        - \param classPath Player class path string.
        """
        self._classPath = classPath

    @RegisterEvent
    def onFixedTick(self, fixedDelta: float) -> None:
        if self._movementSpecialPath:
            from Source.MovementSpecials import notifyPlayerMovementFinished

            notifyPlayerMovementFinished(self, self.consumeMovementSpecialPath())
        if self._wasMovingOnLastFixedTick and not self.isMoving():
            self.triggerStateWalk()
        self._wasMovingOnLastFixedTick = self.isMoving()

    def _getContinueMoveOffset(self) -> Optional[Tuple[int, int]]:
        if self.isInRoute():
            return None
        if self.getForbiddenMoving() or self._isSceneInputBlocked():
            return None
        return self._getHeldKeyboardMoveOffset()

    def _getHeldKeyboardMoveOffset(self) -> Optional[Tuple[int, int]]:
        heldMoves = (
            (Input.getUpKeys(), (0, -1)),
            (Input.getDownKeys(), (0, 1)),
            (Input.getLeftKeys(), (-1, 0)),
            (Input.getRightKeys(), (1, 0)),
        )
        for keys, offset in heldMoves:
            if Input.isActionHeld(keys):
                return offset
        return None

    def asDict(self) -> Dict[str, Any]:
        r"""
        \brief Serialize player information for serialization.

        - \return A dictionary containing player class path, tag, position, attributes, and inventory.
        """
        return {
            "playerClass": self._classPath,
            "tag": self.tag,
            "position": self.getMapPosition().unpack(),
            "attr": {
                "LEVEL": self.infoComp.LEVEL,
                "HP": self.infoComp.HP,
                "MAXHP": self.infoComp.MAXHP,
                "ATK": self.infoComp.ATK,
                "DEF": self.infoComp.DEF,
                "EXP": self.infoComp.EXP,
                "GOLD": self.infoComp.GOLD,
            },
            "items": self._items,
            "equips": self._equips,
            "equipInfo": self._equipInfo,
            "states": self.getStateStacks(),
        }

    @staticmethod
    def InitPlayer(playerPath: str) -> Player:
        r"""
        \brief Initialize a player character from a class path.

        - \param playerPath  Path to the player class.

        - \return A new `Player` instance initialized with the provided class path.
        """
        actorClass: Type[Player] = Data.getClass(playerPath)
        texturePath = actorClass.texturePath
        defaultRect = actorClass.defaultRect
        actor: Player = Cast(
            Player, actorClass.GenActor(actorClass, Manager.loadCharacter(texturePath), defaultRect, "PLAYER")
        )
        actor.setMapTag(actor.tag)
        actor.setClassPath(playerPath)
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
        AssertType(data["equips"], Dict[str, int])
        AssertType(data["equipInfo"], Dict[str, str])
        player = Player.InitPlayer(data["playerClass"])
        player.tag = data["tag"]
        player.setMapPosition(Vector2u(*data["position"]))
        for k, v in data["attr"].items():
            if not setComponentFieldValue(player, k, v):
                setattr(player, k, v)

        player._items = data["items"]
        player._equips = data["equips"]
        player._equipInfo = data["equipInfo"]
        states = data.get("states", {})
        if isinstance(states, list):
            player.setStateIDs(states)
        elif isinstance(states, dict):
            player.setStateStacks(states)
        return player

    @Meta(GeneralDataVars=[("itemID", GeneralDataKey.Item)])
    @ExecSplit(default=(None,))
    def addItem(self, itemID: str, count: int = 1) -> None:
        r"""\brief Add item(s) to the player's inventory.

        - `itemID` - Item identifier.
        - `count` - Number of items to add, default is 1.
        """
        if itemID in self._items:
            self._items[itemID] += count
        else:
            self._items[itemID] = count

    @Meta(GeneralDataVars=[("itemID", GeneralDataKey.Item)])
    @ExecSplit(success=(True,), failed=(False,))
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

    @Meta(GeneralDataVars=[("itemID", GeneralDataKey.Item)])
    @ReturnType(count=int)
    def getItemCount(self, itemID: str) -> int:
        r"""\brief Get the count of a specific item in the player's inventory.

        - `itemID` - Item identifier.

        - \return Number of items owned, or 0 if not found.
        """
        return self._items.get(itemID, 0)

    @Meta(GeneralDataVars=[("itemID", GeneralDataKey.Item)])
    @ReturnType(value=bool)
    def hasItem(self, itemID: str) -> bool:
        r"""\brief Check whether the player owns at least one of the specified item.

        - `itemID` - Item identifier.

        - \return `True` if the item is owned and count > 0, `False` otherwise.
        """
        return itemID in self._items and self._items[itemID] > 0

    @Meta(GeneralDataVars=[("equipID", GeneralDataKey.Equip)])
    @ExecSplit(default=(None,))
    def addEquip(self, equipID: str, count: int = 1) -> None:
        r"""\brief Add equip(s) to the player's equipment.

        - `equipID` - Equip identifier.
        - `count` - Number of equips to add, default is 1.
        """
        if equipID in self._equips:
            self._equips[equipID] += count
        else:
            self._equips[equipID] = count

    @Meta(GeneralDataVars=[("equipID", GeneralDataKey.Equip)])
    @ExecSplit(success=(True,), failed=(False,))
    def removeEquip(self, equipID: str, count: int = 1) -> bool:
        r"""\brief Remove equip(s) from the player's equipment.

        - `equipID` - Equip identifier.
        - `count` - Number of equips to remove, default is 1.

        - \return `True` if removal succeeded, `False` otherwise.
        """
        if equipID not in self._equips or self._equips[equipID] < count:
            return False
        self._equips[equipID] -= count
        if self._equips[equipID] == 0:
            del self._equips[equipID]
        return True

    @Meta(GeneralDataVars=[("equipID", GeneralDataKey.Equip)])
    @ExecSplit(default=(None,))
    def equip(self, equipID: str) -> None:
        r"""\brief Equip a specific equip to the player's equipment.

        - `equipID` - Equip identifier.
        """
        equipInfo = Data.getGeneralEquipData(equipID)
        classInfo = Data.getGeneralClassData(self.infoComp.CLASS)
        slot = equipInfo.get("slot", "")
        if slot not in classInfo.get("slot", {}):
            raise ValueError(f"Equip {equipID} is not in the player's class")
        currentID = self._equipInfo.get(slot, "")
        if currentID and currentID != equipID:
            self.unequip(slot)
        self._updateEquipInfo(slot, equipID)
        for attrKey, attrValue in equipInfo.get("attrPlus", {}).items():
            if hasattr(self.infoComp, attrKey):
                originAttr = getattr(self.infoComp, attrKey)
                setattr(self.infoComp, attrKey, originAttr + int(attrValue))
            else:
                originAttr = getattr(self, attrKey)
                setattr(self, attrKey, originAttr + int(attrValue))
        info = EquipInfo()
        info.ID = equipID
        info.initInfo(Data)
        info.triggerEvent("onEquip")
        self.removeEquip(equipID)

    @ExecSplit(default=(None,))
    def unequip(self, slotID: str) -> None:
        r"""\brief Unequip a specific equip from the player's equipment.

        - `slotID` - Slot identifier.
        """
        equipID = self._equipInfo.get(slotID, "")
        if not equipID:
            return
        self._updateEquipInfo(slotID, "")
        equipInfo = Data.getGeneralEquipData(equipID)
        for attrKey, attrValue in equipInfo.get("attrPlus", {}).items():
            if hasattr(self.infoComp, attrKey):
                originAttr = getattr(self.infoComp, attrKey)
                setattr(self.infoComp, attrKey, originAttr - int(attrValue))
            else:
                originAttr = getattr(self, attrKey)
                setattr(self, attrKey, originAttr - int(attrValue))
        info = EquipInfo()
        info.ID = equipID
        info.initInfo(Data)
        info.triggerEvent("onUnequip")
        self.addEquip(equipID)

    @Meta(GeneralDataVars=[("equipID", GeneralDataKey.Equip)])
    @ReturnType(count=int)
    def getEquipCount(self, equipID: str) -> int:
        r"""\brief Get the count of a specific equip in the player's equipment.

        - `equipID` - Equip identifier.

        - \return Number of equips owned, or 0 if not found.
        """
        return self._equips.get(equipID, 0)

    @Meta(GeneralDataVars=[("equipID", GeneralDataKey.Equip)])
    @ReturnType(value=bool)
    def hasEquip(self, equipID: str) -> bool:
        r"""\brief Check whether the player owns at least one of the specified equip.

        - `equipID` - Equip identifier.

        - \return `True` if the equip is owned and count > 0, `False` otherwise.
        """
        return equipID in self._equips and self._equips[equipID] > 0

    @ReturnType(value=str)
    def getEquipInfo(self, slotID: str) -> str:
        r"""\brief Get the info of a specific equip in the player's equipment.

        - `slotID` - Slot identifier.

        - \return The info of the equip, or empty string if not found.
        """
        return self._equipInfo.get(slotID, "")

    @ReturnType(value=None)
    def getForbiddenMoving(self) -> bool:
        return self._forbiddenMoving

    @ExecSplit(default=(None,))
    def setForbiddenMoving(self, value: bool) -> None:
        self._forbiddenMoving = value

    def _isSceneInputBlocked(self) -> bool:
        gameMap = self.getMap()
        if gameMap is None:
            return False
        scene = gameMap.getScene()
        return scene is not None and scene.isInputBlocked()

    def _updateEquipInfo(self, slot: str, equipID: str) -> None:
        tempEquipInfo = {}
        classInfo = Data.getGeneralClassData(self.infoComp.CLASS)
        for slot_ in classInfo.get("slot", {}).keys():
            if slot_ != slot:
                tempEquipInfo[slot_] = self._equipInfo.get(slot_, "")
            else:
                tempEquipInfo[slot_] = equipID
        self._equipInfo = tempEquipInfo
