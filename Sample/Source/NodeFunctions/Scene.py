# -*- encoding: utf-8 -*-

from typing import Any, Callable, Dict, List, Optional, Tuple, Union
from Engine import Pair, Vector2f, Vector2i, Vector2u
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


def _posToVector2f(position: Any) -> Optional[Vector2f]:
    if position is None:
        return None
    if isinstance(position, (list, tuple)) and len(position) >= 2:
        try:
            return Vector2f(float(position[0]), float(position[1]))
        except (TypeError, ValueError):
            return None
    if hasattr(position, "x") and hasattr(position, "y"):
        try:
            return Vector2f(float(position.x), float(position.y))
        except (TypeError, ValueError):
            return None
    return None


def _getSceneMap():
    from Source.Scenes import Map as SceneMap

    scene = System.getScene()
    assert isinstance(scene, SceneMap)
    return scene


def _getSceneCamera():
    scene = System.getScene()
    if not scene or not hasattr(scene, "getGameMap"):
        return None
    gameMap = scene.getGameMap()
    if gameMap is None:
        return None
    return gameMap.getCamera()


def _getActorByTag(scene: Any, refActorTag: str) -> Optional[Actor]:
    if not bool(refActorTag):
        return None
    return scene.getGameMap().getActorByTag(refActorTag)


def _snapCameraToActor(camera: Any, actor: Actor) -> None:
    viewSize = camera.getViewSize()
    if viewSize is None:
        return
    camera.setViewPosition(actor.getPosition() - viewSize / 2)
    camera.fixViewPosition()


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
    return scene.showMessage(name, message, _getActorByTag(scene, refActorTag))


@Meta(DisplayName='LOC("SHOW_REF_MESSAGE")', DisplayDesc='LOC("SHOW_REF_MESSAGE_DESC")')
@Latent(FinishedDialogue=(True,))
def ShowRefMessage(
    name: str,
    message: str,
    actor: Actor,
) -> Callable[[], bool]:
    r"""\brief Show a dialogue message on the current map scene positioned by a direct actor reference."""
    scene = _getSceneMap()
    return scene.showMessage(name, message, actor)


@Meta(
    DisplayName='LOC("SHOW_VOICE_MESSAGE")',
    DisplayDesc='LOC("SHOW_VOICE_MESSAGE_DESC")',
    PathVars=[("voiceFileName", "Voices")],
)
@Latent(FinishedDialogue=(True,))
def ShowVoiceMessage(
    name: str,
    message: str,
    voiceFileName: str,
    refActorTag: str = "",
) -> Callable[[], bool]:
    r"""\brief Play a non-spatial voice clip and show a dialogue message on the current map scene."""
    scene = _getSceneMap()
    Manager.playVoice(voiceFileName)
    return scene.showMessage(name, message, _getActorByTag(scene, refActorTag))


@Meta(
    DisplayName='LOC("SHOW_VOICE_REF_MESSAGE")',
    DisplayDesc='LOC("SHOW_VOICE_REF_MESSAGE_DESC")',
    PathVars=[("voiceFileName", "Voices")],
)
@Latent(FinishedDialogue=(True,))
def ShowVoiceRefMessage(
    name: str,
    message: str,
    voiceFileName: str,
    refActor: Actor,
    minDistance: float = 64.0,
) -> Callable[[], bool]:
    r"""\brief Play a spatial voice clip relative to an actor and show a dialogue message on the current map scene."""
    scene = _getSceneMap()
    Manager.playVoice(voiceFileName, refActor=refActor, minDistance=float(minDistance))
    return scene.showMessage(name, message, refActor)


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
    return scene.showSelection(name, options, _getActorByTag(scene, refActorTag), bool(allowCancel))


