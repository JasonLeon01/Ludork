# -*- encoding: utf-8 -*-

from Engine import Color, Input, RectangleShape, Text, Vector2f
from Engine.UI import PlainText
from Global import Manager, SceneBase
from Global import System as GlobalSystem
from Source import System as GameSystem


class Scene(SceneBase):
    r"""\brief Game over scene that waits for confirmation before returning to title."""

    def onEnter(self) -> None:
        r"""\brief Fade in the game over screen."""
        GlobalSystem.setTransition(transitionTime=3.0)

    def onCreate(self) -> None:
        r"""\brief Create the black background and centred game over text."""
        gameSize = GlobalSystem.getGameSize()
        self._background = RectangleShape(Vector2f(float(gameSize.x), float(gameSize.y)))
        self._background.setFillColor(Color.Black)

        self._text = PlainText(GameSystem.getFonts()[0], "GAME OVER", 72, Text.Style.Bold, Color.Red)
        textSize = self._text.getSize()
        self._text.setOrigin((textSize.x / 2.0, textSize.y / 2.0))
        self._text.setPosition((gameSize.x / 2.0, gameSize.y / 2.0))
        self._uiManager.loadUI(self._text)

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Return to the title scene after confirm key or left click."""
        if Input.isActionTriggered(Input.getConfirmKeys(), handled=True) or Input.isMouseButtonTriggered(
            Input.Mouse.Button.Left,
            handled=True,
        ):
            self._backToTitle()

    def _renderHandle(self, deltaTime: float) -> None:
        GlobalSystem.draw(self._background)
        super()._renderHandle(deltaTime)

    def _backToTitle(self) -> None:
        from .SceneTitle import Scene as SceneTitle

        Manager.playSE(GameSystem.getDecisionSE())
        GlobalSystem.setScene(SceneTitle())
