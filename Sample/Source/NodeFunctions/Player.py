# -*- encoding: utf-8 -*-

from typing import Any, Optional, List
from Engine import Direction, Vector2i
from Engine.Gameplay.Actors import Actor
from Engine.Gameplay.Components import getComponentFieldValue, setComponentFieldValue
from Global import System
from Source.Player import Player
from .Utils import _attrRef


_MISSING = object()


def _getPlayer() -> Optional[Player]:
    scene = System.getScene()
    if scene and hasattr(scene, "inst"):
        return scene.inst.getPlayer()
    return None


@Meta(DisplayName='LOC("GET_PLAYER")', DisplayDesc='LOC("GET_PLAYER_DESC")')
@ReturnType(player=Player)
def GetPlayer() -> Optional[Player]:
    r"""\brief Get the primary player from the current scene.

    - \return The primary Player instance, or None if no active game scene.
    """
    return _getPlayer()


@Meta(DisplayName='LOC("GET_PLAYER_FRONT_POSITION")', DisplayDesc='LOC("GET_PLAYER_FRONT_POSITION_DESC")')
@ReturnType(position=Vector2i)
def GetPlayerFrontPosition() -> Optional[Vector2i]:
    r"""\brief Get the tile position in front of the player.

    - \return The tile position in front of the player, or None if no active player exists.
    """
    player = _getPlayer()
    if player is None:
        return None
    position = player.getMapPosition()
    direction = player.direction
    if direction == Direction.UP:
        return Vector2i(position.x, position.y - 1)
    if direction == Direction.LEFT:
        return Vector2i(position.x - 1, position.y)
    if direction == Direction.RIGHT:
        return Vector2i(position.x + 1, position.y)
    return Vector2i(position.x, position.y + 1)


@Meta(DisplayName='LOC("ADD_ITEM")', DisplayDesc='LOC("ADD_ITEM_DESC")')
@ExecSplit(default=(None,))
def AddItem(itemID: str, count: int = 1) -> None:
    r"""\brief Add item(s) to the player's inventory.

    - \param itemID Item identifier.
    - \param count Number of items to add.
    """
    player = _getPlayer()
    if player:
        player.addItem(itemID, count)


@Meta(DisplayName='LOC("REMOVE_ITEM")', DisplayDesc='LOC("REMOVE_ITEM_DESC")')
@ExecSplit(Success=(0,), Failed=(1,))
def RemoveItem(itemID: str, count: int = 1) -> int:
    r"""\brief Remove item(s) from the player's inventory.

    - \param itemID Item identifier.
    - \param count Number of items to remove.
    - \return 0 on success, 1 if item count is insufficient.
    """
    player = _getPlayer()
    if player and player.removeItem(itemID, count):
        return 0
    return 1


@Meta(DisplayName='LOC("HAS_ITEM")', DisplayDesc='LOC("HAS_ITEM_DESC")')
@ReturnType(value=bool)
def HasItem(itemID: str) -> bool:
    r"""\brief Check whether the player owns at least one of the specified item.

    - \param itemID Item identifier.
    - \return True if the player owns the item.
    """
    player = _getPlayer()
    return bool(player and player.hasItem(itemID))


@Meta(DisplayName='LOC("GET_ITEM_COUNT")', DisplayDesc='LOC("GET_ITEM_COUNT_DESC")')
@ReturnType(count=int)
def GetItemCount(itemID: str) -> int:
    r"""\brief Get the count of a specific item in the player's inventory.

    - \param itemID Item identifier.
    - \return Number of items owned, or 0 if not found.
    """
    player = _getPlayer()
    return player.getItemCount(itemID) if player else 0


@Meta(DisplayName='LOC("ADD_EQUIP")', DisplayDesc='LOC("ADD_EQUIP_DESC")')
@ExecSplit(default=(None,))
def AddEquip(equipID: str, count: int = 1) -> None:
    r"""\brief Add equip(s) to the player's equipment bag.

    - \param equipID Equip identifier.
    - \param count Number of equips to add.
    """
    player = _getPlayer()
    if player:
        player.addEquip(equipID, count)


