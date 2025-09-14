# -*- encoding: utf-8 -*-

from Engine import pysf

sfClock = pysf.Clock


class ModifiedClock(sfClock):
    def v_getElapsedTime(self) -> float:
        return super().getElapsedTime().asSeconds()

    def v_reset(self) -> float:
        return super().reset().asSeconds()

    def v_restart(self) -> float:
        return super().restart().asSeconds()
