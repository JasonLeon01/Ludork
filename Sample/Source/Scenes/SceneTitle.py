# -*- encoding: utf-8 -*-

from typing import Any, Dict
from Engine.UI import Image
from Engine.UI.Base import FunctionalBase
from Global import Manager, System, SceneBase
from Source import System as SourceSystem, System as GameSystem
from Source.Windows import WindowCommand, WindowSaveLoad
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
            ((0, 0), (320, 96)),
            {
                "Start": {"text": LOC("TITLE_START"), "callback": Scene._startGame},
                "Load": {"text": LOC("TITLE_CONTINUE"), "callback": lambda obj, kwargs: self._onLoadCommand()},
                "Config": {"text": LOC("TITLE_CONFIG"), "callback": Scene._configGame},
                "Exit": {"text": LOC("TITLE_EXIT"), "callback": Scene._exitGame},
            },
            columns=2,
        )
        self._windowCommand.setOrigin((160, 64))
        self._windowCommand.setPosition((320, 240))
        self._windowSaveLoad = WindowSaveLoad(
            slotRect=((112, 112), (160, 256)),
            detailRect=((272, 112), (256, 256)),
            loadOnly=True,
            onClose=self._onSaveLoadClose,
            onLoaded=self._onSaveLoadLoaded,
        )
        self._uiManager.loadUI(self._bg)
        self._uiManager.loadUI(self._windowCommand)
        self._uiManager.loadUI(self._windowSaveLoad.getSlotWindow())
        self._uiManager.loadUI(self._windowSaveLoad.getDetailWindow())
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

    def _onLoadCommand(self) -> None:
        Manager.playSE(GameSystem.getDecisionSE())
        self._windowCommand.setActive(False)
        self._windowSaveLoad.open()

    def _onSaveLoadClose(self, reason: str) -> None:
        if reason == "loaded":
            return
        self._windowCommand.setActive(True)

    def _onSaveLoadLoaded(self, inst: GameInstance) -> None:
        from .SceneMap import Scene as SceneMap

        Manager.stopMusic("BGM")
        nextScene = SceneMap()
        nextScene.setInst(inst)
        System.setScene(nextScene)

    @staticmethod
    def _configGame(obj: FunctionalBase, kwargs: Dict[str, Any]) -> None:
        Manager.playSE(GameSystem.getDecisionSE())
        print("Config Game")

    @staticmethod
    def _exitGame(obj: FunctionalBase, kwargs: Dict[str, Any]) -> None:
        Manager.playSE(GameSystem.getDecisionSE())
        System.exit()
