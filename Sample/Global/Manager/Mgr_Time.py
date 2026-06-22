# -*- encoding: utf-8 -*-

from typing import Any, Callable, List, Optional
from Engine import Time, Clock


class TimerTaskEntry:
    r"""\brief Represents a scheduled timer task."""

    def __init__(
        self,
        time: float,
        task: Optional[Callable],
        params: List[Any],
        blocking: bool = False,
    ) -> None:
        self.time = time
        self.task = task
        self.params = params
        self.blocking = blocking


class TimeManager:
    r"""\brief Manages game time and delta time."""

    _clock = Clock()
    _lastElapsedTime = Time.Zero
    _deltaTime = Time.Zero
    _speed = 1.0

    @classmethod
    def init(cls) -> None:
        r"""\brief Initialise the time manager and start the clock."""
        cls._clock.start()
        cls.update()

    @classmethod
    def getCurrentTime(cls) -> Time:
        r"""\brief Get the current game time.
        - \return: Current elapsed time.
        """
        return cls._lastElapsedTime

    @classmethod
    def v_getDeltaTime(cls) -> float:
        r"""\brief Get delta time in seconds.
        - \return: Delta time in seconds.
        """
        return cls._deltaTime.asSeconds() * cls._speed

    @classmethod
    def getDeltaTime(cls) -> Time:
        r"""\brief Get delta time as Time object.
        - \return: Delta time.
        """
        return cls._deltaTime * cls._speed

    @classmethod
    def update(cls) -> None:
        r"""\brief Update the time manager.

        Call this once per frame.
        """
        lastTime = cls._lastElapsedTime
        currentTime = cls._clock.getElapsedTime()
        cls._lastElapsedTime = currentTime
        cls._deltaTime = currentTime - lastTime

    @classmethod
    def getSpeed(cls) -> float:
        r"""\brief Get the current time speed.
        - \return: Time speed factor.
        """
        return cls._speed

    @classmethod
    def setSpeed(cls, speed: float) -> None:
        r"""\brief Set the time speed.
        - \param speed: Time speed factor (must be >= 0.0).
        """
        assert speed >= 0.0
        cls._speed = speed


TimeManager.init()
