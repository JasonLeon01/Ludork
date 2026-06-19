# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Dict, List, Optional, Tuple
from Engine import IntRect, Texture, Vector2f, Vector2i, Color, Text
from Engine.UI import Canvas, Image, PlainText, SolidRect, RichText, TextStyle
from Engine.UI.Base import ControlBase
from Global import Manager
from .. import Data
from ..NodeFunctions.Utils import ToShortNumber
from ..Player import Player
from ..System import System


class PlayerAttrHUD(Canvas):
    r"""\brief Player attribute HUD displaying level, states, HP bar, and stats.

    Shows the player's avatar, states, HP bar with value, and stat values.
    """

    _AVATAR_MIN_SIZE = 32
    _FONT_SIZE = 18
    _HP_MAX_FONT_SIZE = 12
    _HEADER_FONT_SIZE = 16
    _HUD_POS_X = 16
    _HUD_POS_Y = 16
    _STATE_ICON_SIZE = 16
    _STATE_GAP = 4
    _HEADER_ROW_Y = 0
    _HP_ROW_Y = 68
    _HP_BAR_OFFSET_Y = 8
    _HP_BAR_HEIGHT = 8
    _HP_TEXT_LAYOUT_HEIGHT = 12
    _HP_BAR_WIDTH = 96
    _STAT_VALUE_X = 128
    _DEBUFF_TEXT_OFFSET_X = 2
    _ATK_ROW_Y = 96
    _DEF_ROW_Y = 128
    _EXP_ROW_Y = 192
    _GOLD_ROW_Y = 224
    _KEY_ROW_Y = 288
    _KEY_ICON_HEIGHT = 32

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
        self._stateWidgets: List[ControlBase] = []
        self._stateSignature: Optional[Tuple[str, ...]] = None
        self._stateIconCache: Dict[str, Optional[Texture]] = {}
        self._initAvatar()
        self._initTextStyles()
        self._initLayout()
        super().__init__(((self._HUD_POS_X, self._HUD_POS_Y), (self._hudWidth, self._hudHeight)))
        self._buildUI()
        self._refresh()

    def onTick(self, deltaTime: float) -> None:
        r"""Update HUD texts and bars every frame to reflect current player attributes.

        - \param deltaTime  Elapsed frame time in seconds
        """
        self._refresh()
        return super().onTick(deltaTime)

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
        self._hpTextStyles: Dict[str, TextStyle] = {}
        self._hpTextStyles["default"] = TextStyle(self._FONT_SIZE, Text.Style.Regular, Color.White, None, 0.0)
        self._hpTextStyles["max"] = TextStyle(self._HP_MAX_FONT_SIZE, Text.Style.Regular, Color.White, None, 0.0)

    def _initLayout(self) -> None:
        self._hudWidth = max(
            self._infoStartX + self._hpBarWidth,
            self._hpBarWidth + self._AVATAR_MIN_SIZE,
            self._STAT_VALUE_X + 16,
        )
        self._hpBarWidth = self._hudWidth
        keyRowHeight = max(self._FONT_SIZE, self._KEY_ICON_HEIGHT)
        self._hudHeight = self._KEY_ROW_Y + keyRowHeight + 4

    def _buildUI(self) -> None:
        if self._avatar is not None:
            self._avatar.setPosition((0, 0))
            self.addChild(self._avatar)

        self._levelText = PlainText(self._font, "", self._HEADER_FONT_SIZE)
        self._levelText.setPosition((self._infoStartX, self._HEADER_ROW_Y))
        self.addChild(self._levelText)

        self._hpBack = SolidRect((self._hpBarWidth, self._HP_BAR_HEIGHT), Color(24, 24, 24, 220))
        self._hpBack.setPosition((0, self._HP_ROW_Y + self._HP_BAR_OFFSET_Y))
        self.addChild(self._hpBack)

        self._hpFill = SolidRect((self._hpBarWidth, self._HP_BAR_HEIGHT), Color(0, 192, 0, 255))
        self._hpFill.setPosition((0, self._HP_ROW_Y + self._HP_BAR_OFFSET_Y))
        self.addChild(self._hpFill)

        self._hpLabelText = PlainText(self._font, LOC("STAT_HP"), self._FONT_SIZE)
        self._hpLabelText.setPosition((0, self._HP_ROW_Y))
        self.addChild(self._hpLabelText)

        self._hpText = RichText(self._font, "", self._hpTextStyles)
        self._hpText.setPosition((self._STAT_VALUE_X, self._HP_ROW_Y))
        self.addChild(self._hpText)

        self._statValueTexts: Dict[str, PlainText] = {}
        for locKey, statKey, y in [
            ("STAT_ATK", "ATK", self._ATK_ROW_Y),
            ("STAT_DEF", "DEF", self._DEF_ROW_Y),
            ("STAT_EXP", "EXP", self._EXP_ROW_Y),
            ("STAT_GOLD", "GOLD", self._GOLD_ROW_Y),
        ]:
            labelText = PlainText(self._font, LOC(locKey), self._FONT_SIZE)
            labelText.setPosition((0, y))
            self.addChild(labelText)

            valueText = PlainText(self._font, "", self._FONT_SIZE)
            valueText.setLineAlignment(Text.LineAlignment.Right)
            valueText.setPosition((self._STAT_VALUE_X, y))
            self.addChild(valueText)
            self._statValueTexts[statKey] = valueText

        _purpleColor = Color(150, 0, 220, 255)
        debuffX = self._STAT_VALUE_X + self._DEBUFF_TEXT_OFFSET_X
        self._atkDebuffText = PlainText(self._font, "", 8, fillColor=_purpleColor)
        self._atkDebuffText.setPosition((debuffX, self._ATK_ROW_Y))
        self.addChild(self._atkDebuffText)

        self._defDebuffText = PlainText(self._font, "", 8, fillColor=_purpleColor)
        self._defDebuffText.setPosition((debuffX, self._DEF_ROW_Y))
        self.addChild(self._defDebuffText)

        _poisonColor = Color(0, 192, 0, 255)
        self._hpPoisonText = PlainText(self._font, "", 8, fillColor=_poisonColor)
        self._hpPoisonText.setPosition((debuffX, self._HP_ROW_Y))
        self.addChild(self._hpPoisonText)

        self._keyIcon: Optional[Image] = None
        try:
            keyTexture = Manager.loadSystem("Keys.png")
            self._keyIcon = Image(keyTexture)
            self._keyIcon.setPosition((0, self._KEY_ROW_Y))
            self.addChild(self._keyIcon)
        except Exception:
            self._keyIcon = None

        self._itemText = RichText(self._font, "", self._textStyles)
        self._itemText.setPosition((0, self._KEY_ROW_Y))
        self.addChild(self._itemText)

    def _loadStateIcon(self, iconPath: str) -> Optional[Texture]:
        if not iconPath:
            return None
        if iconPath in self._stateIconCache:
            return self._stateIconCache[iconPath]
        texture: Optional[Texture] = None
        try:
            if "/" in iconPath or "\\" in iconPath:
                parts = iconPath.replace("\\", "/").split("/")
                if len(parts) >= 2:
                    subfolder = "/".join(parts[:-1])
                    filename = parts[-1]
                    texture = Manager.loadTexture(subfolder, filename)
            else:
                texture = Manager.loadTexture("Icons/States", iconPath)
        except Exception:
            texture = None
        self._stateIconCache[iconPath] = texture
        return texture

    def _clearStateWidgets(self) -> None:
        for widget in self._stateWidgets:
            self.removeChild(widget)
        self._stateWidgets.clear()

    def _refreshStates(self) -> None:
        states = self._player.getStates()
        signature = tuple(state.ID for state in states)
        if signature == self._stateSignature:
            return
        self._stateSignature = signature
        self._clearStateWidgets()
        if not states:
            return

        x = 0.0
        y = float(self._avatarSize)
        for state in states:
            iconPath = state.icon or Data.getGeneralStateData(state.ID).get("icon", "")
            texture = self._loadStateIcon(iconPath) if iconPath else None
            if texture is not None:
                icon = Image(texture)
                texSize = texture.getSize()
                scale = self._STATE_ICON_SIZE / max(float(texSize.x), float(texSize.y), 1.0)
                icon.setScale(Vector2f(scale, scale))
                icon.setPosition((x, y))
                self.addChild(icon)
                self._stateWidgets.append(icon)
                x += self._STATE_ICON_SIZE + self._STATE_GAP
                continue

            nameText = PlainText(self._font, LOC(state.name), self._HEADER_FONT_SIZE)
            nameText.setPosition((x, y))
            self.addChild(nameText)
            self._stateWidgets.append(nameText)
            bounds = nameText.getLocalBounds()
            x += bounds.size.x + self._STATE_GAP

    def _refresh(self) -> None:
        hp = self._player.infoComp.HP
        maxhp = self._player.infoComp.MAXHP
        level = self._player.infoComp.LEVEL
        atk = self._player.infoComp.ATK
        defence = self._player.infoComp.DEF
        exp = self._player.infoComp.EXP
        gold = self._player.infoComp.GOLD

        self._levelText.setString(f"Lv. {level}")
        self._refreshStates()
        self._hpText.setString(f"#default#{ToShortNumber(hp)}/#max#{ToShortNumber(maxhp)}#default#")
        self._statValueTexts["ATK"].setString(f"{ToShortNumber(atk)}")
        self._statValueTexts["DEF"].setString(f"{ToShortNumber(defence)}")
        self._statValueTexts["EXP"].setString(f"{ToShortNumber(exp)}")
        self._statValueTexts["GOLD"].setString(f"{ToShortNumber(gold)}")

        weakStacks = self._player.getStateStacks().get("Weak", 0)
        debuffStr = f"(-{weakStacks})" if weakStacks > 0 else ""
        self._atkDebuffText.setString(debuffStr)
        self._defDebuffText.setString(debuffStr)

        poisonStacks = self._player.getStateStacks().get("Poisoned", 0)
        self._hpPoisonText.setString(f"({poisonStacks})" if poisonStacks > 0 else "")

        hpRate = hp / maxhp
        self._hpFill.setSize(Vector2f(self._hpBarWidth * hpRate, self._HP_BAR_HEIGHT))

        hpBounds = self._hpText.getLocalBounds()
        textY = self._HP_ROW_Y + (self._HP_TEXT_LAYOUT_HEIGHT - hpBounds.size.y) / 2.0 - hpBounds.position.y
        textX = self._STAT_VALUE_X - hpBounds.size.x - hpBounds.position.x
        self._hpLabelText.setPosition((0, textY))
        self._hpText.setPosition((textX, textY))
        self._hpPoisonText.setPosition((self._STAT_VALUE_X + self._DEBUFF_TEXT_OFFSET_X, textY))

        keyY_count = self._player.getItemCount("KEY_Y")
        keyB_count = self._player.getItemCount("KEY_B")
        keyR_count = self._player.getItemCount("KEY_R")
        displayText = (
            f"#Yellow#{keyY_count:02d}#default#  #Blue#{keyB_count:02d}#default#  #Red#{keyR_count:02d}#default#"
        )
        self._itemText.setString(displayText)
        itemBounds = self._itemText.getLocalBounds()
        itemX = self._STAT_VALUE_X - itemBounds.size.x - itemBounds.position.x
        keyRowHeight = max(self._FONT_SIZE, self._KEY_ICON_HEIGHT)
        itemY = self._KEY_ROW_Y + (keyRowHeight - itemBounds.size.y) / 2.0 - itemBounds.position.y
        self._itemText.setPosition((itemX, itemY))
        if self._keyIcon is not None:
            iconY = self._KEY_ROW_Y + (keyRowHeight - self._KEY_ICON_HEIGHT) / 2.0
            self._keyIcon.setPosition((0, iconY))
