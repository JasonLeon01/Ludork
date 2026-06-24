# -*- encoding: utf-8 -*-
r"""\brief Enemy child actor that displays handbook battle damage."""

from __future__ import annotations
from typing import TYPE_CHECKING, List, Optional, Tuple, Union
from Engine import (
    CellSize,
    Color,
    Image,
    IntRect,
    Pair,
    RenderTexture,
    Text,
    Texture,
    UI,
    Vector2f,
    Vector2u,
)
from Engine.Gameplay.Actors import Actor
from .Battler import DamageType
from .NodeFunctions.Utils import ToShortNumber

if TYPE_CHECKING:
    from .Player import Player


@Meta(
    GeneralDataVars=[("requiredItemID", "Item")],
    ColourVars=["fillColor", "shadowColor"],
    Vector2fVars=["damageTextOffset"],
)
class EnemyDamageText(Actor):
    r"""\brief Text-only child actor showing enemy damage against the player."""

    tickable: bool = True  #: Update visibility and text every frame
    collisionEnabled: bool = False  #: Text overlay should not block movement
    requiredItemID: str = "EnemyBook"  #: Item required to reveal damage text
    fontSize: int = 8  #: Damage text font size
    damageTextOffset: Pair[float] = (0.0, 0.0)  #: Offset from the parent top-left corner
    fillColor: Tuple[int, int, int, int] = (255, 255, 255, 255)  #: Text colour
    shadowColor: Tuple[int, int, int, int] = (0, 0, 0, 255)  #: Shadow text colour
    _blankTexture: Optional[Texture] = None

    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        r"""\brief Construct an enemy damage text actor."""
        super().__init__(self._getBlankTexture(), None, tag)
        self._text: Optional[Text] = None
        self._renderTexture: Optional[RenderTexture] = None
        self._currentDamageText: str = ""
        self._currentCriticalText: str = ""
        self._currentDamageColor: Optional[Tuple[int, int, int, int]] = None
        self._currentOverlaySize: Optional[Tuple[int, int]] = None
        self._fillColor = self._makeColor(self.fillColor)
        self._shadowColor = self._makeColor(self.shadowColor)
        self.setVisible(False, False)

    @RegisterEvent
    def onTick(self, deltaTime: float) -> None:
        r"""\brief Refresh damage text and visibility.

        - \param deltaTime Time elapsed since the last frame.
        """
        from .Enemy import Enemy

        player = self._getPlayer()
        if player:
            parent: Enemy = Cast(Enemy, self.getParent())
            visible = bool(
                player
                and parent
                and parent.getVisible()
                and player.hasItem(self.requiredItemID)
                and hasattr(parent, "getDamage")
            )
            self.setVisible(visible, False)
            if not visible:
                self._clearRenderedTexture()
                return
            self._updateOverlayPosition()
            if not self._ensureText():
                self.setVisible(False, False)
                self._clearRenderedTexture()
                return
            damageType, damage = parent.getDamage(player)
            damageText = "???" if damageType == DamageType.UNDEFEATABLE else str(ToShortNumber(int(damage)))
            criticalText = self._formatCriticalText(parent.getCriticalValue(player))
            self._setOverlayText(
                damageText,
                criticalText,
                self._getDamageColor(damageType, int(damage), int(player.infoComp.HP)),
                self._getParentSize(),
            )

    def _ensureText(self) -> bool:
        if self._text is not None:
            return True
        font = getattr(UI, "DefaultFont", None)
        if font is None:
            return False
        self._text = Text(font, "", max(1, int(self.fontSize)))
        self._text.setStyle(Text.Style.Bold)
        self._text.setFillColor(self._fillColor)
        return True

    def _setOverlayText(self, damageText: str, criticalText: str, damageColor: Color, overlaySize: Vector2u) -> None:
        colorKey = (damageColor.r, damageColor.g, damageColor.b, damageColor.a)
        sizeKey = (int(overlaySize.x), int(overlaySize.y))
        if self._text is None or (
            damageText == self._currentDamageText
            and criticalText == self._currentCriticalText
            and colorKey == self._currentDamageColor
            and sizeKey == self._currentOverlaySize
        ):
            return
        self._currentDamageText = damageText
        self._currentCriticalText = criticalText
        self._currentDamageColor = colorKey
        self._currentOverlaySize = sizeKey
        self._fillColor = damageColor
        self._renderTextTexture(overlaySize)

    def _renderTextTexture(self, overlaySize: Vector2u) -> None:
        if self._text is None:
            return
        width = max(1, int(overlaySize.x))
        height = max(1, int(overlaySize.y))
        padding = 2
        self._renderTexture = RenderTexture(Vector2u(width, height))
        self._renderTexture.clear(Color.Transparent)
        self._drawText(self._currentCriticalText, Color.White, width, height, padding, False)
        self._drawText(self._currentDamageText, self._fillColor, width, height, padding, True)
        self._renderTexture.display()
        self.setTexture(self._renderTexture.getTexture(), True)
        self.setOrigin(Vector2f(0.0, 0.0))

    def _drawText(
        self,
        text: str,
        fillColor: Color,
        width: int,
        height: int,
        padding: int,
        bottomAligned: bool,
    ) -> None:
        if not text or self._text is None or self._renderTexture is None:
            return
        self._text.setString(text)
        bounds = self._text.getLocalBounds()
        x = width - padding - bounds.size.x - bounds.position.x
        if bottomAligned:
            y = height - padding - bounds.size.y - bounds.position.y
        else:
            y = padding - bounds.position.y
        basePosition = Vector2f(x, y)
        self._text.setFillColor(self._shadowColor)
        for offsetX, offsetY in [(-1.0, -1.0), (-1.0, 1.0), (1.0, -1.0), (1.0, 1.0)]:
            self._text.setPosition(Vector2f(basePosition.x + offsetX, basePosition.y + offsetY))
            self._renderTexture.draw(self._text)
        self._text.setFillColor(fillColor)
        self._text.setPosition(basePosition)
        self._renderTexture.draw(self._text)

    def _clearRenderedTexture(self) -> None:
        if self.getTexture() is self._blankTexture:
            return
        self._renderTexture = None
        self._currentDamageText = ""
        self._currentCriticalText = ""
        self._currentDamageColor = None
        self._currentOverlaySize = None
        self.setTexture(self._getBlankTexture(), True)
        self.setOrigin(Vector2f(0.0, 0.0))

    def _updateOverlayPosition(self) -> None:
        offset = self.damageTextOffset
        if isinstance(offset, str):
            try:
                offset = Eval(offset)
            except Exception:
                offset = (0.0, 0.0)
        offsetX = float(offset[0]) if isinstance(offset, (tuple, list)) and len(offset) >= 1 else 0.0
        offsetY = float(offset[1]) if isinstance(offset, (tuple, list)) and len(offset) >= 2 else 0.0
        self.setRelativePosition(Vector2f(offsetX, offsetY))

    def _getParentSize(self) -> Vector2u:
        parent = self.getParent()
        if parent is None:
            return Vector2u(CellSize, CellSize)
        rect = parent.getTextureRect()
        if rect is None:
            return Vector2u(CellSize, CellSize)
        return Vector2u(max(1, int(rect.size.x)), max(1, int(rect.size.y)))

    @staticmethod
    def _getPlayer() -> Optional[Player]:
        from Global import System, SceneBase

        scene = Cast(SceneBase, System.getScene())
        player = getattr(scene, "player", None)
        if player is None and hasattr(scene, "inst"):
            player = scene.inst.getPlayer()
        return player

    @staticmethod
    def _getDamageColor(damageType: DamageType, damage: int, playerHP: int) -> Color:
        if damageType == DamageType.UNDEFEATABLE:
            return UI.GetDimGrey()
        damage = max(0, damage)
        playerHP = max(0, playerHP)
        if damage == 0:
            return Color.Green
        if playerHP <= 0:
            return UI.GetDimGrey()
        if damage < playerHP / 4:
            return Color.White
        if damage < playerHP / 2:
            return Color.Yellow
        if damage < playerHP * 3 / 4:
            return UI.GetCopper()
        if damage < playerHP:
            return Color.Red
        return UI.GetDimGrey()

    @staticmethod
    def _formatCriticalText(criticalValue: int) -> str:
        if criticalValue == -2:
            return ""
        if criticalValue == -1:
            return "???"
        return str(ToShortNumber(int(criticalValue)))

    @staticmethod
    def _makeColor(value: Union[Color, Tuple[int, int, int, int]]) -> Color:
        if isinstance(value, Color):
            return Color(value.r, value.g, value.b, value.a)
        values = list(value)
        if len(values) == 3:
            return Color(int(values[0]), int(values[1]), int(values[2]), 255)
        return Color(int(values[0]), int(values[1]), int(values[2]), int(values[3]))

    @classmethod
    def _getBlankTexture(cls) -> Texture:
        if cls._blankTexture is None:
            cls._blankTexture = Texture(Image(Vector2u(1, 1), Color.Transparent))
        return cls._blankTexture
