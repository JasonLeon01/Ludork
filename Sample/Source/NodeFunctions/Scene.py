# -*- encoding: utf-8 -*-

from typing import Any, Callable, List, Optional, Tuple, Union
from Engine import Pair, Vector2i, Vector2u
from Engine.Gameplay.Actors import Actor
from Global import Manager, System


def _evalTerrainTileID(tileID: Any) -> Union[int, str, None]:
    if isinstance(tileID, str):
        try:
            return Eval(tileID)
        except Exception:
            return tileID
    return tileID


def _posToVector2u(position: Any) -> Optional[Vector2u]:
    if position is None:
        return None
    if isinstance(position, (list, tuple)) and len(position) >= 2:
        try:
            return Vector2u(int(position[0]), int(position[1]))
        except (TypeError, ValueError):
            return None
    if hasattr(position, "x") and hasattr(position, "y"):
        try:
            return Vector2u(int(position.x), int(position.y))
        except (TypeError, ValueError):
            return None
    return None


def _getSceneMap():
    from Source.Scenes import Map as SceneMap

    scene = System.getScene()
    assert isinstance(scene, SceneMap)
    return scene


def _getBlueprintOwner(refLocal) -> Optional[Any]:
    graph = refLocal.get("__graph__")
    if graph is None:
        return None
    parent = getattr(graph, "parent", None)
    if parent is None:
        return None
    if hasattr(parent, "getOwner"):
        return parent.getOwner()
    return parent


@Meta(
    DisplayName='LOC("GOTO_MAP")',
    DisplayDesc='LOC("GOTO_MAP_DESC")',
    Transfer=[("position", "mapPath")],
)
@ExecSplit(default=(None,))
def GotoMap(
    mapPath: str,
    blockTransition: bool = False,
    position: Optional[Union[Vector2i, Pair[int], List[int], Tuple[int, int]]] = None,
) -> None:
    r"""\brief Transition to a map and optionally place the player at a tile coordinate.

    - \param mapPath Path to the map data file (relative to Data/Maps/).
    - \param blockTransition Whether to skip the map transition effect.
    - \param position Target tile coordinate as ``(x, y)``, or ``None`` to keep the current position.
    """
    scene = System.getScene()
    if scene and hasattr(scene, "gotoMapAndPos"):
        scene.gotoMapAndPos(mapPath, _posToVector2u(position), blockTransition=bool(blockTransition))


@Meta(DisplayName='LOC("GAME_OVER")', DisplayDesc='LOC("GAME_OVER_DESC")')
@ExecSplit(default=(None,))
def GameOver() -> None:
    r"""\brief Switch to the game over scene."""
    from Source.Scenes import GameOver as GameOverScene

    System.setScene(GameOverScene())


@Meta(DisplayName='LOC("ADD_TIMER")', DisplayDesc='LOC("ADD_TIMER_DESC")')
@Latent(TimeUp=(True,))
def AddTimer(interval: float, blocking: bool = False) -> Callable[[], bool]:
    r"""\brief Add a timer on the current scene.

    - \param interval Time in seconds before the timer fires.
    - \param blocking Whether scene input should be blocked until the timer fires.
    - \return A condition callable that becomes True when the timer fires.
    """
    scene = System.getScene()
    if scene is not None:
        return scene.addTimer(float(interval), blocking=bool(blocking))

    def condition() -> bool:
        return True

    return condition


@Meta(DisplayName='LOC("SHOW_MESSAGE")', DisplayDesc='LOC("SHOW_MESSAGE_DESC")')
@Latent(FinishedDialogue=(True,))
def ShowMessage(
    name: str,
    message: str,
    refActorTag: str = "",
) -> Callable[[], bool]:
    r"""\brief Show a dialogue message on the current map scene."""
    scene = _getSceneMap()
    return scene.showMessage(refActorTag, name, message)


@Meta(
    DisplayName='LOC("SHOW_VOICE_MESSAGE")',
    DisplayDesc='LOC("SHOW_VOICE_MESSAGE_DESC")',
    PathVars=[("voiceFileName", "Sounds")],
    Rely={"minDistance": {"source": "refActor", "op": "!=", "value": None}},
)
@Latent(FinishedDialogue=(True,))
def ShowVoiceMessage(
    name: str,
    message: str,
    voiceFileName: str,
    refActorTag: str = "",
    refActor: Optional[Actor] = None,
    minDistance: float = 64.0,
) -> Callable[[], bool]:
    r"""\brief Play a voice clip and show a dialogue message on the current map scene."""
    scene = _getSceneMap()
    if refActor is None:
        Manager.playVoice(voiceFileName)
    else:
        Manager.playVoice(voiceFileName, refActor=refActor, minDistance=float(minDistance))
    return scene.showMessage(refActorTag, name, message)


