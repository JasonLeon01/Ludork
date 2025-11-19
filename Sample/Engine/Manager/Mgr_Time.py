# -*- encoding: utf-8 -*-

from . import Time, Clock


class TimeManager:
    _clock = Clock()
    _lastElapsedTime = Time.Zero
    _deltaTime = Time.Zero
    _speed = 1.0

    @classmethod
    def init(cls) -> None:
        cls._clock.start()
        cls.update()

    @classmethod
    def getCurrentTime(cls) -> Time:
        return cls._lastElapsedTime

    @classmethod
    def v_getDeltaTime(cls) -> float:
        return cls._deltaTime.asSeconds() * cls._speed

    @classmethod
    def getDeltaTime(cls) -> Time:
        return cls._deltaTime * cls._speed

    @classmethod
    def update(cls) -> None:
        lastTime = cls._lastElapsedTime
        currentTime = cls._clock.getElapsedTime()
        cls._lastElapsedTime = currentTime
        cls._deltaTime = currentTime - lastTime

    @classmethod
    def getSpeed(cls) -> float:
        return cls._speed

    @classmethod
    def setSpeed(cls, speed: float) -> None:
        assert speed >= 0.0
        cls._speed = speed


TimeManager.init()
