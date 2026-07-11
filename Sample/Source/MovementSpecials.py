# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import TYPE_CHECKING, Any, Dict, List, Tuple, cast
from Engine import Vector2i
from Engine.Utils import Event
from Source.Configs.EventKeys import EventKeys
from Source.Configs.GeneralEnum import Special

if TYPE_CHECKING:
    from Source.Enemy import Enemy
    from Source.Player import Player

_handlersRegistered: bool = False


def registerHandlers() -> None:
    r"""\brief Register movement-special handlers on the shared EventBus."""
    global _handlersRegistered
    if _handlersRegistered:
        return
    Event.subscribe(EventKeys.PlayerMovementFinished, _onPlayerMovementFinished)
    _handlersRegistered = True


def notifyPlayerMovementFinished(player: Player) -> None:
    r"""\brief Notify listeners that the player has finished a movement step.

    - \param player The player that just stopped moving.
    """
    registerHandlers()
    Event.publish(EventKeys.PlayerMovementFinished, {"player": player})


def _onPlayerMovementFinished(payload: Any) -> None:
    if not isinstance(payload, dict):
        return
    player = cast("Player", payload.get("player"))
    if player is None:
        return
    _applyMovementSpecials(player)


def _applyMovementSpecials(player: Player) -> None:
    from Source.Enemy import Enemy

    gameMap = player.getMap()
    if gameMap is None:
        return

    enemies: List[Enemy] = [
        actor for actor in gameMap.getAllActors() if isinstance(actor, Enemy) and actor.getSpecial()
    ]
    if not enemies:
        return

    playerPos = player.getMapPosition()
    totalDamage = 0
    damagingEnemies: List[Enemy] = []

    for enemy in enemies:
        enemyPos = enemy.getMapPosition()
        dist = _getManhattanDistance(playerPos, enemyPos)

        if enemy.hasSpecial(Special.Domain):
            domainRange = enemy._getSpecialIntValue(Special.Domain, 0, 1)
            if dist < domainRange:
                totalDamage += enemy.getDamagePerRound(player)
                damagingEnemies.append(enemy)

        if enemy.hasSpecial(Special.Blockade) and dist == 1:
            totalDamage += enemy.getDamagePerRound(player)
            damagingEnemies.append(enemy)
            _doBlockadeRetreat(enemy, playerPos)

    flankEnemies = [enemy for enemy in enemies if enemy.hasSpecial(Special.Flank)]
    if len(flankEnemies) >= 2:
        flankDamage, flankAttackers = _checkFlankDamage(flankEnemies, player, playerPos)
        totalDamage += flankDamage
        damagingEnemies.extend(flankAttackers)

    if totalDamage <= 0:
        return

    scene = gameMap.getScene()
    if scene is not None:
        playerPosition = player.getPosition()
        seenEnemies: set[int] = set()
        for enemy in damagingEnemies:
            enemyKey = id(enemy)
            if enemyKey in seenEnemies:
                continue
            seenEnemies.add(enemyKey)
            enemy.playAttackAnimationAt(scene, playerPosition)

    player.infoComp.HP -= totalDamage
    gameMap.addDamageText(str(totalDamage), player.getPosition())

    if player.infoComp.HP <= 0:
        from Source.Scenes import GameOver
        from Global import System

        System.setScene(GameOver())


def _doBlockadeRetreat(enemy: Enemy, playerPos: Vector2i) -> None:
    enemyPos = enemy.getMapPosition()
    moveX = _getSign(int(enemyPos.x) - int(playerPos.x))
    moveY = _getSign(int(enemyPos.y) - int(playerPos.y))
    offset = Vector2i(moveX, moveY)

    moved = enemy.MapMove(offset)
    newPos = enemyPos + offset if moved else enemyPos

    from Source.NodeFunctions.Utils import SetGameVariable

    tag = enemy.tag or enemy.ID
    SetGameVariable(f"Blockade_{tag}_X", int(newPos.x))
    SetGameVariable(f"Blockade_{tag}_Y", int(newPos.y))


def _checkFlankDamage(flankEnemies: List[Enemy], player: Player, playerPos: Vector2i) -> Tuple[int, List[Enemy]]:
    playerX = int(playerPos.x)
    playerY = int(playerPos.y)
    posMap: Dict[Tuple[int, int], Enemy] = {}

    for enemy in flankEnemies:
        enemyPos = enemy.getMapPosition()
        relPos = (int(enemyPos.x) - playerX, int(enemyPos.y) - playerY)
        posMap[relPos] = enemy

    totalDamage = 0
    attackers: List[Enemy] = []
    if (-1, 0) in posMap and (1, 0) in posMap:
        totalDamage += posMap[(-1, 0)].getDamagePerRound(player)
        totalDamage += posMap[(1, 0)].getDamagePerRound(player)
        attackers.extend([posMap[(-1, 0)], posMap[(1, 0)]])
    if (0, -1) in posMap and (0, 1) in posMap:
        totalDamage += posMap[(0, -1)].getDamagePerRound(player)
        totalDamage += posMap[(0, 1)].getDamagePerRound(player)
        attackers.extend([posMap[(0, -1)], posMap[(0, 1)]])
    return totalDamage, attackers


def _getManhattanDistance(a: Vector2i, b: Vector2i) -> int:
    return abs(int(a.x) - int(b.x)) + abs(int(a.y) - int(b.y))


def _getSign(value: int) -> int:
    if value > 0:
        return 1
    if value < 0:
        return -1
    return 0
