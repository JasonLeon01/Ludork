# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
from typing import Any, List, Optional, Tuple, Union
from ... import Pair, BPBase, Vector2f, Vector2i, Vector2u, Vector3f, IntRect, Texture, Sound, Listener
from ...Utils import Math, Inner
from ...Filters import SoundFilter
from ... import Material
from ..Components import ChildActorComponent, LightComponent, componentFromData
from .Base import _ActorBase


@Meta(
    VariableDisplayNames={
        "volume": 'LOC("AUTO_SOUND_VAR_VOLUME")',
        "minDistance": 'LOC("AUTO_SOUND_VAR_MIN_DISTANCE")',
        "attenuation": 'LOC("AUTO_SOUND_VAR_ATTENUATION")',
    },
    VariableDisplayDescs={
        "volume": 'LOC("AUTO_SOUND_VAR_VOLUME_DESC")',
        "minDistance": 'LOC("AUTO_SOUND_VAR_MIN_DISTANCE_DESC")',
        "attenuation": 'LOC("AUTO_SOUND_VAR_ATTENUATION_DESC")',
    },
)
@dataclass
class AutoSoundParams:
    r"""\brief Spatial audio parameters used by Actor automatic sound playback."""

    volume: float = 100.0  #: Base sound volume
    minDistance: float = 64.0  #: Distance before attenuation starts
    attenuation: float = 1.0  #: Distance attenuation factor
    loop: bool = False  #: Loop playback continuously
    maxDistance: float = 0.0  #: Stop beyond this listener distance; 0 means unlimited


