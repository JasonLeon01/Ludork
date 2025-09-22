# -*- encoding: utf-8 -*-

from . import Time, Clock


class TimeManager:
    _clock = Clock()
    _lastElapsedTime = Time.Zero
    _deltaTime = Time.Zero

    @classmethod
    def init(cls) -> None:
        cls._clock.start()
        cls.update()

    @classmethod
    def getCurrentTime(cls) -> Time:
        return cls._lastElapsedTime

    @classmethod
    def v_getDeltaTime(cls) -> float:
        return cls._deltaTime.asSeconds()

    @classmethod
    def getDeltaTime(cls) -> Time:
        return cls._deltaTime

    @classmethod
    def update(cls) -> None:
        lastTime = cls._lastElapsedTime
        currentTime = cls._clock.getElapsedTime()
        cls._lastElapsedTime = currentTime
        cls._deltaTime = currentTime - lastTime


TimeManager.init()
