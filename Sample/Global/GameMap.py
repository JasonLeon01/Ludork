# -*- encoding: utf-8 -*-
r"""\brief GameMap: manages tile layers, actors, lights, collisions, and pathfinding for a single map."""

from __future__ import annotations
from collections import deque
import copy
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Union
import Engine
from Engine import (
    Pair,
    RenderTexture,
    RenderStates,
    RenderTarget,
    Vector2i,
    Vector2f,
    Vector2u,
    Color,
    Texture,
    Direction,
    OppositeDirection,
    Shader,
    ParticleSystem,
)
from Engine.Utils import Math
from Engine.Utils.Inner import IS_IOS_PLATFORM, warnIosShaderSkippedOnce
from Engine.Gameplay import Tilemap, TileLayer, Material
from Engine.Gameplay.Actors import Actor
from . import SceneBase
from .GlobalExt import GameMapExt
from .Camera import Camera
from .System import System
from .Components import MapClickAutoPath, PathPreviewComponent, PathRouteState, ComponentBase
from .Weather import WeatherController
from .Fog import FogController


@dataclass
class Light:
    r"""\brief Light source with position, colour, radius, and intensity."""

    position: Vector2f
    color: Color
    radius: float = 256.0
    intensity: float = 1.0

    def asDict(self) -> Dict[str, Any]:
        r"""\brief Serialize the light to a dictionary.

        - \return A dictionary with position, color, radius, and intensity.
        """
        return {
            "position": [self.position.x, self.position.y],
            "color": [self.color.r, self.color.g, self.color.b, self.color.a],
            "radius": self.radius,
            "intensity": self.intensity,
        }

    @staticmethod
    def fromDict(data: Dict[str, Any]) -> Light:
        r"""\brief Deserialize a light from a dictionary.

        - \param data The serialised light data.

        - \return A new Light instance.
        """
        return Light(
            Vector2f(*data["position"]),
            Color(*data["color"]),
            data.get("radius", 256.0),
            data.get("intensity", 1.0),
        )


