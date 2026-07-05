# -*- encoding: utf-8 -*-
r"""\brief Door actor with open/close sprite animations."""

from __future__ import annotations
from typing import List, Optional, Tuple, Union
from Engine import Filters, IntRect, Pair, RegisterEvent, Texture, Vector2i, Vector3f
from Engine.Gameplay.Actors import Actor
from Global import GameMap, Manager
from Source.Scenes import Map

_LATENT_STARTED = 0
_LATENT_FINISHED = 1


class _DoorAnimationCondition:
    """Condition callable polled by LatentManager for door animation latents."""

    def __init__(self, door: Door, finishedAttr: str) -> None:
        self._door = door
        self._finishedAttr = finishedAttr
        self._startedEmitted = False
        self._finished = False

    def __call__(self) -> List[int]:
        if self._finished:
            return [_LATENT_FINISHED]
        if not self._startedEmitted:
            self._startedEmitted = True
            return [_LATENT_STARTED]
        if getattr(self._door, self._finishedAttr):
            self._finished = True
            return [_LATENT_FINISHED]
        return []

    def isFinished(self) -> bool:
        return self._finished


@Meta(PathVars=[("gateSE", "Sounds")], ConfigVars=[("gateSE", "Audio", "gateSE")])
class Door(Actor):
    r"""A door actor that plays sprite-sheet open and close animations.

    The texture should contain frames arranged horizontally (left to right).
    When `openDoor()` is called, the door advances through each frame at
    `openInterval` seconds, then self-destructs. When `closeDoor()` is called,
    the door animates from the current frame back to the first frame. The frame
    count is derived from the texture width, the initial rect origin, and the
    frame width. The door is NOT `animatable` — animations are driven manually
    in `onTick()`.

    `tickable` defaults to `True` so that `onTick` fires each frame.
    Calling `openDoor()` or `closeDoor()` while the same animation is already
    running is a safe no-op.
    """

    collisionEnabled: bool = True  #: Whether the door blocks movement
    tickable: bool = True  #: Must be True for onTick to drive the animation
    openInterval: float = 0.05  #: Seconds between frame advances
    gateSE: str = ""  #: Door sound effect override; empty uses Audio.gateSE
    opening: bool = False  #: Whether the door is currently opening
    closing: bool = False  #: Whether the door is currently closing

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
        self._frameIndex: int = 0
        self._animTimer: float = 0.0
        self._openFinished: bool = False
        self._closeFinished: bool = False
        self._frameWidth: int = 0
        self._startX: int = 0
        self._startY: int = 0
        self._captureClosedFrameLayout()

    def setTextureRect(self, rect: IntRect) -> None:
        r"""Set the texture sub-rectangle and keep the closed-frame layout in sync.

        While the door is idle, the rect defines the animation strip origin.
        During open/close playback the layout is held fixed so frame advances
        do not overwrite it.
        """
        super().setTextureRect(rect)
        if not self.opening and not self.closing:
            self._captureClosedFrameLayout(rect)

    @Latent(Started=(_LATENT_STARTED,), Finished=(_LATENT_FINISHED,))
    def openDoor(self) -> _DoorAnimationCondition:
        r"""Start the door-open animation (latent).

        Advances through each frame every `openInterval` seconds, then
        self-destructs. Calling while already opening or destroyed is a safe
        no-op. Interrupts an in-progress close animation.

        - \return A condition callable for the LatentManager to poll
        """
        if self._openFinished or self.isDestroyed():
            condition = _DoorAnimationCondition(self, "_openFinished")
            condition._finished = True
            return condition
        if self.opening:
            condition = _DoorAnimationCondition(self, "_openFinished")
            condition._finished = True
            return condition
        if self.closing:
            self.closing = False
            self._closeFinished = False
        self._playGateSE()
        self.opening = True
        self.closing = False
        self._frameIndex = 0
        self._animTimer = 0.0
        self._openFinished = False
        self._closeFinished = False
        self._advanceToFrame(0)
        return _DoorAnimationCondition(self, "_openFinished")

    @Latent(Started=(_LATENT_STARTED,), Finished=(_LATENT_FINISHED,))
    def closeDoor(self) -> _DoorAnimationCondition:
        r"""Start the door-close animation (latent).

        Animates from the current frame back to the first frame every
        `openInterval` seconds. Calling while already closing, destroyed, or
        already on the first frame is a safe no-op. Interrupts an in-progress
        open animation.

        - \return A condition callable for the LatentManager to poll
        """
        if self.isDestroyed() or self._openFinished:
            condition = _DoorAnimationCondition(self, "_closeFinished")
            condition._finished = True
            return condition
        if self.closing:
            condition = _DoorAnimationCondition(self, "_closeFinished")
            condition._finished = True
            return condition
        if self.opening:
            self.opening = False
            self._openFinished = False
        self._resolveFrameLayout()
        currentIndex = self._getCurrentFrameIndex()
        if currentIndex <= 0:
            currentIndex = self._resolveClosingFrameIndex()
        if currentIndex <= 0:
            condition = _DoorAnimationCondition(self, "_closeFinished")
            condition._finished = True
            return condition
        self._playGateSE()
        self.closing = True
        self.opening = False
        self._frameIndex = currentIndex
        self._animTimer = 0.0
        self._closeFinished = False
        self._openFinished = False
        return _DoorAnimationCondition(self, "_closeFinished")

    @RegisterEvent
    def onTick(self, deltaTime: float) -> None:
        r"""Blueprint event: drive open/close animations when active.

        Called every frame while `tickable` is `True`.

        - \param deltaTime  Seconds since the last frame
        """
        if self.opening:
            self._tickOpen(deltaTime)
        elif self.closing:
            self._tickClose(deltaTime)

    def _tickOpen(self, deltaTime: float) -> None:
        frameCount = self._getFrameCount()
        self._animTimer += deltaTime
        while self._animTimer >= self.openInterval:
            self._animTimer -= self.openInterval
            self._frameIndex += 1
            if self._frameIndex >= frameCount:
                self._finishOpening()
                return
            self._advanceToFrame(self._frameIndex)

    def _tickClose(self, deltaTime: float) -> None:
        self._animTimer += deltaTime
        while self._animTimer >= self.openInterval:
            self._animTimer -= self.openInterval
            self._frameIndex -= 1
            if self._frameIndex <= 0:
                self._advanceToFrame(0)
                self._finishClosing()
                return
            self._advanceToFrame(self._frameIndex)

    def _playGateSE(self) -> None:
        position = self.getPosition()
        Manager.playSE(
            self.gateSE,
            Filters.SoundFilter(
                spatial=True,
                position=Vector3f(position.x, position.y, 0.0),
                relativeToListener=False,
            ),
        )

    def _captureClosedFrameLayout(self, rect: Optional[IntRect] = None) -> None:
        if rect is None:
            rect = self.getTextureRect()
        if rect is None:
            return
        self._frameWidth = rect.size.x
        self._startX = rect.position.x
        self._startY = rect.position.y

    def _resolveFrameLayout(self) -> None:
        if self._frameWidth > 0:
            return
        self._captureClosedFrameLayout()

    def _getFrameCount(self) -> int:
        self._resolveFrameLayout()
        if self._frameWidth <= 0:
            return 1
        texture = self.getTexture()
        if texture is None:
            return 1
        remainingWidth = texture.getSize().x - self._startX
        return max(1, remainingWidth // self._frameWidth)

    def _getCurrentFrameIndex(self) -> int:
        self._resolveFrameLayout()
        if self._frameWidth <= 0:
            return 0
        rect = self.getTextureRect()
        if rect is None:
            return 0
        return max(0, (rect.position.x - self._startX) // self._frameWidth)

    def _resolveClosingFrameIndex(self) -> int:
        if self._frameWidth <= 0:
            return 0
        rect = self.getTextureRect()
        if rect is None:
            return 0
        texture = self.getTexture()
        if texture is not None and rect.position.x + self._frameWidth < texture.getSize().x:
            return 0
        stripStartX = rect.position.x % self._frameWidth
        if stripStartX == self._startX:
            return 0
        self._startX = stripStartX
        self._startY = rect.position.y
        return self._getCurrentFrameIndex()

    def _finishOpening(self) -> None:
        self._openFinished = True
        self.opening = False
        self.destroy()
        Cast(Map, Cast(GameMap, self.getMap()).getScene()).recordDestroyedActor(self)

    def _finishClosing(self) -> None:
        self._closeFinished = True
        self.closing = False
        self._frameIndex = 0

    def _advanceToFrame(self, index: int) -> None:
        r"""Set the texture rect to the given frame index (0-based).

        - \param index  Frame index (column in the sprite sheet)
        """
        if self._frameWidth <= 0:
            return
        rect = self.getTextureRect()
        if rect is None:
            return
        newRect = IntRect(
            Vector2i(self._startX + index * self._frameWidth, self._startY),
            rect.size,
        )
        self.setTextureRect(newRect)
