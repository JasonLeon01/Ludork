# -*- encoding: utf-8 -*-
r"""\brief Door actor with a one-shot open animation that self-destructs."""

from __future__ import annotations
from typing import List, Optional, Tuple, Union
from Engine import Filters, IntRect, Pair, RegisterEvent, Texture, Vector2i, Vector3f
from Engine.Gameplay.Actors import Actor
from Global import GameMap, Manager
from Source import System
from Source.Scenes import Map

_LATENT_STARTED = 0
_LATENT_FINISHED = 1


class _DoorOpenCondition:
    """Condition callable polled by LatentManager for the openDoor latent."""

    def __init__(self, door: Door) -> None:
        self._door = door
        self._startedEmitted = False
        self._finished = False

    def __call__(self) -> List[int]:
        if self._finished:
            return [_LATENT_FINISHED]
        if not self._startedEmitted:
            self._startedEmitted = True
            return [_LATENT_STARTED]
        if self._door._openFinished:
            self._finished = True
            return [_LATENT_FINISHED]
        return []

    def isFinished(self) -> bool:
        return self._finished


class Door(Actor):
    r"""A door actor that plays a one-shot open animation then destroys itself.

    The texture should contain frames arranged horizontally (left to right).
    When `openDoor()` is called, the door advances through each frame at
    `openInterval` seconds, then self-destructs. The door is NOT `animatable` — the open
    animation is driven manually in `onTick()`.

    `tickable` defaults to `True` so that `onTick` fires each frame.
    Calling `openDoor()` while already opening is a safe no-op.
    """

    collisionEnabled: bool = True  #: Whether the door blocks movement
    tickable: bool = True  #: Must be True for onTick to drive the animation
    openFrameCount: int = 4  #: Number of frames in the sprite sheet
    openInterval: float = 0.05  #: Seconds between frame advances
    opening: bool = False  #: Whether the door is currently opening

    def __init__(
        self,
        texture: Optional[Texture] = None,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        r"""Construct a Door actor.

        - \param texture  The sprite-sheet texture (frames arranged horizontally)
        - \param rect     Sub-rectangle for the initial frame
        - \param tag      Optional identifier tag
        """
        super().__init__(texture, rect, tag)
        self._openFrameIndex: int = 0
        self._openTimer: float = 0.0
        self._openFinished: bool = False
        self._frameWidth: int = 0
        if rect is not None:
            if isinstance(rect, IntRect):
                self._frameWidth = rect.size.x
            elif isinstance(rect, (tuple, list)):
                self._frameWidth = rect[1][0]

    @Latent(Started=(_LATENT_STARTED,), Finished=(_LATENT_FINISHED,))
    def openDoor(self) -> _DoorOpenCondition:
        r"""Start the door-open animation (latent).

        Advances through each frame every `openInterval` seconds. After the
        Calling while already opening or destroyed is a safe no-op.

        - \return A condition callable for the LatentManager to poll
        """
        if self.opening or self._openFinished or self.isDestroyed():
            cond = _DoorOpenCondition(self)
            cond._finished = True
            return cond
        position = self.getPosition()
        Manager.playSE(
            System.getGateSE(),
            Filters.SoundFilter(
                spatial=True,
                position=Vector3f(position.x, position.y, 0.0),
                relativeToListener=False,
            ),
        )
        self.opening = True
        self._openFrameIndex = 0
        self._openTimer = 0.0
        self._openFinished = False
        if self._frameWidth <= 0:
            rect = self.getTextureRect()
            if rect is not None:
                self._frameWidth = rect.size.x
        return _DoorOpenCondition(self)

    @RegisterEvent
    def onTick(self, deltaTime: float) -> None:
        r"""Blueprint event: drive the open animation when active.

        Called every frame while `tickable` is `True`.

        - \param deltaTime  Seconds since the last frame
        """
        if not self.opening:
            return
        self._openTimer += deltaTime
        frameCount = max(1, self.openFrameCount)
        if self._openFrameIndex < frameCount:
            if self._openTimer >= self.openInterval:
                self._openTimer -= self.openInterval
                self._openFrameIndex += 1
                if self._openFrameIndex < frameCount:
                    self._advanceToFrame(self._openFrameIndex)
                    if self._openFrameIndex == frameCount - 1:
                        self._openTimer = 0.0
                        self._openFrameIndex = frameCount
                else:
                    self._openTimer = 0.0
        else:
            self._openFinished = True
            self.opening = False
            self.destroy()
            Cast(Map, Cast(GameMap, self.getMap()).getScene()).recordDestroyedActor(self)

    def _advanceToFrame(self, index: int) -> None:
        r"""Set the texture rect to the given frame index (0-based).

        - \param index  Frame index (column in the sprite sheet)
        """
        if self._frameWidth <= 0:
            return
        rect = self.getTextureRect()
        if rect is None:
            return
        newRect = IntRect(Vector2i(index * self._frameWidth, rect.position.y), rect.size)
        self.setTextureRect(newRect)