@Meta(DisplayName='LOC("REMOVE_EQUIP")', DisplayDesc='LOC("REMOVE_EQUIP_DESC")')
@ExecSplit(Success=(0,), Failed=(1,))
def RemoveEquip(equipID: str, count: int = 1) -> int:
    r"""\brief Remove equip(s) from the player's equipment bag.

    - \param equipID Equip identifier.
    - \param count Number of equips to remove.
    - \return 0 on success, 1 if equip count is insufficient.
    """
    player = _getPlayer()
    if player and player.removeEquip(equipID, count):
        return 0
    return 1


@Meta(DisplayName='LOC("HAS_EQUIP")', DisplayDesc='LOC("HAS_EQUIP_DESC")')
@ReturnType(value=bool)
def HasEquip(equipID: str) -> bool:
    r"""\brief Check whether the player owns at least one of the specified equip.

    - \param equipID Equip identifier.
    - \return True if the player owns the equip.
    """
    player = _getPlayer()
    return bool(player and player.hasEquip(equipID))


@Meta(DisplayName='LOC("EQUIP_ITEM")', DisplayDesc='LOC("EQUIP_ITEM_DESC")')
@ExecSplit(default=(None,))
def EquipItem(equipID: str) -> None:
    r"""\brief Equip a piece of equipment onto the player.

    - \param equipID Equip identifier.
    """
    player = _getPlayer()
    if player:
        player.equip(equipID)


@Meta(DisplayName='LOC("UNEQUIP_SLOT")', DisplayDesc='LOC("UNEQUIP_SLOT_DESC")')
@ExecSplit(default=(None,))
def UnequipSlot(slotID: str) -> None:
    r"""\brief Unequip the item occupying the given equipment slot.

    - \param slotID Equipment slot identifier.
    """
    player = _getPlayer()
    if player:
        player.unequip(slotID)


@Meta(DisplayName='LOC("GET_EQUIP_IN_SLOT")', DisplayDesc='LOC("GET_EQUIP_IN_SLOT_DESC")')
@ReturnType(equipID=str)
def GetEquipInSlot(slotID: str) -> str:
    r"""\brief Get the equip ID currently occupying the given slot.

    - \param slotID Equipment slot identifier.
    - \return The equip ID, or an empty string if the slot is empty.
    """
    player = _getPlayer()
    return player.getEquipInfo(slotID) if player else ""


@Meta(DisplayName='LOC("GET_PLAYER_ATTR")', DisplayDesc='LOC("GET_PLAYER_ATTR_DESC")')
@ReturnType(value=object)
def GetPlayerAttr(attrName: str) -> Any:
    r"""\brief Get a named attribute from the player (e.g. HP, MAXHP, ATK, DEF, GOLD, EXP, LEVEL).

    - \param attrName The attribute name.
    - \return The attribute value, or None if not found.
    """
    player = _getPlayer()
    if player is None:
        return None
    value = getComponentFieldValue(player, attrName, _MISSING)
    if value is not _MISSING:
        return value
    return getattr(player, attrName, None)


@Meta(DisplayName='LOC("SET_PLAYER_ATTR")', DisplayDesc='LOC("SET_PLAYER_ATTR_DESC")')
@ExecSplit(default=(None,))
def SetPlayerAttr(attrName: str, value: Any) -> None:
    r"""\brief Set a named attribute on the player (e.g. HP, MAXHP, ATK, DEF, GOLD, EXP, LEVEL).

    - \param attrName The attribute name.
    - \param value The new value.
    """
    player = _getPlayer()
    if player:
        if not setComponentFieldValue(player, attrName, value):
            setattr(player, attrName, value)


@Meta(DisplayName='LOC("GET_PLAYER_ATTR_REF")', DisplayDesc='LOC("GET_PLAYER_ATTR_REF_DESC")')
@ReturnType(value=object)
def GetPlayerAttrRef(attrName: str) -> Any:
    r"""\brief Get a readable/writable reference to a player attribute.

    - \param attrName The attribute name (e.g. HP, ATK, DEF, GOLD).
    - \return An attribute reference wrapper, or None if no active player.
    """
    player = _getPlayer()
    if player is None:
        return None
    return _attrRef(player, attrName)


