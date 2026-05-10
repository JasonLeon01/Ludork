# -*- encoding: utf-8 -*-

from typing import Any, Dict
from Engine.UI import Image
from Engine.UI.Base import FunctionalBase
from Global import Manager, System, SceneBase
from Source import System as SourceSystem, System as GameSystem
from Source.Windows import WindowCommand
from Source.GameInstance import GameInstance


class Scene(SceneBase):
    r"""\brief Title screen scene with a command window for menu options."""

    def onEnter(self) -> None:
        r"""\brief Start with a blind transition effect."""
        System.setTransition(Manager.loadTransition("001-Blind01.png"), 3)

    def onCreate(self) -> None:
        r"""\brief Create background, command window, and load UI elements."""
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
        self._uiManager.loadUI(self._bg)
        self._uiManager.loadUI(self._windowCommand)
        self._titleBGM = None
        titleBGMFile = SourceSystem.getTitleBGM()
        if titleBGMFile:
            self._titleBGM = Manager.playMusic("BGM", titleBGMFile)
            if self._titleBGM is not None:
                self._titleBGM.setLooping(True)

    def onQuit(self) -> None:
        r"""\brief Stop title BGM when leaving this scene."""
        Manager.stopMusic("BGM")
        self._titleBGM = None

    def onDestroy(self) -> None:
        r"""\brief Ensure title BGM is stopped when scene is destroyed."""
        Manager.stopMusic("BGM")
        self._titleBGM = None

    @staticmethod
    def _startGame(obj: FunctionalBase, kwargs: Dict[str, Any]) -> None:
        from .SceneMap import Scene as SceneMap

        Manager.playSE(GameSystem.getDecisionSE())
        Manager.stopMusic("BGM")
        System.setScene(SceneMap())
        Cast(SceneMap, System.getScene()).setInst(GameInstance())

    @staticmethod
    def _loadGame(obj: FunctionalBase, kwargs: Dict[str, Any]) -> None:
        Manager.playSE(GameSystem.getDecisionSE())
        print("Load Game")

    @staticmethod
    def _configGame(obj: FunctionalBase, kwargs: Dict[str, Any]) -> None:
        Manager.playSE(GameSystem.getDecisionSE())
        print("Config Game")

    @staticmethod
    def _exitGame(obj: FunctionalBase, kwargs: Dict[str, Any]) -> None:
        Manager.playSE(GameSystem.getDecisionSE())
        System.exit()