@Meta(DisplayName='LOC("SHOW_REF_SELECTION")', DisplayDesc='LOC("SHOW_REF_SELECTION_DESC")')
@Latent(Selected0=(0,), Selected1=(1,), Selected2=(2,), Selected3=(3,), Cancelled=(-1,))
def ShowRefSelection(
    name: str,
    options: List[str],
    refActor: Actor,
    allowCancel: bool = True,
) -> Callable[[], Optional[int]]:
    r"""\brief Show a selection window on the current map scene positioned by a direct actor reference."""
    scene = _getSceneMap()
    return scene.showSelection(name, options, refActor, bool(allowCancel))


@Meta(DisplayName='LOC("LOCK_CAMERA")', DisplayDesc='LOC("LOCK_CAMERA_DESC")')
@ExecSplit(default=(None,))
def LockCamera() -> None:
    r"""\brief Lock the current map camera to the player."""
    scene = System.getScene()
    if scene and hasattr(scene, "getGameMap"):
        gameMap = scene.getGameMap()
        if gameMap is not None:
            camera = _getSceneCamera()
            player = gameMap.getPlayer()
            if camera is not None and player is not None:
                camera.setParent(player)
                _snapCameraToActor(camera, player)


@Meta(DisplayName='LOC("UNLOCK_CAMERA")', DisplayDesc='LOC("UNLOCK_CAMERA_DESC")')
@ExecSplit(default=(None,))
def UnlockCamera() -> None:
    r"""\brief Unlock the current map camera from the player."""
    camera = _getSceneCamera()
    if camera is not None:
        camera.setParent(None)


@Meta(DisplayName='LOC("ATTACH_CAMERA")', DisplayDesc='LOC("ATTACH_CAMERA_DESC")')
@ExecSplit(default=(None,))
def AttachCamera(actor: Optional[Actor] = None) -> None:
    r"""\brief Attach the current map camera to an actor, or detach it when None is passed.

    - \param actor The actor for the camera to follow, or None to stop following.
    """
    camera = _getSceneCamera()
    if camera is None:
        return
    camera.setParent(actor)
    if actor is not None:
        _snapCameraToActor(camera, actor)


@Meta(DisplayName='LOC("MOVE_CAMERA")', DisplayDesc='LOC("MOVE_CAMERA_DESC")', Vector2fVars=["delta"])
@ExecSplit(default=(None,))
def MoveCamera(delta: Any) -> None:
    r"""\brief Move the current map camera by a delta offset and clamp it to the map bounds.

    - \param delta The viewport offset to apply as `(x, y)`.
    """
    camera = _getSceneCamera()
    if camera is None:
        return
    deltaValue = _posToVector2f(delta)
    if deltaValue is None:
        return
    camera.moveView(deltaValue)
    camera.fixViewPosition()


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


@Meta(DisplayName='LOC("RECORD_ADDED_ACTOR")', DisplayDesc='LOC("RECORD_ADDED_ACTOR_DESC")')
@ExecSplit(default=(None,))
def RecordAddedActor(actor: Actor) -> None:
    r"""\brief Record an added actor for persistence on the current map scene.

    - \param actor The added actor.
    """
    scene = _getSceneMap()
    scene.recordAddedActor(actor)


@Meta(DisplayName='LOC("SELF_RECORD_ADDED")', DisplayDesc='LOC("SELF_RECORD_ADDED_DESC")')
@ExecSplit(default=(None,))
def SelfRecordAdded() -> None:
    r"""\brief Record the blueprint owner as an added actor for persistence on the current map scene."""
    actor = _getBlueprintOwner(SelfRecordAdded._refLocal)
    if actor is None:
        return
    scene = _getSceneMap()
    scene.recordAddedActor(actor)


@Meta(DisplayName='LOC("RECORD_ACTOR_POSITION")', DisplayDesc='LOC("RECORD_ACTOR_POSITION_DESC")')
@ExecSplit(default=(None,))
def RecordActorPosition(actor: Actor) -> None:
    r"""\brief Record an actor position change for persistence on the current map scene.

    - \param actor The moved actor.
    """
    scene = _getSceneMap()
    scene.recordActorPosition(actor)