@Meta(DisplayName='LOC("HEAL_PLAYER")', DisplayDesc='LOC("HEAL_PLAYER_DESC")')
@ExecSplit(default=(None,))
def HealPlayer(amount: int = 1) -> None:
    r"""\brief Restore HP to the player, capped at MAXHP.

    - \param amount Amount of HP to restore.
    """
    player = _getPlayer()
    if player:
        player.infoComp.HP = max(0, min(player.infoComp.HP + int(amount), player.infoComp.MAXHP))


@Meta(DisplayName='LOC("DAMAGE_PLAYER")', DisplayDesc='LOC("DAMAGE_PLAYER_DESC")')
@ExecSplit(default=(None,))
def DamagePlayer(amount: int = 1) -> None:
    r"""\brief Deal damage to the player, floored at 0.

    - \param amount Amount of HP to subtract.
    """
    player = _getPlayer()
    if player:
        player.infoComp.HP = max(player.infoComp.HP - int(amount), 0)


@Meta(DisplayName='LOC("REMOVE_PLAYER_STATE")', DisplayDesc='LOC("REMOVE_PLAYER_STATE_DESC")')
@ExecSplit(default=(None,))
def RemovePlayerState(stateID: str) -> None:
    r"""\brief Remove a state from the player.

    - \param stateID State identifier.
    """
    player = _getPlayer()
    if player:
        player.removeState(stateID)


@Meta(DisplayName='LOC("REDUCE_PLAYER_STATE")', DisplayDesc='LOC("REDUCE_PLAYER_STATE_DESC")')
@ExecSplit(default=(None,))
def ReducePlayerState(stateID: str, stacks: int = 1) -> None:
    r"""\brief Reduce a state stack count on the player and remove it at zero.

    - \param stateID State identifier.
    - \param stacks Stack count to reduce.
    """
    player = _getPlayer()
    if player:
        player.reduceStateStacks(stateID, stacks)


@Meta(DisplayName='LOC("ADD_HP")', DisplayDesc='LOC("ADD_HP_DESC")')
@ExecSplit(default=(None,))
def AddHP(amount: int = 1) -> None:
    r"""\brief Add HP to the player.

    - \param amount Amount of HP to add (can be negative to subtract).
    """
    player = _getPlayer()
    if player:
        player.infoComp.HP += int(amount)


@Meta(DisplayName='LOC("ADD_GOLD")', DisplayDesc='LOC("ADD_GOLD_DESC")')
@ExecSplit(default=(None,))
def AddGold(amount: int = 1) -> None:
    r"""\brief Add gold to the player.

    - \param amount Amount of gold to add (can be negative to subtract).
    """
    player = _getPlayer()
    if player:
        player.infoComp.GOLD += int(amount)


@Meta(DisplayName='LOC("ADD_ATK")', DisplayDesc='LOC("ADD_ATK_DESC")')
@ExecSplit(default=(None,))
def AddATK(amount: int = 1) -> None:
    r"""\brief Add ATK to the player.

    - \param amount Amount of ATK to add (can be negative to subtract).
    """
    player = _getPlayer()
    if player:
        player.infoComp.ATK += int(amount)


@Meta(DisplayName='LOC("ADD_DEF")', DisplayDesc='LOC("ADD_DEF_DESC")')
@ExecSplit(default=(None,))
def AddDEF(amount: int = 1) -> None:
    r"""\brief Add DEF to the player.

    - \param amount Amount of DEF to add (can be negative to subtract).
    """
    player = _getPlayer()
    if player:
        player.infoComp.DEF += int(amount)


@Meta(DisplayName='LOC("ADD_EXP")', DisplayDesc='LOC("ADD_EXP_DESC")')
@ExecSplit(default=(None,))
def AddEXP(amount: int = 1) -> None:
    r"""\brief Add experience points to the player.

    - \param amount Amount of EXP to add.
    """
    player = _getPlayer()
    if player:
        player.infoComp.EXP += int(amount)


@Meta(DisplayName='LOC("MEET_PLAYER")', DisplayDesc='LOC("MEET_PLAYER_DESC")')
@ReturnType(playerInfo=Player)
def MeetPlayer(actors: List[Actor]) -> Optional[Player]:
    from Source.Scenes import Map
    from Global import System

    map = Cast(Map, System.getScene())
    player = map.inst.getPlayer()
    if player and player in actors:
        return player
    return None
