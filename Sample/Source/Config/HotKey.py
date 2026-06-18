# -*- encoding: utf-8 -*-

from Engine import Keyboard
from Source.Scenes import Map


HotKey = {
    Keyboard.Key.D: {
        "Scene": Map,
        "FunctionWhenPressed": None,
        "FunctionWhenReleased": Map.showEnemyBook,
    },
    Keyboard.Key.G: {
        "Scene": Map,
        "FunctionWhenPressed": None,
        "FunctionWhenReleased": Map.showFloorTeleporter,
    },
}
