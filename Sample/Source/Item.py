# -*- encoding: utf-8 -*-

from __future__ import annotations
from Engine import RegisterEvent
from Engine.Gameplay.Actors import Actor


class Item(Actor):
    name: str = ""
    desc: str = ""

    @RegisterEvent
    def onUse(self):
        pass
