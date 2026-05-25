# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Dict
from Engine import IntRect, Vector2f, Vector2i, Color, Text
from Engine.UI import Canvas, Image, PlainText, SolidRect, RichText, TextStyle
from ..Player import Player
from ..System import System
from .. import Data


class PlayerAttrHUD(Canvas):
    r"""\brief Player attribute HUD displaying level, states, HP bar, and stats.

    Shows the player's avatar, states, HP bar with value, and stat values.
    """

    _AVATAR_MIN_SIZE = 32
    _FONT_SIZE = 12
    _HUD_POS_X = 16
    _HUD_POS_Y = 16
    _STATE_ROW_Y = 16
    _HP_ROW_Y = 32
    _HP_BAR_HEIGHT = 16
    _HP_BAR_WIDTH = 128
    _STAT_ROW_1_Y = 48
    _STAT_ROW_2_Y = 64
    _STAT_ROW_3_Y = 80

    def __init__(self, player: Player) -> None:
        r"""Construct a player attribute HUD bound to the given player instance.

        - \param player  Target player whose attributes are displayed on this HUD
        """
        self._player = player
        self._avatar: Optional[Image] = None
        self._avatarSize = self._AVATAR_MIN_SIZE
        self._infoStartX = self._AVATAR_MIN_SIZE
        self._hpBarWidth = self._HP_BAR_WIDTH
        self._font = System.getFonts()[0]
        self._initAvatar()
        self._initTextStyles()
        self._initLayout()
        super().__init__(((self._HUD_POS_X, self._HUD_POS_Y), (self._hudWidth, self._hudHeight)))
        self._buildUI()
        self._refresh()

    def update(self, deltaTime: float) -> None:
        r"""Update HUD texts and bars every frame to reflect current player attributes.

        - \param deltaTime  Elapsed frame time in seconds
        """
        self._refresh()
        return super().update(deltaTime)

    def _initAvatar(self) -> None:
        texture = self._player.getTexture()
        if texture is None:
            return
        textureSize = texture.getSize()
        frameWidth = max(1, int(textureSize.x / 4))
        frameHeight = max(1, int(textureSize.y / 4))
        frameSize = min(frameWidth, frameHeight)
        self._avatarSize = max(self._AVATAR_MIN_SIZE, frameSize)
        self._infoStartX = max(self._AVATAR_MIN_SIZE, self._avatarSize)
        avatarRect = IntRect(Vector2i(0, 0), Vector2i(frameWidth, frameHeight))
        self._avatar = Image(texture, avatarRect)

    def _initTextStyles(self) -> None:
        r"""Initialize text styles for rich text rendering."""
        self._textStyles: Dict[str, TextStyle] = {}
        self._textStyles["default"] = TextStyle(self._FONT_SIZE, Text.Style.Regular, Color.White, None, 0.0)
        self._textStyles["Yellow"] = TextStyle(fillColor=Color.Yellow)
        self._textStyles["Blue"] = TextStyle(fillColor=Color.Blue)
        self._textStyles["Red"] = TextStyle(fillColor=Color.Red)

    def _initLayout(self) -> None:
        self._hudWidth = max(self._infoStartX + self._hpBarWidth, self._hpBarWidth + self._AVATAR_MIN_SIZE)
        self._hpBarWidth = self._hudWidth
        self._hudHeight = self._STAT_ROW_3_Y + self._FONT_SIZE + 4

    def _buildUI(self) -> None:
        if self._avatar is not None:
            self._avatar.setPosition((0, 0))
            self.addChild(self._avatar)

        self._levelText = PlainText(self._font, "", self._FONT_SIZE)
        self._levelText.setPosition((self._infoStartX, 0))
        self.addChild(self._levelText)

        self._stateText = PlainText(self._font, "", self._FONT_SIZE)
        self._stateText.setPosition((self._infoStartX, self._STATE_ROW_Y))
        self.addChild(self._stateText)

        self._hpBack = SolidRect((self._hpBarWidth, self._HP_BAR_HEIGHT), Color(24, 24, 24, 220))
        self._hpBack.setPosition((0, self._HP_ROW_Y))
        self.addChild(self._hpBack)

        self._hpFill = SolidRect((self._hpBarWidth, self._HP_BAR_HEIGHT), Color(0, 192, 0, 255))
        self._hpFill.setPosition((0, self._HP_ROW_Y))
        self.addChild(self._hpFill)

        self._hpText = PlainText(self._font, "", self._FONT_SIZE)
        self.addChild(self._hpText)

        self._statLine1 = PlainText(self._font, "", self._FONT_SIZE)
        self._statLine1.setPosition((0, self._STAT_ROW_1_Y))
        self.addChild(self._statLine1)

        self._statLine2 = PlainText(self._font, "", self._FONT_SIZE)
        self._statLine2.setPosition((0, self._STAT_ROW_2_Y))
        self.addChild(self._statLine2)

        self._itemText = RichText(self._font, "", self._textStyles)
        self._itemText.setPosition((0, self._STAT_ROW_3_Y))
        self.addChild(self._itemText)

    def _refresh(self) -> None:
        hp = self._player.infoComp.HP
        maxhp = self._player.infoComp.MAXHP
        level = self._player.infoComp.LEVEL
        atk = self._player.infoComp.ATK
        defense = self._player.infoComp.DEF
        exp = self._player.infoComp.EXP
        gold = self._player.infoComp.GOLD
        states = " ".join(self._player.getStateNames())
        if not states:
            states = LOC("NORMAL")

        self._levelText.setString(f"Lv. {level}")
        self._stateText.setString(f"[{states}]")
        self._hpText.setString(f"{hp}/{maxhp}")
        self._statLine1.setString(f"ATK: {atk}\tDEF: {defense}")
        self._statLine2.setString(f"EXP: {exp}\tGOLD: {gold}")

        hpRate = hp / maxhp
        self._hpFill.setSize(Vector2f(self._hpBarWidth * hpRate, self._HP_BAR_HEIGHT))

        hpBounds = self._hpText.getLocalBounds()
        textX = (self._hpBarWidth - hpBounds.size.x) / 2.0 - hpBounds.position.x
        textY = self._HP_ROW_Y + (self._HP_BAR_HEIGHT - hpBounds.size.y) / 2.0 - hpBounds.position.y
        self._hpText.setPosition((textX, textY))

        keyY_count = self._player.getItemCount("KEY_Y")
        keyB_count = self._player.getItemCount("KEY_B")
        keyR_count = self._player.getItemCount("KEY_R")
        displayText = (
            f"#default#KEYS: #Yellow#{keyY_count}#default#\t#Blue#{keyB_count}#default#\t#Red#{keyR_count}#default#"
        )
        self._itemText.setString(displayText)