@Meta(DisplayName='LOC("SELF_RECORD_ACTOR_POSITION")', DisplayDesc='LOC("SELF_RECORD_ACTOR_POSITION_DESC")')
@ExecSplit(default=(None,))
def SelfRecordActorPosition() -> None:
    r"""\brief Record the blueprint owner's position change for persistence on the current map scene."""
    actor = _getBlueprintOwner(SelfRecordActorPosition._refLocal)
    if actor is None:
        return
    scene = _getSceneMap()
    scene.recordActorPosition(actor)


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
    r"""\brief Record the blueprint owner as a destroyed actor for persistence on the current map scene."""
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
    r"""\brief Record the blueprint owner as destroyed for persistence and destroy it on the current map scene."""
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


@Meta(DisplayName='LOC("OPEN_ATTR_SHOP")', DisplayDesc='LOC("OPEN_ATTR_SHOP_DESC")')
@Latent(Closed=(True,))
def OpenAttrShop(
    actor: Actor = None,
    shopName: str = "",
    shopDescription: str = "",
    abilities: Dict[str, int] = {},
    price: Union[int, List[int]] = 0,
    priceIncrement: int = 1,
    moneyName: str = "GOLD",
) -> Callable[[], bool]:
    r"""\brief Open an attribute shop on the current map scene.

    - \param actor The actor whose avatar is shown.
    - \param shopName Locale key for the shop name.
    - \param shopDescription Locale key for the shop description.
    - \param abilities Mapping of player attribute names to purchased increments.
    - \param price Shared price, ordered price list, or game variable name containing either.
    - \param priceIncrement Amount added to the price after each purchase.
    - \param moneyName Player info component attribute used as currency.
    - \return A condition callable that becomes True when the shop closes.
    """

    def condition() -> bool:
        return True

    from Source.Scenes import Map as SceneMap

    scene = System.getScene()
    if not isinstance(scene, SceneMap):
        return condition
    from Source.NodeFunctions.Utils import _attrRef, _localRef

    if isinstance(price, (_attrRef, _localRef)):
        priceRef = price
    elif isinstance(price, str) and price:
        priceRef = _localRef(scene.inst.getVariables(), price, 0)
    else:
        priceValue = 0 if price == "" else price
        priceRef = _localRef({"price": priceValue}, "price", priceValue)
    return scene.openAttrShop(
        actor,
        shopName,
        shopDescription,
        dict(abilities),
        priceRef,
        int(priceIncrement),
        moneyName,
    )


@Meta(DisplayName='LOC("OPEN_ATTR_SHOP_BY_TAG")', DisplayDesc='LOC("OPEN_ATTR_SHOP_BY_TAG_DESC")')
@Latent(Closed=(True,))
def OpenAttrShopByTag(
    actorTag: str = "",
    shopName: str = "",
    shopDescription: str = "",
    abilities: Dict[str, int] = {},
    price: Union[int, List[int]] = 0,
    priceIncrement: int = 1,
    moneyName: str = "GOLD",
) -> Callable[[], bool]:
    r"""\brief Open an attribute shop on the current map scene.

    - \param actorTag Tag of the actor whose avatar is shown.
    - \param shopName Locale key for the shop name.
    - \param shopDescription Locale key for the shop description.
    - \param abilities Mapping of player attribute names to purchased increments.
    - \param price Shared price, ordered price list, or game variable name containing either.
    - \param priceIncrement Amount added to the price after each purchase.
    - \param moneyName Player info component attribute used as currency.
    - \return A condition callable that becomes True when the shop closes.
    """

    def condition() -> bool:
        return True

    from Source.Scenes import Map as SceneMap

    scene = System.getScene()
    if not isinstance(scene, SceneMap):
        return condition
    actor = scene.getGameMap().getActorByTag(actorTag) if actorTag else None

    return OpenAttrShop(actor, shopName, shopDescription, abilities, price, priceIncrement, moneyName)