@Meta(DisplayName='LOC("SHOW_SELECTION")', DisplayDesc='LOC("SHOW_SELECTION_DESC")')
@Latent(Selected0=(0,), Selected1=(1,), Selected2=(2,), Selected3=(3,), Cancelled=(-1,))
def ShowSelection(
    name: str,
    options: List[str],
    refActorTag: str = "",
    allowCancel: bool = True,
) -> Callable[[], Optional[int]]:
    r"""\brief Show a selection window on the current map scene."""
    scene = _getSceneMap()
    return scene.showSelection(refActorTag, name, options, bool(allowCancel))


@Meta(DisplayName='LOC("LOCK_CAMERA")', DisplayDesc='LOC("LOCK_CAMERA_DESC")')
@ExecSplit(default=(None,))
def LockCamera() -> None:
    r"""\brief Lock the current map camera to the player."""
    scene = System.getScene()
    if scene and hasattr(scene, "getGameMap"):
        gameMap = scene.getGameMap()
        if gameMap is not None:
            camera = gameMap.getCamera()
            player = gameMap.getPlayer()
            if camera is not None and player is not None:
                camera.setParent(player)
                viewSize = camera.getViewSize()
                if viewSize is not None:
                    camera.setViewPosition(player.getPosition() - viewSize / 2)
                    camera.fixViewPosition()


@Meta(DisplayName='LOC("UNLOCK_CAMERA")', DisplayDesc='LOC("UNLOCK_CAMERA_DESC")')
@ExecSplit(default=(None,))
def UnlockCamera() -> None:
    r"""\brief Unlock the current map camera from the player."""
    scene = System.getScene()
    if scene and hasattr(scene, "getGameMap"):
        gameMap = scene.getGameMap()
        if gameMap is not None:
            camera = gameMap.getCamera()
            if camera is not None:
                camera.setParent(None)


@Meta(DisplayName='LOC("RECORD_TELEPOINT")', DisplayDesc='LOC("RECORD_TELEPOINT_DESC")')
@ExecSplit(default=(None,))
def RecordTelepoint(mapPath: str, x: int = 0, y: int = 0) -> None:
    r"""\brief Record a telepoint in the current game instance.

    - \param mapPath The map path where the telepoint is located.
    - \param x Telepoint tile column.
    - \param y Telepoint tile row.
    """
    scene = System.getScene()
    if scene and hasattr(scene, "inst"):
        scene.inst.recordTelepoint(mapPath, Vector2u(int(x), int(y)))


@Meta(
    DisplayName='LOC("CREATE_ACTOR_FROM_BP_PATH")',
    DisplayDesc='LOC("CREATE_ACTOR_FROM_BP_PATH_DESC")',
    Vector2iVars=["position"],
)
@ExecSplit(default=(None,))
@ReturnType(actor=Optional[Actor])
def CreateActorFromBPPath(
    bpPath: str,
    layerName: str = "default",
    position: Optional[Union[Vector2i, Vector2u, Pair[int], List[int], Tuple[int, int]]] = None,
    tag: Optional[str] = None,
    emitCreateEvent: bool = True,
) -> Optional[Actor]:
    r"""\brief Create an actor from a blueprint path and spawn it on the current map.

    - \param bpPath The blueprint class path, such as Data.Blueprints.Enemies.BP_Enemy_redKing.
    - \param layerName The layer name to place the created actor on.
    - \param position Optional tile coordinate for the created actor.
    - \param tag Optional tag string for the created actor.
    - \param emitCreateEvent Whether to run the actor's onCreate blueprint event.
    - \return The created actor instance, or None when the actor cannot be created.
    """
    from Source import Data

    scene = System.getScene()
    if not scene or not hasattr(scene, "getGameMap"):
        return None
    gameMap = scene.getGameMap()
    if gameMap is None:
        return None
    actor = Data.genActorFromClassPath(bpPath, tag)
    if actor is None:
        return None
    mapPosition = _posToVector2u(position)
    if mapPosition is not None:
        actor.setMapPosition(mapPosition)
    gameMap.spawnActor(actor, layerName, emitCreateEvent)
    return actor


@Meta(DisplayName='LOC("DESTROY_TERRAIN")', DisplayDesc='LOC("DESTROY_TERRAIN_DESC")', Vector2iVars=["position"])
@ExecSplit(default=(None,))
def DestroyTerrain(
    layerName: str,
    position: Union[Vector2i, Vector2u, Pair[int], List[int]],
    tileID: Any = None,
) -> None:
    r"""\brief Replace and persist one terrain tile on the current map.

    - \param layerName The tile layer to edit.
    - \param position The tile coordinate.
    - \param tileID The replacement tile expression, autotile key, or None.
    """
    scene = System.getScene()
    if scene and hasattr(scene, "getGameMap"):
        gameMap = scene.getGameMap()
        if gameMap is not None:
            gameMap.destroyTerrain(layerName, position, _evalTerrainTileID(tileID))