class GameMap(GameMapExt):
    r"""\brief Game map managing tile layers, actors, lights, collisions, and pathfinding.

    Provides the core gameplay map with lighting, occlusion, actor management,
    and pathfinding support. Integrates with Camera and SceneBase.
    """

    DefaultCoverAlpha: int
    MapViewOffset: Vector2f = Vector2f(192.0, 32.0)

    def __init__(self, mapName: str, tilemap: Tilemap, camera: Optional[Camera] = None) -> None:
        r"""\brief Construct a game map.

        - \param mapName The name of the map.
        - \param tilemap The tilemap data.
        - \param camera Optional camera; a default one is created if not provided.
        """
        self.mapName = mapName
        self._scene: SceneBase = None
        self._tilemap = tilemap
        self._layersTopFirst = list(self._tilemap.getAllLayers().values())
        self._layersTopFirst.reverse()
        self._actors: Dict[str, List[Actor]] = {}
        self._particleSystem: ParticleSystem = ParticleSystem()
        self._actorsOnDestroy: List[Actor] = []
        self._wholeActorList: Dict[str, List[Actor]] = {}
        self._camera = camera
        if self._camera is None:
            self._camera = Camera()
        self._camera.setMap(self)
        self._lights: List[Light] = []
        self._ambientLight: Color = Color(255, 255, 255, 255)
        self._lightMask: Optional[RenderTexture] = None
        self._materialDirty: bool = True
        self._tilePassableGrid: Optional[List[List[bool]]] = None
        self._occupancyMap: Dict[Pair[int], List[Actor]] = {}
        self._player: Optional[Actor] = None
        self._pathRouteState = PathRouteState()
        self._components: List[ComponentBase] = [
            MapClickAutoPath(self, self._pathRouteState),
            PathPreviewComponent(self, self._pathRouteState),
        ]
        self._lightMask = RenderTexture(self._tilemap.getSize() * Engine.CellSize)
        if IS_IOS_PLATFORM:
            warnIosShaderSkippedOnce(
                "GameMap.mapShaders",
                "iOS: shaders are disabled; skipped map lighting shaders",
            )
            self._tilemapLightMaskShader = None
            self._lightMaskShader = None
            self._materialShader = None
            self._tilemapRenderStates = copy.copy(self._camera.getRenderStates())
            self._actorRenderStates = copy.copy(self._camera.getRenderStates())
        else:
            self._tilemapLightMaskShader = Shader("./Assets/Shaders/Map/TilemapLightMask.frag", Shader.Type.Fragment)
            self._lightMaskShader = Shader("./Assets/Shaders/Map/lightMask.frag", Shader.Type.Fragment)
            self._materialShader = Shader("./Assets/Shaders/Map/Material.frag", Shader.Type.Fragment)
            self._tilemapRenderStates = copy.copy(self._camera.getRenderStates())
            self._tilemapRenderStates.shader = self._tilemapLightMaskShader
            self._actorRenderStates = copy.copy(self._camera.getRenderStates())
            self._actorRenderStates.shader = self._lightMaskShader
        super().__init__(self._materialShader)
        self.tilemapRef = self._tilemap
        self.actorsRef = self._actors
        self.getLayer = Tilemap.getLayer
        self.getMapPosition = Actor.getMapPosition
        self.getCollisionEnabled = Actor.getCollisionEnabled
        self.TileLayerPassable = TileLayer.isPassable

    @ReturnType(player=Actor)
    def getPlayer(self) -> Optional[Actor]:
        r"""\brief Get the player actor.

        - \return The player actor, or None.
        """
        return self._player

    @ExecSplit(default=(None,))
    def setPlayer(self, player: Optional[Actor]) -> None:
        r"""\brief Set the player actor and parent the camera to it.

        - \param player The player actor to set, or None.
        """
        if not self._camera:
            return
        if self._player is None:
            self._camera.setParent(player)
        self._player = player

    @ReturnType(actors=List[Actor])
    def getAllActors(self) -> List[Actor]:
        r"""\brief Get all actors across all layers.

        - \return A flat list of all actors.
        """
        actors = []
        for actorList in self._actors.values():
            actors.extend(actorList)
        return actors

    @ReturnType(actors=List[Actor])
    def getActorsByPosition(self, position: Vector2i) -> List[Actor]:
        r"""\brief Get actors at a specific map position.

        - \param position The map position to query.

        - \return A list of actors at the given position.
        """
        return self._occupancyMap.get((position.x, position.y), [])

    @ReturnType(actor=Actor)
    def getActorByLayerAndPosition(self, layer: str, position: Vector2i) -> Optional[Actor]:
        r"""\brief Get the first actor on a given layer at a position.

        - \param layer The layer name.
        - \param position The map position.

        - \return The matching actor, or None.
        """
        actors = self._actors.get(layer, [])
        for actor in actors:
            if actor.getPosition() == position:
                return actor
        return None

    @ReturnType(actors=List[Actor])
    def getActorsByRange(self, position: Vector2i, radius: int) -> List[Actor]:
        r"""\brief Get actors within a range of a position.

        - \param position The centre position.
        - \param radius The search radius in tiles.

        - \return A list of actors within the range.
        """
        actors = []
        for x in range(position.x - radius, position.x + radius + 1):
            for y in range(position.y - radius, position.y + radius + 1):
                actors.extend(self._occupancyMap.get((x, y), []))
        return actors

    @ReturnType(actors=List[Actor])
    def getAllActorsByTag(self, tag: str) -> List[Actor]:
        r"""\brief Get all actors with a given tag.

        - \param tag The tag to search for.

        - \return A list of matching actors.
        """
        actors = []
        for actorList in self._actors.values():
            for actor in actorList:
                if actor.tag == tag:
                    actors.append(actor)
        return actors

    def removeActorsByTags(self, tags: List[str]) -> None:
        r"""\brief Remove actors matching any tag from the map without replaying destroy events.

        - \param tags Actor tags to remove.
        """
        if not tags:
            return
        tagSet = set(tags)
        removed = False
        for layerName, actorList in self._actors.items():
            keptActors = [actor for actor in actorList if actor.tag not in tagSet]
            if len(keptActors) != len(actorList):
                self._actors[layerName] = keptActors
                removed = True
        if removed:
            self.updateActorList()
            self._materialDirty = True

    @ReturnType(passable=bool)
    def isPassable(self, actor: Actor, targetPosition: Vector2i) -> bool:
        r"""\brief Check if an actor can move to a target position.

        Considers tile passability, direction, and actor collision.

        - \param actor The moving actor.
        - \param targetPosition The target map position.

        - \return True if the position is passable.
        """
        if not actor.getCollisionEnabled():
            return True
        size = self._tilemap.getSize()
        x = targetPosition.x
        y = targetPosition.y
        if x < 0 or y < 0 or x >= size.x or y >= size.y:
            return False
        if self._tilePassableGrid is None or self._occupancyMap is None or self._materialDirty:
            self._rebuildPassabilityCache()
            self._materialDirty = False
        if self._tilePassableGrid and not self._tilePassableGrid[y][x]:
            return False

        currentPosition = actor.getMapPosition()
        direction = None
        delta = targetPosition - currentPosition
        if delta.x == 0 and delta.y == 1:
            direction = Direction.DOWN
        elif delta.x == 0 and delta.y == -1:
            direction = Direction.UP
        elif delta.x == 1 and delta.y == 0:
            direction = Direction.RIGHT
        elif delta.x == -1 and delta.y == 0:
            direction = Direction.LEFT

        if direction is not None:
            dirIndex = int(direction)
            oppIndex = int(OppositeDirection(direction))
            currBlocked = False
            nextBlocked = False

            for layer in self._layersTopFirst:
                if not layer.visible:
                    continue

                if not currBlocked:
                    tileCurr = layer.get(currentPosition)
                    if tileCurr is not None:
                        ts = layer._data.layerTileset
                        d4 = ts.dir4[tileCurr] if hasattr(ts, "dir4") and tileCurr < len(ts.dir4) else None
                        if isinstance(d4, (list, tuple)) and len(d4) == 4:
                            if not d4[dirIndex]:
                                return False
                        currBlocked = True

                if not nextBlocked:
                    tileNext = layer.get(targetPosition)
                    if tileNext is not None:
                        ts = layer._data.layerTileset
                        d4 = ts.dir4[tileNext] if hasattr(ts, "dir4") and tileNext < len(ts.dir4) else None
                        if isinstance(d4, (list, tuple)) and len(d4) == 4:
                            if not d4[oppIndex]:
                                return False
                        nextBlocked = True

                if currBlocked and nextBlocked:
                    break

        occ = self._occupancyMap.get((x, y))
        if occ:
            for other in occ:
                if other is actor:
                    continue
                if other in actor.getChildren():
                    continue
                if other.isDestroyed():
                    continue
                if other.getCollisionEnabled():
                    return False
        return True

    @ExecSplit(default=(None,))
    def spawnActor(self, actor: Actor, layer: str, emitCreateEvent: bool = True) -> None:
        r"""\brief Spawn an actor on a layer.

        - \param actor The actor to spawn.
        - \param layer The layer name to place the actor on.
        - \param emitCreateEvent Whether to run the actor's onCreate blueprint event.
        """
        if layer not in self._actors:
            self._actors[layer] = []
        actor.setMap(self)
        self._actors[layer].append(actor)
        if emitCreateEvent:
            Actor.BlueprintEvent(actor, Actor, "onCreate")
        children = actor.getChildren()
        if children:
            for child in children:
                if not child in self._wholeActorList:
                    self.spawnActor(child, layer, emitCreateEvent)
        self.updateActorList()
        self._materialDirty = True

    @ExecSplit(default=(None,))
    def destroyActor(self, actor: Actor) -> None:
        r"""\brief Queue an actor for destruction on the next tick.

        - \param actor The actor to destroy.
        """
        self._actorsOnDestroy.append(actor)
        self._materialDirty = True

    @ReturnType(camera=Camera)
    def getCamera(self) -> Optional[Camera]:
        r"""\brief Get the camera attached to this map.

        - \return The Camera, or None.
        """
        return self._camera

    @ExecSplit(default=(None,))
    def setCamera(self, camera: Camera) -> None:
        r"""\brief Set the camera for this map.

        - \param camera The camera to set.
        """
        self._camera = camera

    @ReturnType(tilemap=Tilemap)
    def getTilemap(self) -> Tilemap:
        r"""\brief Get the tilemap.

        - \return The Tilemap.
        """
        return self._tilemap

    @ReturnType(lights=List[Light])
    def getLights(self) -> List[Light]:
        r"""\brief Get all lights on the map.

        - \return A list of Light objects.
        """
        return self._lights

    @ExecSplit(default=(None,))
    def setLights(self, lights: List[Light]) -> None:
        r"""\brief Replace all lights on the map.

        - \param lights The new list of Light objects.
        """
        self._lights = lights

    @ExecSplit(default=(None,))
    def addLight(self, light: Light) -> None:
        r"""\brief Add a light to the map.

        - \param light The Light to add.
        """
        self._lights.append(light)

    @ExecSplit(default=(None,))
    def removeLight(self, light: Light) -> None:
        r"""\brief Remove a light from the map.

        - \param light The Light to remove.
        """
        self._lights.remove(light)

    @ExecSplit(default=(None,))
    @TypeAdapter(position=(tuple, Vector2f))
    def setLightPosition(self, light: Light, position: Union[Vector2f, Pair[float]]) -> None:
        r"""\brief Set the position of a light.

        - \param light The light to modify.
        - \param position The new position.
        """
        assert light in self._lights, "Light not found in map"
        light.position = position

    @ExecSplit(default=(None,))
    def setLightColor(self, light: Light, color: Color) -> None:
        r"""\brief Set the colour of a light.

        - \param light The light to modify.
        - \param color The new colour.
        """
        assert light in self._lights, "Light not found in map"
        light.color = color

    @ExecSplit(default=(None,))
    def setLightRadius(self, light: Light, radius: float) -> None:
        r"""\brief Set the radius of a light.

        - \param light The light to modify.
        - \param radius The new radius.
        """
        assert light in self._lights, "Light not found in map"
        light.radius = radius

    @ExecSplit(default=(None,))
    def setLightIntensity(self, light: Light, intensity: float) -> None:
        r"""\brief Set the intensity of a light.

        - \param light The light to modify.
        - \param intensity The new intensity.
        """
        assert light in self._lights, "Light not found in map"
        light.intensity = intensity

    @ReturnType(ambientLight=Color)
    def getAmbientLight(self) -> Color:
        r"""\brief Get the ambient light colour.

        - \return The ambient light Colour.
        """
        return self._ambientLight

    @ExecSplit(default=(None,))
    def setAmbientLight(self, ambientLight: Color) -> None:
        r"""\brief Set the ambient light colour.

        - \param ambientLight The new ambient light Colour.
        """
        self._ambientLight = ambientLight

    @ReturnType(size=Vector2u)
    def getSize(self) -> Vector2u:
        r"""\brief Get the map size in tiles.

        - \return The map size.
        """
        return self._tilemap.getSize()

    @ReturnType(topMaterial=Optional[Material])
    def getTopMaterial(self, pos: Vector2i) -> Optional[Material]:
        r"""\brief Get the topmost visible material at a position.

        - \param pos The map position.

        - \return The top Material, or None.
        """
        layers = self._tilemap.getAllLayers()
        layerKeys = list(layers.keys())
        layerKeys.reverse()
        for layerName in layerKeys:
            layer = layers[layerName]
            if layer is None or not layer.visible:
                continue
            if layerName in self._actors:
                for actor in self._actors[layerName]:
                    if actor == self._player:
                        continue
                    if actor.getMapPosition() == pos:
                        return actor.getMaterial()
            material = layer.getMaterial(pos)
            if material is not None:
                return material
        return None

    @ReturnType(path=List[Vector2i])
    def findPath(self, start: Vector2i, goal: Vector2i) -> List[Vector2i]:
        r"""\brief Find a path from start to goal using pathfinding.

        - \param start The starting position.
        - \param goal The goal position.

        - \return A list of positions forming the path.
        """
        size = self._tilemap.getSize()
        layerKeys = list(self._tilemap.getAllLayers().keys())
        layerKeys.reverse()
        self.layerKeysRef = layerKeys
        self.tileDataRef = self._tilemap.getTilesData()
        self.autoTileDataRef = self._tilemap.getAutoTilesData()
        self.actorsRef = self._actors
        return self.findPathExt(start, goal, size)

    @ReturnType(scene=SceneBase)
    def getScene(self) -> SceneBase:
        r"""\brief Get the scene this map belongs to.

        - \return The parent SceneBase.
        """
        return self._scene

    def setScene(self, scene: SceneBase) -> None:
        r"""\brief Set the scene this map belongs to.

        - \param scene The parent scene.
        """
        self._scene = scene

    @ExecSplit(default=(None,))
    def addCommonTip(self, text: str) -> None:
        r"""\brief Display a floating tip text in the parent scene.

        - \param text The tip message to display.
        """
        if self._scene is not None:
            self._scene.addCommonTip(text)

    def getCollision(self, actor: Actor, targetPosition: Vector2i) -> List[Actor]:
        r"""\brief Get all actors colliding with a given actor at a target position.

        - \param actor The querying actor.
        - \param targetPosition The map position to check.

        - \return A list of colliding actors.
        """
        if not actor.getCollisionEnabled():
            return []
        result: List[Actor] = []
        for actorList in self._actors.values():
            for other in actorList:
                if actor is other:
                    continue
                if not other.getCollisionEnabled():
                    continue
                if other in actor.getChildren():
                    continue
                if other.isDestroyed():
                    continue
                if other.getMapPosition() == targetPosition:
                    result.append(other)
        return result

    def getOverlaps(self, actor: Actor) -> List[Actor]:
        r"""\brief Get all actors overlapping with the given actor.

        - \param actor The querying actor.

        - \return A list of overlapping actors.
        """
        result: List[Actor] = []
        for actorList in self._actors.values():
            for other in actorList:
                if actor is other:
                    continue
                if other in actor.getChildren():
                    continue
                if other.isDestroyed():
                    continue
                if actor.getMapPosition() == other.getMapPosition():
                    result.append(other)
        return result

    def updateActorList(self) -> None:
        r"""\brief Rebuild the flat actor list from all layers, including children."""
        self._wholeActorList.clear()
        for layerName, actorList in self._actors.items():
            self._wholeActorList[layerName] = []
            q = deque(actorList)
            while q:
                child = q.popleft()
                child.setMap(self)
                self._wholeActorList[layerName].append(child)
                if child.getChildren():
                    q.extend(child.getChildren())

    def getMaterialPropertyMap(
        self, functionName: str, invalidValue: Union[float, bool]
    ) -> List[List[Union[float, bool]]]:
        r"""\brief Get a 2D map of material property values.

        - \param functionName The material property function name.
        - \param invalidValue The default value for invalid positions.

        - \return A 2D grid of property values.
        """
        mapSize = self._tilemap.getSize()
        width = mapSize.x
        height = mapSize.y
        layerKeys = list(self._tilemap.getAllLayers().keys())
        layerKeys.reverse()
        self.layerKeysRef = layerKeys
        self.tileDataRef = self._tilemap.getTilesData()
        self.autoTileDataRef = self._tilemap.getAutoTilesData()
        self.actorsRef = self._actors
        return self.getMaterialPropertyMapExt(width, height, functionName, invalidValue)

    def getActorLayerLightBlockMap(self, layerName: str, size: Vector2u) -> Optional[List[List[float]]]:
        r"""\brief Get a light blocking map for a specific actor layer.

        - \param layerName The layer name.
        - \param size The map size.

        - \return A 2D grid of light block values, or None.
        """
        width: int = size.x
        height: int = size.y
        if layerName not in self._actors:
            return None
        result = [[0.0] * width for _ in range(height)]
        for actor in self._actors[layerName]:
            position = actor.getMapPosition()
            result[position.y][position.x] = actor.getLightBlock()
        return result

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update all actors, components, and particles each frame.

        - \param deltaTime Elapsed time in seconds.
        """
        if not hasattr(self, "_shaderTime"):
            self._shaderTime = 0.0
        self._shaderTime += deltaTime
        if self._camera:
            self._camera.onTick(deltaTime)
        for component in self._components:
            component.onTick()
        if len(self._actorsOnDestroy) > 0:
            for actor in self._actorsOnDestroy:
                for actorList in self._actors.values():
                    for i, listed in enumerate(actorList):
                        if listed is actor:
                            actorList.pop(i)
                            Actor.BlueprintEvent(actor, Actor, "onDestroy")
                            break
            self.updateActorList()
            self._actorsOnDestroy.clear()
        for actorList in self._actors.values():
            for actor in actorList:
                actor.update(deltaTime)
                if actor.getTickable():
                    Actor.BlueprintEvent(actor, Actor, "onTick", {"deltaTime": deltaTime})
        self._particleSystem.onTick(deltaTime)

    def onLateTick(self, deltaTime: float) -> None:
        r"""\brief Late-update all actors, components, and particles each frame.

        - \param deltaTime Elapsed time in seconds.
        """
        if self._camera:
            self._camera.onLateTick(deltaTime)
        for component in self._components:
            component.onLateTick()
        for actorList in self._actors.values():
            for actor in actorList:
                actor.lateUpdate(deltaTime)
                if actor.getTickable():
                    Actor.BlueprintEvent(actor, Actor, "onLateTick", {"deltaTime": deltaTime})
        self._particleSystem.onLateTick(deltaTime)

    def onFixedTick(self, fixedDelta: float) -> None:
        r"""\brief Fixed-timestep update for all actors and components.

        - \param fixedDelta Fixed timestep in seconds.
        """
        if self._camera:
            self._camera.onFixedTick(fixedDelta)
        for component in self._components:
            component.onFixedTick()
        for actorList in self._actors.values():
            for actor in actorList:
                actor.fixedUpdate(fixedDelta)
                if actor.getTickable():
                    Actor.BlueprintEvent(actor, Actor, "onFixedTick", {"fixedDelta": fixedDelta})

    def drawMapContent(
        self,
        target: RenderTarget,
        states: Optional[RenderStates] = None,
        applyPlayerCover: bool = False,
    ) -> None:
        r"""\brief Draw visible tile layers and actors to a render target.

        - \param target Render target receiving the map content.
        - \param states Optional render states for normal map draws.
        - \param applyPlayerCover Whether to apply the player cover transparency pass.
        """
        if states is None:
            states = RenderStates()
        if applyPlayerCover:
            self._resetTransparentTiles()
        layers = self._tilemap.getAllLayers()
        layerKeys = list(layers.keys())
        playerLayerIndex = self._getPlayerLayerIndex(layerKeys) if applyPlayerCover else -1

        for i, layerName in enumerate(layerKeys):
            layer = layers[layerName]
            if not layer.visible:
                continue

            if applyPlayerCover:
                self._applyPlayerCover(layer, i, playerLayerIndex)

            target.draw(layer, states)
            self._drawLayerActors(target, states, layerName, i, playerLayerIndex, applyPlayerCover)

    def show(self) -> None:
        r"""\brief Render the full map including layers, actors, lights, and particles."""
        layers = self._tilemap.getAllLayers()
        layerKeys = list(layers.keys())
        System.setWindowMapView(GameMap.MapViewOffset)
        if self._camera:
            self._camera.clear()
        if self._lightMask:
            self._lightMask.clear()

        if self._camera:
            self.drawMapContent(
                self._camera.getRenderTexture(),
                self._camera.getRenderStates(),
                applyPlayerCover=True,
            )
        else:
            self._resetTransparentTiles()
        for component in self._components:
            component.onRender(self._camera)
        useLightMaskShaders = self._tilemapLightMaskShader is not None and self._lightMaskShader is not None
        if useLightMaskShaders:
            tilemapLightMask = Cast(Shader, self._tilemapLightMaskShader)
            actorLightMask = Cast(Shader, self._lightMaskShader)
            for layerName in layerKeys:
                layer = layers[layerName]
                if not layer.visible:
                    continue
                cellSize = Engine.CellSize
                lbTexture = Texture(layer.getLightBlockImage())
                reflectionStrengthTexture = Texture(layer.getReflectionStrengthImage())
                tilemapLightMask.setUniform("lightBlockTex", lbTexture)
                tilemapLightMask.setUniform("reflectionStrengthTex", reflectionStrengthTexture)
                tilemapLightMask.setUniform("lightBlockSize", Vector2f(cellSize, cellSize))
                tilemapLightMask.setUniform("mapSize", Vector2f(self._tilemap.getSize().x, self._tilemap.getSize().y))
                if self._lightMask:
                    self._lightMask.draw(layer, self._tilemapRenderStates)
                    if layerName in self._actors:
                        for actor in self._actors[layerName]:
                            actorLightMask.setUniform("lightBlock", actor.getLightBlock())
                            if actor.getMirror():
                                actorLightMask.setUniform("reflectionStrength", actor.getReflectionStrength())
                            else:
                                actorLightMask.setUniform("reflectionStrength", 0.0)
                            self._lightMask.draw(actor, self._actorRenderStates)
        elif IS_IOS_PLATFORM:
            warnIosShaderSkippedOnce(
                "GameMap.show.lightMaskPass",
                "iOS: shaders are disabled; skipped map light-mask render pass",
            )
        if self._camera:
            self._camera.display()
        if self._lightMask:
            self._lightMask.display()
        self.refreshShader()
        if self._camera:
            WeatherController.drawShaderOverlay(self._camera)
        System.draw(self._camera, self._materialShader)
        FogController.drawOverlay()
        WeatherController.registerParticleSystem(self._particleSystem)
        System.draw(self._particleSystem)
        System.setWindowDefaultView()

    def refreshShader(self) -> None:
        r"""\brief Refresh the material shader uniforms with current lighting data."""
        if self._lightMask is None or self._materialDirty:
            self._rebuildPassabilityCache()
            self._materialDirty = False
        if self._camera:
            super().refreshShader(
                self._lightMask,
                System.getScale(),
                Math.ToVector2f(System.getGameSize()),
                self._camera.getViewPosition(),
                self._camera.v_getViewRotation(),
                Math.ToVector2f(self._tilemap.getSize()),
                Engine.CellSize,
                self._lights,
                self._ambientLight,
            )
            if self._materialShader is not None:
                self._materialShader.setUniform("mapViewOffset", GameMap.MapViewOffset)

    def markPassabilityDirty(self) -> None:
        r"""\brief Mark the passability cache as dirty for rebuild on next query."""
        self._materialDirty = True

    def _resetTransparentTiles(self) -> None:
        if not hasattr(self, "_transparentTiles"):
            self._transparentTiles = []
        if not self._transparentTiles:
            return
        for layer, x, y in self._transparentTiles:
            if hasattr(layer, "resetTileColor"):
                layer.resetTileColor(x, y)
        self._transparentTiles.clear()

    def _getPlayerLayerIndex(self, layerKeys: List[str]) -> int:
        if not self._player:
            return -1
        for i, name in enumerate(layerKeys):
            if name in self._actors and self._player in self._actors[name]:
                return i
        return -1

    def _applyPlayerCover(self, layer: TileLayer, layerIndex: int, playerLayerIndex: int) -> None:
        if not self._player or layerIndex <= playerLayerIndex or playerLayerIndex == -1:
            return
        playerPos = self._player.getMapPosition()
        if layer.get(playerPos) is None:
            return
        if hasattr(layer, "floodFillTransparent"):
            processed = layer.floodFillTransparent(
                playerPos.x, playerPos.y, Color(255, 255, 255, GameMap.DefaultCoverAlpha)
            )
            for x, y in processed:
                self._transparentTiles.append((layer, x, y))
        elif hasattr(layer, "setTileColor"):
            layer.setTileColor(playerPos.x, playerPos.y, Color(255, 255, 255, GameMap.DefaultCoverAlpha))
            self._transparentTiles.append((layer, playerPos.x, playerPos.y))

    def _drawLayerActors(
        self,
        target: RenderTarget,
        states: RenderStates,
        layerName: str,
        layerIndex: int,
        playerLayerIndex: int,
        applyPlayerCover: bool,
    ) -> None:
        if layerName not in self._actors:
            return
        for actor in self._actors[layerName]:
            actorAlpha = 255
            if (
                applyPlayerCover
                and self._player
                and layerIndex > playerLayerIndex
                and playerLayerIndex != -1
                and actor != self._player
                and actor.intersects(self._player)
            ):
                actorAlpha = GameMap.DefaultCoverAlpha
            self._drawActor(target, states, actor, actorAlpha)

    def _drawActor(self, target: RenderTarget, states: RenderStates, actor: Actor, actorAlpha: int) -> None:
        if actor.hasShaderError():
            actor.setColor(Color(255, 0, 255, actorAlpha))
            target.draw(actor, states)
            return

        actor.setColor(Color(255, 255, 255, actorAlpha))
        actorShader = actor.getShader()
        if actorShader:
            try:
                actorShader.setUniform("time", getattr(self, "_shaderTime", 0.0))
            except Exception:
                pass
            renderStates = RenderStates()
            renderStates.shader = actorShader
            target.draw(actor, renderStates)
            return
        target.draw(actor, states)

    def _getMaterialPropertyTexture(
        self, functionName: str, invalidValue: Union[float, bool], smooth: bool = False
    ) -> Texture:
        size = self._tilemap.getSize()
        materialMap = self.getMaterialPropertyMap(functionName, invalidValue)
        return self.generateDataFromMap(size, materialMap, smooth)

    def _rebuildPassabilityCache(self) -> None:
        size = self._tilemap.getSize()
        layerKeysList = list(self._tilemap.getAllLayers().keys())
        layerKeysList.reverse()
        self.layerKeysRef = layerKeysList
        self.tileDataRef = self._tilemap.getTilesData()
        self.autoTileDataRef = self._tilemap.getAutoTilesData()
        self.actorsRef = self._actors
        self._tilePassableGrid, self._occupancyMap = self.rebuildPassabilityCache(size)
