# -*- encoding: utf-8 -*-

from typing import Any, Dict
from Engine import SceneBase, System, Manager
from Engine.UI import Image
from Engine.UI.Base import FunctionalBase
from Source.Windows import WindowCommand


class Scene(SceneBase):
    def onEnter(self) -> None:
        System.setTransition(Manager.loadTransition("001-Blind01.png"), 3)

    def onCreate(self):
        self._bg = Image(Manager.loadSystem("GrassBackground.png"))
        self._windowCommand = WindowCommand(
            ((0, 0), (256, 128)),
            {
                "Start": {"text": "Start", "callback": Scene._startGame},
                "Load": {"text": "Load", "callback": Scene._loadGame},
                "Config": {"text": "Config", "callback": Scene._configGame},
                "Exit": {"text": "Exit", "callback": Scene._exitGame},
            },
        )
        self._windowCommand.setOrigin((128, 64))
        self._windowCommand.setPosition((320, 240))
        self.addUI(self._bg)
        self.addUI(self._windowCommand)

    @staticmethod
    def _startGame(obj: FunctionalBase, kwargs: Dict[str, Any]):
        from .Map import Scene as MapScene

        System.setScene(MapScene())

    @staticmethod
    def _loadGame(obj: FunctionalBase, kwargs: Dict[str, Any]):
        print("Load Game")

    @staticmethod
    def _configGame(obj: FunctionalBase, kwargs: Dict[str, Any]):
        print("Config Game")

    @staticmethod
    def _exitGame(obj: FunctionalBase, kwargs: Dict[str, Any]):
        System.exit()