@Meta(DisplayName='LOC("DESTROY_TERRAIN_LIST")', DisplayDesc='LOC("DESTROY_TERRAIN_LIST_DESC")')
@ExecSplit(default=(None,))
def DestroyTerrainList(layerName: str, positions: List[Any], tileID: Any = None) -> None:
    r"""\brief Replace and persist multiple terrain tiles on the current map.

    - \param layerName The tile layer to edit.
    - \param positions The tile coordinates.
    - \param tileID The replacement tile expression, autotile key, or None.
    """
    scene = System.getScene()
    if scene and hasattr(scene, "getGameMap"):
        gameMap = scene.getGameMap()
        if gameMap is not None:
            gameMap.destroyTerrainList(layerName, positions, _evalTerrainTileID(tileID))


@Meta(DisplayName='LOC("GET_TERRAIN_TILE")', DisplayDesc='LOC("GET_TERRAIN_TILE_DESC")', Vector2iVars=["position"])
@ReturnType(tileID=Any)
def GetTerrainTile(layerName: str, position: Union[Vector2i, Vector2u, Pair[int], List[int]]) -> Any:
    r"""\brief Get the terrain tile ID on the current map.

    - \param layerName The tile layer to query.
    - \param position The tile coordinate.
    - \return The static tile ID, autotile key, or None.
    """
    scene = System.getScene()
    if scene and hasattr(scene, "getGameMap"):
        gameMap = scene.getGameMap()
        if gameMap is not None:
            return gameMap.getTerrainTile(layerName, position)
    return None


@Meta(DisplayName='LOC("GET_TERRAIN_TILE_POSITIONS")', DisplayDesc='LOC("GET_TERRAIN_TILE_POSITIONS_DESC")')
@ReturnType(positions=List[Vector2i])
def GetTerrainTilePositions(layerName: str, tileID: Any = None) -> List[Vector2i]:
    r"""\brief Get all current-map coordinates that match a tile ID on one layer.

    - \param layerName The tile layer to query.
    - \param tileID The static tile ID expression, autotile key, or None.
    - \return A list of matching tile coordinates.
    """
    scene = System.getScene()
    if scene and hasattr(scene, "getGameMap"):
        gameMap = scene.getGameMap()
        if gameMap is not None:
            return gameMap.getTerrainTilePositions(layerName, _evalTerrainTileID(tileID))
    return []


@Meta(DisplayName='LOC("RECORD_DESTROYED_ACTOR")', DisplayDesc='LOC("RECORD_DESTROYED_ACTOR_DESC")')
@ExecSplit(default=(None,))
def RecordDestroyedActor(actor: Actor) -> None:
    r"""\brief Record a destroyed actor for persistence on the current map scene.

    - \param actor The destroyed actor.
    """
    scene = _getSceneMap()
    scene.recordDestroyedActor(actor)


@Meta(DisplayName='LOC("SELF_RECORD_DESTROYED")', DisplayDesc='LOC("SELF_RECORD_DESTROYED_DESC")')
@ExecSplit(default=(None,))
def SelfRecordDestroyed() -> None:
    r"""\brief Record the blueprint owner as a destroyed actor for persistence on the current map scene.
    """
    actor = _getBlueprintOwner(SelfRecordDestroyed._refLocal)
    if actor is None:
        return
    scene = _getSceneMap()
    scene.recordDestroyedActor(actor)


@Meta(DisplayName='LOC("RECORD_AND_DESTROY_ACTOR")', DisplayDesc='LOC("RECORD_AND_DESTROY_ACTOR_DESC")')
@ExecSplit(default=(None,))
def RecordAndDestroyActor(actor: Actor) -> None:
    r"""\brief Record a destroyed actor for persistence and destroy it on the current map scene.

    - \param actor The actor to record and destroy.
    """
    scene = _getSceneMap()
    scene.recordDestroyedActor(actor)
    actor.destroy()


@Meta(DisplayName='LOC("SELF_RECORD_AND_DESTROY")', DisplayDesc='LOC("SELF_RECORD_AND_DESTROY_DESC")')
@ExecSplit(default=(None,))
def SelfRecordAndDestroy() -> None:
    r"""\brief Record the blueprint owner as destroyed for persistence and destroy it on the current map scene.
    """
    actor = _getBlueprintOwner(SelfRecordAndDestroy._refLocal)
    if actor is None:
        return
    scene = _getSceneMap()
    scene.recordDestroyedActor(actor)
    actor.destroy()


@Meta(DisplayName='LOC("OPEN_SHOP")', DisplayDesc='LOC("OPEN_SHOP_DESC")')
@Latent(Closed=(True,))
def OpenShop(items: List[str], canSell: bool = True) -> Callable[[], bool]:
    r"""\brief Open the map-bound shop.

    - \param items Item IDs available for purchase.
    - \param canSell Whether selling is available.
    - \return A condition callable that becomes True when the shop closes.
    """
    scene = System.getScene()
    if scene and hasattr(scene, "openShop"):
        return scene.openShop(list(items), bool(canSell))

    def condition() -> bool:
        return True

    return condition
