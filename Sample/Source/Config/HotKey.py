# -*- encoding: utf-8 -*-

from Engine import Keyboard
from Source.Scenes import Map


HotKey = {
    Keyboard.Key.Escape: {
        "Scene": Map,
        "Filter": ["casual"],
        "FunctionWhenPressed": Map.openMenu,
        "FunctionWhenReleased": None,
    },
    Keyboard.Key.D: {
        "Scene": Map,
        "Filter": ["casual"],
        "FunctionWhenPressed": None,
        "FunctionWhenReleased": Map.showEnemyBook,
    },
    Keyboard.Key.G: {
        "Scene": Map,
        "Filter": ["casual"],
        "FunctionWhenPressed": None,
        "FunctionWhenReleased": Map.showFloorTeleporter,
    },
}
