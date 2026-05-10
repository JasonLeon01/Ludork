# -*- encoding: utf-8 -*-

from pysf import Clock


class ModifiedClock(Clock):
    r"""
    \brief Modified Clock.
    """

    def v_getElapsedTime(self) -> float:
        r"""
        \brief Virtual get for elapsed time.
        """

        return super().getElapsedTime().asSeconds()

    def v_reset(self) -> float:
        r"""
        \brief v_reset.
        """

        return super().reset().asSeconds()

    def v_restart(self) -> float:
        r"""
        \brief v_restart.
        """

        return super().restart().asSeconds()