@Meta(
    PathVars=[("texturePath", "Characters"), ("shaderPath", "Shaders"), ("autoSound", "Sounds")],
    Vector2fVars=["defaultTranslation", "defaultScale", "defaultOrigin"],
    RectRangeVars={"defaultRect": "texturePath"},
    Rely={
        "autoSoundInterval": {"source": "autoSound", "op": "!=", "value": ""},
        "autoSoundParams": {"source": "autoSound", "op": "!=", "value": ""},
    },
    VariableDisplayNames={
        "tickable": 'LOC("ACTOR_VAR_TICKABLE")',
        "speed": 'LOC("ACTOR_VAR_SPEED")',
        "autoSound": 'LOC("ACTOR_VAR_AUTO_SOUND")',
        "autoSoundInterval": 'LOC("ACTOR_VAR_AUTO_SOUND_INTERVAL")',
        "autoSoundParams": 'LOC("ACTOR_VAR_AUTO_SOUND_PARAMS")',
        "texturePath": 'LOC("ACTOR_VAR_TEXTURE_PATH")',
        "defaultRect": 'LOC("ACTOR_VAR_DEFAULT_RECT")',
        "defaultTranslation": 'LOC("ACTOR_VAR_DEFAULT_TRANSLATION")',
        "defaultRotation": 'LOC("ACTOR_VAR_DEFAULT_ROTATION")',
        "defaultScale": 'LOC("ACTOR_VAR_DEFAULT_SCALE")',
        "defaultOrigin": 'LOC("ACTOR_VAR_DEFAULT_ORIGIN")',
        "lightComp": 'LOC("ACTOR_VAR_LIGHT_COMP")',
        "childActorComp": 'LOC("ACTOR_VAR_CHILD_ACTOR_COMP")',
    },
    VariableDisplayDescs={
        "tickable": 'LOC("ACTOR_VAR_TICKABLE_DESC")',
        "speed": 'LOC("ACTOR_VAR_SPEED_DESC")',
        "autoSound": 'LOC("ACTOR_VAR_AUTO_SOUND_DESC")',
        "autoSoundInterval": 'LOC("ACTOR_VAR_AUTO_SOUND_INTERVAL_DESC")',
        "autoSoundParams": 'LOC("ACTOR_VAR_AUTO_SOUND_PARAMS_DESC")',
        "texturePath": 'LOC("ACTOR_VAR_TEXTURE_PATH_DESC")',
        "defaultRect": 'LOC("ACTOR_VAR_DEFAULT_RECT_DESC")',
        "defaultTranslation": 'LOC("ACTOR_VAR_DEFAULT_TRANSLATION_DESC")',
        "defaultRotation": 'LOC("ACTOR_VAR_DEFAULT_ROTATION_DESC")',
        "defaultScale": 'LOC("ACTOR_VAR_DEFAULT_SCALE_DESC")',
        "defaultOrigin": 'LOC("ACTOR_VAR_DEFAULT_ORIGIN_DESC")',
        "lightComp": 'LOC("ACTOR_VAR_LIGHT_COMP_DESC")',
        "childActorComp": 'LOC("ACTOR_VAR_CHILD_ACTOR_COMP_DESC")',
    },
)
class Actor(_ActorBase, BPBase):
    r"""
    \brief Game actor with collision, movement, and blueprint event support.

    Extends `_ActorBase` with grid-based movement, collision detection,
    route (pathfinding) execution, and blueprint event dispatching.
    """

    tickable: bool = False  #: Whether tick events are dispatched
    speed: float = 64.0  #: Movement speed in pixels per second
    autoSound: str = ""  #: Sound asset played automatically as a spatial sound
    autoSoundInterval: float = 0.0  #: Delay between one automatic sound ending and the next starting
    autoSoundParams: AutoSoundParams = AutoSoundParams()  #: Parameters applied to the automatic spatial sound
    _componentTypes = {"lightComp": LightComponent, "childActorComp": ChildActorComponent}
    ### Generation use only
    texturePath: str = ""  #: Asset path to the character texture
    defaultRect: Optional[Tuple[Pair[int], Pair[int]]] = ((0, 0), (32, 32))  #: Default texture rectangle (origin, size)
    defaultTranslation: Pair[float] = (0.0, 0.0)  #: Default position offset
    defaultRotation: float = 0.0  #: Default rotation in degrees
    defaultScale: Pair[float] = (1.0, 1.0)  #: Default scale factors (x, y)
    defaultOrigin: Pair[float] = (0.0, 0.0)  #: Default origin point for transformations
    ### Generation use only

    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        r"""
        \brief Initialise an Actor instance.

        Creates the actor with optional texture, texture rectangle, and tag.
        Initialises movement and route state.

        - \param texture  Optional single texture or list of textures
        - \param rect     Optional texture rectangle or (origin, size) pair
        - \param tag      Optional tag string for actor identification
        """
        _ActorBase.__init__(self, texture, rect, tag)
        BPBase.__init__(self)
        self._normaliseLightComp()
        self._normaliseAutoSoundParams()
        self._isMoving: bool = False
        self._inRoute: bool = False
        self._route: List[Vector2i] = []
        self._moveEnabled: bool = True
        self._departure: Optional[Vector2f] = None
        self._destination: Optional[Vector2f] = None
        self._moveOriginMapPos: Optional[Vector2i] = None
        self._realSpeed: float = 0.0
        self._autoSoundObject: Optional[Sound] = None
        self._autoSoundCooldown: float = 0.0
        self._autoSoundLastPosition: Optional[Vector3f] = None
        self._applyCollisionEnabledClassDefault()

    def _applyCollisionEnabledClassDefault(self) -> None:
        for cls in type(self).__mro__:
            if cls is object:
                break
            if cls.__name__ == "ActorCore":
                continue
            if "collisionEnabled" not in cls.__dict__:
                continue
            default = cls.__dict__["collisionEnabled"]
            if isinstance(default, bool):
                self.setCollisionEnabled(default)
                return

    @property
    def lightColour(self) -> Tuple[int, int, int, int]:
        comp = self._getLightComp()
        return (255, 255, 255, 255) if comp is None else comp.lightColour

    @lightColour.setter
    def lightColour(self, value: Tuple[int, int, int, int]) -> None:
        self._ensureLightComp().lightColour = value

    @property
    def lightRadius(self) -> float:
        comp = self._getLightComp()
        return 16.0 if comp is None else comp.lightRadius

    @lightRadius.setter
    def lightRadius(self, value: float) -> None:
        self._ensureLightComp().lightRadius = float(value)

    def _normaliseLightComp(self) -> None:
        if "lightComp" not in self.__dict__:
            return
        value = self.__dict__.get("lightComp")
        if value is not None and not isinstance(value, LightComponent):
            self.lightComp = componentFromData(LightComponent, value)

    def _normaliseAutoSoundParams(self) -> None:
        value = getattr(self, "autoSoundParams", AutoSoundParams())
        if isinstance(value, AutoSoundParams):
            self.autoSoundParams = AutoSoundParams(
                value.volume,
                value.minDistance,
                value.attenuation,
                value.loop,
                value.maxDistance,
            )
            return
        if isinstance(value, dict):
            self.autoSoundParams = AutoSoundParams(**Inner.filterDataClassParams(value, AutoSoundParams))
            return
        self.autoSoundParams = AutoSoundParams()

    def _getLightComp(self) -> Optional[LightComponent]:
        value = self.__dict__.get("lightComp")
        if value is None:
            return None
        if not isinstance(value, LightComponent):
            value = Cast(LightComponent, componentFromData(LightComponent, value))
            self.lightComp = value
        return value

    def _ensureLightComp(self) -> LightComponent:
        value = self._getLightComp()
        if value is None:
            value = LightComponent()
            self.lightComp = value
        return value

    def fixedUpdate(self, fixedDelta: float) -> None:
        r"""Fixed-timestep update callback.

        Processes route movement and per-step motion. Computes real speed
        for the frame.

        - \param fixedDelta  Fixed timestep duration in seconds
        """
        startPosition = self.getPosition()
        remaining = fixedDelta
        while remaining > 0.0:
            if not self._isMoving:
                self._tryStartNextRouteStep()
            if not self._isMoving:
                continueOffset = self._getContinueMoveOffset()
                if continueOffset is not None:
                    self.MapMove(continueOffset)
            if not self._isMoving:
                break
            remaining = self._processMoving(remaining)
        dist = (self.getPosition() - startPosition).length()
        if fixedDelta <= 0.0 or Math.IsNearZero(dist, 0.001):
            self._realSpeed = 0.0
        else:
            self._realSpeed = dist / fixedDelta

    def _getContinueMoveOffset(self) -> Optional[Union[Vector2i, Pair[int], List[int]]]:
        return None

    def _tryStartNextRouteStep(self) -> None:
        if not self._inRoute:
            return
        if not self._route:
            self._inRoute = False
            self._route = []
            return
        step = self._route.pop(0)
        if not self.MapMove(step):
            self._inRoute = False
            self._route = []

    def update(self, deltaTime: float) -> None:
        r"""\brief Update actor animation and automatic spatial sound.

        - \param deltaTime Time elapsed since the last frame in seconds.
        """
        super().update(deltaTime)
        self._updateAutoSound(deltaTime)

    @RegisterEvent
    def onCreate(self) -> None:
        r"""Blueprint event: called once when the actor is spawned into the scene."""
        pass

    @RegisterEvent
    def onTick(self, deltaTime: float) -> None:
        r"""Blueprint event: called every frame while `tickable` is `True`.

        - \param deltaTime  Time elapsed since the last frame in seconds
        """
        pass

    @RegisterEvent
    def onLateTick(self, deltaTime: float) -> None:
        r"""Blueprint event: called after all actors' onTick in the same frame.

        - \param deltaTime  Time elapsed since the last frame in seconds
        """
        pass

    @RegisterEvent
    def onFixedTick(self, fixedDelta: float) -> None:
        r"""Blueprint event: called at a fixed physics timestep.

        - \param fixedDelta  Fixed timestep duration in seconds
        """
        pass

    @RegisterEvent
    def onDestroy(self) -> None:
        r"""Blueprint event: called when the actor is removed from the scene."""
        pass

    @RegisterEvent
    def onCollision(self, other: List[Actor]) -> None:
        r"""Blueprint event: called when movement is blocked by another actor.

        - \param other  List of actors at the collision target cell
        """
        pass

    @RegisterEvent
    def onOverlap(self, other: List[Actor]) -> None:
        r"""Blueprint event: called when this actor enters a cell occupied by others.

        - \param other  List of actors sharing the same cell
        """
        pass

    @ExecSplit(default=(None,))
    def destroy(self) -> None:
        r"""Remove this actor from the current map and trigger `onDestroy`."""
        if self.isDestroyed():
            return
        self.destroyed = True
        self._stopAutoSound()
        for child in list(self.getChildren()):
            if isinstance(child, Actor):
                child.destroy()
        if self._map:
            self._map.destroyActor(self)

    @Meta(Vector2iVars=["offset"])
    @ExecSplit(success=(True,), fail=(False,))
    @TypeAdapter(offset=([tuple, list], Vector2i))
    def MapMove(self, offset: Union[Vector2i, Pair[int], List[int]]) -> bool:
        r"""Move the actor by one cell in the given direction.

        Validates boundaries and passability before initiating movement.
        Triggers `onCollision` on both parties if the target cell is blocked.

        - \param offset  Direction vector (clamped to unit: -1, 0, or 1 per axis)
        - \return `True` if movement was initiated, `False` otherwise
        """
        from ... import CellSize

        if not self._moveEnabled or not self._map:
            return False
        x = offset.x
        y = offset.y
        sx = 1 if x > 0 else (-1 if x < 0 else 0)
        sy = 1 if y > 0 else (-1 if y < 0 else 0)
        offset = Vector2i(sx, sy)
        if not self._moveEnabled:
            return False
        if offset == Vector2i(0, 0):
            return False
        if self._isMoving:
            return False
        target = self.getMapPosition() + offset
        if target.x < 0 or target.x >= self._map.getSize().x or target.y < 0 or target.y >= self._map.getSize().y:
            return False
        if not self._map.isPassable(self, target):
            collisions = self._map.getCollision(self, target)
            if collisions:
                Actor.BlueprintEvent(self, Actor, "onCollision", {"other": collisions})
                for collision in collisions:
                    Actor.BlueprintEvent(collision, Actor, "onCollision", {"other": [self]})
            return False
        self._isMoving = True
        self._moveOriginMapPos = Vector2i(self.getMapPosition().x, self.getMapPosition().y)
        self._departure = self.getPosition()
        self._destination = Vector2f(
            self.getPosition().x + offset.x * CellSize,
            self.getPosition().y + offset.y * CellSize,
        )
        return True

    @ReturnType(tickable=bool)
    def getTickable(self) -> bool:
        r"""Check whether tick events are enabled.

        - \return  `True` if tickable, `False` otherwise
        """
        return self.tickable

    @ExecSplit(default=(None,))
    def setTickable(self, tickable: bool, applyToChildren: bool = True) -> None:
        r"""Enable or disable tick event dispatch.

        Optionally propagates the setting to all child actors.

        - \param tickable         Whether to enable tick events
        - \param applyToChildren  If `True`, propagate to all child actors
        """
        self.tickable = tickable
        if applyToChildren:
            if self.getChildren():
                for child in self.getChildren():
                    if isinstance(child, Actor):
                        child.setTickable(tickable, applyToChildren)

    @ReturnType(intersects=bool)
    def intersects(self, other: Actor) -> bool:
        r"""Test whether this actor's bounding box overlaps another's.

        - \param other   The other actor to test against
        - \return        `True` if bounding boxes intersect, `False` otherwise
        """
        return not self.getGlobalBounds().findIntersection(other.getGlobalBounds()) is None

    @ReturnType(isMoving=bool)
    def isMoving(self) -> bool:
        r"""Check whether this actor is currently in motion.

        - \return  `True` if moving or has non-zero real speed
        """
        return self._isMoving or self._realSpeed > 0.0 or self._inRoute

    @ReturnType(pos=Vector2i)
    def getMapPosition(self) -> Vector2i:
        r"""\brief Get the grid cell position.

        While a one-cell move is in progress, keep reporting the departure
        cell so cover/path preview do not jump at the halfway point.

        - \return Grid cell position as Vector2i
        """
        if self._isMoving and self._moveOriginMapPos is not None:
            return Vector2i(self._moveOriginMapPos.x, self._moveOriginMapPos.y)
        return super().getMapPosition()

    @ReturnType(isInRoute=bool)
    def isInRoute(self) -> bool:
        r"""Check whether this actor is executing a movement route.

        - \return  `True` if a route is active, `False` otherwise
        """
        return self._inRoute

    @ExecSplit(default=(None,))
    @Meta(MoveRouteVars=["route"])
    def setRoute(self, route: List[Vector2i] = []) -> None:
        r"""Set a sequence of grid offsets to walk automatically.

        - \param route  List of grid offsets, or `None` to clear the route
        """
        self._inRoute = len(route) > 0
        self._route = route

    @ReturnType(route=List[Vector2i])
    def getRoute(self) -> List[Vector2i]:
        r"""Get the current movement route.

        - \return  List of grid offsets, or `None` if inactive
        """
        return self._route

    @ReturnType(moveEnabled=bool)
    def getMoveEnabled(self) -> bool:
        r"""Check whether movement is allowed.

        - \return  `True` if movement is enabled, `False` otherwise
        """
        return self._moveEnabled

    @ExecSplit(default=(None,))
    def setMoveEnabled(self, enabled: bool) -> None:
        r"""Enable or disable movement.

        Disabling also stops current motion.

        - \param enabled  Whether to enable movement
        """
        self._moveEnabled = enabled
        if not enabled:
            self.stop()

    @ExecSplit(default=(None,))
    def stop(self) -> None:
        r"""Immediately halt all movement and clear the current route.

        Resets motion state and snaps map position to the current location.
        """
        self._isMoving = False
        self._inRoute = False
        self._route = None
        self._departure = None
        self._destination = None
        self._moveOriginMapPos = None
        self._realSpeed = 0.0
        self._autoFixMapPosition()

    @ReturnType(velocity=Optional[Vector2f])
    def getVelocity(self) -> Optional[Vector2f]:
        r"""Compute the current velocity vector.

        Accounts for the tile material speed rate at the current map position.

        - \return  Velocity vector in pixels/second, or `None` if not moving
        """
        if not self._map or self._departure is None or self._destination is None:
            return None

        topMaterial = self._map.getTopMaterial(self.getMapPosition())
        speed = self.speed
        if not topMaterial is None:
            speed *= topMaterial.speedRate
        dist = self._destination - self._departure
        length = dist.length()
        time = length / speed
        velocity = dist / time
        return velocity

    @staticmethod
    def GenActor(
        ActorModel: type, texture: Texture, textureRect: Optional[Tuple[Pair[int], Pair[int]]], tag: str
    ) -> Actor:
        r"""Create an actor instance from a model class with material initialisation.

        - \param ActorModel     The actor subclass to instantiate
        - \param texture        The texture to assign to the new actor
        - \param textureRect    Optional texture rectangle for sprite slicing
        - \param tag            Optional tag string for the new actor
        - \return               The created `Actor` instance
        """
        actor: Actor = ActorModel(texture, textureRect, tag)
        if isinstance(actor.material, dict):
            actor.material = Material(**Inner.filterDataClassParams(actor.material, Material))
        return actor

    def _processMoving(self, fixedDelta: float) -> float:
        velocity = self.getVelocity()
        if velocity is None:
            return 0.0
        if self._destination is None or self._departure is None or self._map is None:
            return 0.0
        remainingDist = (self._destination - self.getPosition()).length()
        speed = velocity.length()
        if speed <= 0.0:
            return 0.0
        timeToDestination = remainingDist / speed
        if timeToDestination > fixedDelta:
            self.move(velocity * fixedDelta)
            return 0.0
        self.setPosition(Vector2f(self._destination.x, self._destination.y))
        self._isMoving = False
        self._departure = None
        self._destination = None
        self._moveOriginMapPos = None
        self._autoFixMapPosition()
        overlaps = self._map.getOverlaps(self)
        if overlaps:
            Actor.BlueprintEvent(self, Actor, "onOverlap", {"other": overlaps})
            for overlap in overlaps:
                Actor.BlueprintEvent(overlap, Actor, "onOverlap", {"other": [self]})
        self._onArrivedAtMapCell()
        return max(0.0, fixedDelta - timeToDestination)

    def _onArrivedAtMapCell(self) -> None:
        return

    def _autoFixMapPosition(self) -> None:
        from ... import CellSize

        pos = self.getPosition()
        x = int(pos.x * 1.0 / CellSize + 0.5)
        y = int(pos.y * 1.0 / CellSize + 0.5)
        self._moveOriginMapPos = None
        self.setMapPosition(Vector2u(x, y))
        if self._map:
            self._map.updateActorOccupancy(self)

    def _getAutoSoundListenerDistance(self) -> float:
        listenerPos = Listener.getPosition()
        actorPos = self.getPosition()
        dx = float(actorPos.x) - float(listenerPos.x)
        dy = float(actorPos.y) - float(listenerPos.y)
        return (dx * dx + dy * dy) ** 0.5

    def _updateAutoSound(self, deltaTime: float) -> None:
        self._normaliseAutoSoundParams()
        if not self.autoSound:
            self._stopAutoSound()
            self._autoSoundCooldown = 0.0
            return
        stopDistance = float(self.autoSoundParams.maxDistance)
        if stopDistance > 0.0:
            distance = self._getAutoSoundListenerDistance()
            startDistance = stopDistance * 0.85
            if distance > stopDistance:
                self._stopAutoSound()
                self._autoSoundCooldown = 0.0
                return
            if self._autoSoundObject is None and distance > startDistance:
                return
        if self._autoSoundObject is not None:
            if self._autoSoundObject.getStatus() == Sound.Status.Stopped:
                self._autoSoundObject = None
                self._autoSoundCooldown = max(0.0, float(self.autoSoundInterval))
            else:
                self._applyAutoSoundParams()
                return
        if self._autoSoundCooldown > 0.0:
            self._autoSoundCooldown = max(0.0, self._autoSoundCooldown - deltaTime)
            return
        self._playAutoSound()

    def _playAutoSound(self) -> None:
        from Global import Manager

        sound = Manager.playSE(self.autoSound, self._buildAutoSoundFilter())
        self._autoSoundObject = sound
        pos = self.getPosition()
        self._autoSoundLastPosition = Vector3f(pos.x, pos.y, 0.0)

    def _stopAutoSound(self) -> None:
        if self._autoSoundObject is None:
            return
        if self._autoSoundObject.getStatus() != Sound.Status.Stopped:
            self._autoSoundObject.stop()
        self._autoSoundObject = None
        self._autoSoundLastPosition = None

    def _applyAutoSoundParams(self) -> None:
        if self._autoSoundObject is None:
            return
        from Global.Manager.Mgr_Audio import AudioManager

        pos = self.getPosition()
        newPos = Vector3f(pos.x, pos.y, 0.0)
        if self._autoSoundLastPosition is not None and self._autoSoundLastPosition == newPos:
            return
        self._autoSoundLastPosition = newPos
        AudioManager.setSoundFilter(self._autoSoundObject, self._buildAutoSoundFilter())

    def _buildAutoSoundFilter(self) -> SoundFilter:
        from ... import Filters

        params = self.autoSoundParams
        position = self.getPosition()
        maxDistance = float(params.maxDistance)
        return Filters.SoundFilter(
            volume=float(params.volume),
            spatial=True,
            position=Vector3f(position.x, position.y, 0.0),
            relativeToListener=False,
            minDistance=float(params.minDistance),
            attenuation=float(params.attenuation),
            loop=True if params.loop else None,
            maxDistance=maxDistance if maxDistance > 0.0 else None,
        )
