# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
from typing import TYPE_CHECKING, Any, Callable, Dict, List, Optional, Tuple, Union
from Engine import Color, Input, IntRect, Pair, Text, Texture, UI, Vector2f, Vector2i
from Engine.UI import Canvas, ListView
from Engine.UI.Base import FunctionalBase
from Engine.UI.FunctionalUI import FImage, FPlainText
from Global import Manager
from .. import Data
from .Base import WindowSelectable
from ..Battler import DamageType
from ..Enemy import Enemy
from ..NodeFunctions.Utils import ToShortNumber
from ..System import System as GameSystem

if TYPE_CHECKING:
    from Global import GameMap
    from ..Player import Player


_WINDOW_SIZE = 352
_CELL_WIDTH = 320
_CELL_HEIGHT = 64
_ICON_AREA_WIDTH = 64
_TEXT_SIZE = 12
_NAME_TEXT_SIZE = 16
_SPECIAL_TEXT_SIZE = 10
_SPECIAL_ICON_SIZE = 16
_SPECIAL_GAP = 4
_SPECIAL_RIGHT_PAD = 4
_SPECIAL_NAME_MAX_WIDTH = 80
_specialIconCache: Dict[str, Optional[Texture]] = {}


def _loadSpecialIcon(iconPath: str) -> Optional[Texture]:
    if not iconPath:
        return None
    if iconPath in _specialIconCache:
        return _specialIconCache[iconPath]
    texture: Optional[Texture] = None
    try:
        if "/" in iconPath or "\\" in iconPath:
            parts = iconPath.replace("\\", "/").split("/")
            if len(parts) >= 2:
                subfolder = "/".join(parts[:-1])
                filename = parts[-1]
                texture = Manager.loadTexture(subfolder, filename)
        else:
            filename = iconPath if "." in iconPath else f"{iconPath}.png"
            texture = Manager.loadTexture("Icons/Specials", filename)
    except Exception:
        texture = None
    _specialIconCache[iconPath] = texture
    return texture


class _EnemyBookCell(Canvas, FunctionalBase):
    r"""\brief Single enemy-book row with an animated actor image and battle stats."""

    def __init__(self, entry: Dict[str, Any]) -> None:
        r"""\brief Construct a monster handbook cell.

        - \param entry Prepared enemy display data.
        """
        Canvas.__init__(self, ((0, 0), (_CELL_WIDTH, _CELL_HEIGHT)))
        FunctionalBase.__init__(self)
        self._icon: Optional[FImage] = None
        self._texture: Optional[Texture] = entry.get("texture")
        self._rect: Optional[IntRect] = copy.copy(entry.get("rect"))
        self._animatable = bool(entry.get("animatable", False))
        self._switchInterval = float(entry.get("switchInterval", 0.2))
        self._switchTimer = 0.0
        self._buildIcon(entry)
        self._buildTexts(entry)

    def getChildren(self):
        return []

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update animated icon and render this row to its canvas.

        - \param deltaTime Elapsed time in seconds.
        """
        self._animateIcon(deltaTime)
        self._buildRenderQueue()
        self.render()

    def _buildIcon(self, entry: Dict[str, Any]) -> None:
        if self._texture is None or self._rect is None:
            return
        self._icon = FImage(self._texture, self._rect)
        scale = entry.get("scale", Vector2f(1.0, 1.0))
        displayScale = Vector2f(max(0.01, abs(scale.x)), max(0.01, abs(scale.y)))
        iconW = max(1.0, float(self._rect.size.x) * displayScale.x)
        iconH = max(1.0, float(self._rect.size.y) * displayScale.y)
        fit = min(1.0, _ICON_AREA_WIDTH / iconW, _CELL_HEIGHT / iconH)
        displayScale = Vector2f(displayScale.x * fit, displayScale.y * fit)
        self._icon.setScale(displayScale)
        iconW = float(self._rect.size.x) * displayScale.x
        iconH = float(self._rect.size.y) * displayScale.y
        self._icon.setPosition(Vector2f((_ICON_AREA_WIDTH - iconW) / 2.0, (_CELL_HEIGHT - iconH) / 2.0))
        self.addChild(self._icon)

    def _buildTexts(self, entry: Dict[str, Any]) -> None:
        specialDisplays = entry.get("specialDisplays", [])
        specialAreaWidth = self._measureSpecialAreaWidth(specialDisplays)
        nameMaxWidth = max(32, int(_CELL_WIDTH - _ICON_AREA_WIDTH - specialAreaWidth))
        name = FPlainText(
            UI.DefaultFont,
            self._fitText(entry.get("name", ""), nameMaxWidth, _NAME_TEXT_SIZE),
            _NAME_TEXT_SIZE,
        )
        name.setPosition(Vector2f(64.0, 0.0))
        self.addChild(name)
        if specialDisplays:
            self._buildSpecials(specialDisplays)

        statTexts = [
            (Vector2f(64.0, 24.0), f"{LOC('STAT_HP')}{ToShortNumber(entry.get('MAXHP', 0))}"),
            (Vector2f(150.0, 24.0), f"{LOC('STAT_ATK')}{ToShortNumber(entry.get('ATK', 0))}"),
            (Vector2f(236.0, 24.0), f"{LOC('STAT_DEF')}{ToShortNumber(entry.get('DEF', 0))}"),
            (Vector2f(64.0, 44.0), f"{LOC('STAT_EXP')}{ToShortNumber(entry.get('EXP', 0))}"),
            (Vector2f(150.0, 44.0), f"{LOC('STAT_GOLD')}{ToShortNumber(entry.get('GOLD', 0))}"),
            (Vector2f(236.0, 44.0), f"{LOC('STAT_DMG')}{ToShortNumber(entry.get('damage', '--'))}"),
        ]
        for position, value in statTexts:
            text = FPlainText(UI.DefaultFont, value, _TEXT_SIZE)
            if value.endswith("???"):
                text.setColour(Color(255, 96, 96, 255))
            text.setPosition(position)
            self.addChild(text)

    def _measureSpecialAreaWidth(self, specialDisplays: List[Dict[str, Any]]) -> float:
        if not specialDisplays:
            return 0.0
        width = float(_SPECIAL_RIGHT_PAD)
        for index, item in enumerate(specialDisplays):
            if index > 0:
                width += _SPECIAL_GAP
            if item.get("texture") is not None:
                width += _SPECIAL_ICON_SIZE
            else:
                displayName = self._fitText(str(item.get("name", "")), _SPECIAL_NAME_MAX_WIDTH, _SPECIAL_TEXT_SIZE)
                width += self._measureText(displayName, _SPECIAL_TEXT_SIZE)
        return width

    def _buildSpecials(self, specialDisplays: List[Dict[str, Any]]) -> None:
        currentX = float(_CELL_WIDTH) - _SPECIAL_RIGHT_PAD
        for item in reversed(specialDisplays):
            texture = item.get("texture")
            if texture is not None:
                icon = FImage(texture)
                texSize = texture.getSize()
                scale = _SPECIAL_ICON_SIZE / max(float(texSize.x), float(texSize.y), 1.0)
                icon.setScale(Vector2f(scale, scale))
                iconX = currentX - _SPECIAL_ICON_SIZE
                icon.setPosition(Vector2f(iconX, 0.0))
                self.addChild(icon)
                currentX = iconX - _SPECIAL_GAP
                continue
            displayName = self._fitText(str(item.get("name", "")), _SPECIAL_NAME_MAX_WIDTH, _SPECIAL_TEXT_SIZE)
            textWidth = self._measureText(displayName, _SPECIAL_TEXT_SIZE)
            text = FPlainText(UI.DefaultFont, displayName, _SPECIAL_TEXT_SIZE)
            text.setLineAlignment(Text.LineAlignment.Right)
            text.setPosition(Vector2f(currentX, 0.0))
            self.addChild(text)
            currentX -= textWidth + _SPECIAL_GAP

    def _fitText(self, text: str, maxWidth: int, textSize: int) -> str:
        if not text:
            return ""
        result = text
        while result and self._measureText(result, textSize) > maxWidth:
            result = result[:-1]
        if result != text and len(result) > 1:
            result = result[:-1] + "."
        return result

    def _measureText(self, text: str, textSize: int) -> float:
        from Engine import Scale

        charSize = int(textSize * Scale)
        return sum(UI.DefaultFont.getGlyph(ch, charSize, False).advance for ch in text) / Scale

    def _animateIcon(self, deltaTime: float) -> None:
        if not self._animatable or self._icon is None or self._texture is None or self._rect is None:
            return
        self._switchTimer += deltaTime
        if self._switchTimer < self._switchInterval:
            return
        self._switchTimer = 0.0
        rect = copy.copy(self._rect)
        textureWidth = self._texture.getSize().x
        rect.position.x = (rect.position.x + rect.size.x) % textureWidth
        self._rect = rect
        self._icon.setTextureRect(rect)


class WindowEnemyBook(WindowSelectable):
    r"""\brief Selectable monster handbook for enemies on the current map."""

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        player: Player,
        onClose: Optional[Callable[[], None]] = None,
        onConfirm: Optional[Callable[[Dict[str, Any]], None]] = None,
    ) -> None:
        r"""\brief Construct the enemy handbook window.

        - \param rect Window rectangle.
        - \param player Player used to calculate displayed damage.
        - \param onClose Optional callback invoked when the window closes.
        - \param onConfirm Optional callback invoked when an enemy is confirmed.
        """
        super().__init__(rect, None, _CELL_WIDTH, _CELL_HEIGHT)
        self._player: Player = player
        self._onCloseCallback = onClose
        self._onConfirmCallback = onConfirm
        self._enemies: List[Dict[str, Any]] = []
        self.setActive(False)
        self.setVisible(False)

    def setPlayer(self, player: Player) -> None:
        r"""\brief Rebind the player used for damage preview.

        - \param player The current player instance.
        """
        self._player = player

    def open(self, gameMap: Optional[GameMap]) -> None:
        r"""\brief Open the handbook and rescan current-map enemies.

        - \param gameMap Current map to scan.
        """
        self._refreshEnemies(gameMap)
        self.setVisible(True)
        self.setActive(True)

    def close(self) -> None:
        r"""\brief Close the handbook."""
        self.setVisible(False)
        self.setActive(False)

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Close on cancel, otherwise use selectable navigation.

        - \param kwargs Event data.
        """
        if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
            self._closeByCancel()
            return
        super().onKeyDown(kwargs)

    def onMouseButtonDown(self, kwargs: Dict[str, Any]) -> bool:
        r"""\brief Close on right click."""
        if kwargs["button"] == Input.Mouse.Button.Right:
            Input.getMouseButtonPressed(Input.Mouse.Button.Right, handled=True)
            Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True)
            self._closeByCancel()
            return True
        return False

    def _refreshEnemies(self, gameMap: Optional[GameMap]) -> None:
        entries: List[Dict[str, Any]] = []
        seen: set[str] = set()
        if gameMap is not None:
            for actor in gameMap.getAllActors():
                if not isinstance(actor, Enemy) or actor.isDestroyed():
                    continue
                enemyID = actor.ID
                if enemyID in seen:
                    continue
                seen.add(enemyID)
                entries.append(self._buildEntry(actor))
        self._enemies = entries
        listView = ListView(
            IntRect(Vector2i(0, 0), Vector2i(int(self.content.getSize().x), int(self.content.getSize().y))),
            _CELL_HEIGHT,
            True,
            1,
        )
        for entry in entries:
            cell = _EnemyBookCell(entry)
            cell.addConfirmCallback(lambda obj, kwargs, enemyEntry=entry: self._confirmEnemy(enemyEntry))
            listView.addChild(cell)
        self.setListView(listView)
        self.index = 0 if entries else None
        if self._rect.getParent() is not None:
            self.content.removeChild(self._rect)

    def _buildEntry(self, enemy: Enemy) -> Dict[str, Any]:
        damageType, damage = enemy.getDamage(self._player)
        special = enemy.getSpecial()
        return {
            "name": self._formatName(enemy.infoComp.name or enemy.ID),
            "MAXHP": int(enemy.infoComp.MAXHP),
            "ATK": int(enemy.getATK(self._player)),
            "DEF": int(enemy.getDEF(self._player)),
            "EXP": int(enemy.infoComp.EXP),
            "GOLD": int(enemy.infoComp.GOLD),
            "damage": "???" if damageType == DamageType.UNDEFEATABLE else int(damage),
            "critical": int(enemy.getCriticalValue(self._player)),
            "hitCount": int(enemy.getHitCount()) if enemy.hasSpecial("MultiHit") else None,
            "specialDisplays": self._buildSpecialDisplays(special),
            "specialDetails": self._buildSpecialDetails(special),
            "texture": enemy.getTexture(),
            "texturePath": enemy.texturePath,
            "rect": copy.copy(enemy.getTextureRect()),
            "scale": enemy.getScale(),
            "animatable": enemy.getAnimatable(),
            "switchInterval": enemy.switchInterval,
        }

    def _buildSpecialDisplays(self, special: Dict[str, Any]) -> List[Dict[str, Any]]:
        if not isinstance(special, dict) or len(special) == 0:
            return []
        if len(special) > 3:
            return [{"texture": None, "name": LOC("MORE_SPECIAL")}]
        displays: List[Dict[str, Any]] = []
        for specialKey in special.keys():
            specialData = Data.getGeneralSpecialData(str(specialKey))
            name = self._formatName(specialData.get("name", specialKey))
            iconPath = str(specialData.get("icon", "") or specialKey)
            displays.append({"texture": _loadSpecialIcon(iconPath), "name": name})
        return displays

    def _buildSpecialDetails(self, special: Dict[str, Any]) -> List[Dict[str, str]]:
        if not isinstance(special, dict) or len(special) == 0:
            return []
        details: List[Dict[str, str]] = []
        for specialKey in special.keys():
            specialData = Data.getGeneralSpecialData(str(specialKey))
            details.append(
                {
                    "name": self._formatText(specialData.get("name", specialKey)),
                    "desc": self._formatText(specialData.get("desc", "")),
                }
            )
        return details

    def _formatName(self, name: str) -> str:
        return self._formatText(name)

    def _formatText(self, text: str) -> str:
        try:
            return str(text).format(**LOC_D()).replace("\\n", "\n")
        except Exception:
            return str(text)

    def _getRectPosition(self) -> Optional[Vector2f]:
        if self.index is None:
            return None
        return Vector2f(0.0, float(self.index * _CELL_HEIGHT))

    def _getRectWidth(self) -> int:
        return _CELL_WIDTH

    def _closeByCancel(self) -> None:
        Manager.playSE(GameSystem.getCancelSE())
        self.close()
        if self._onCloseCallback is not None:
            self._onCloseCallback()

    def _confirmEnemy(self, entry: Dict[str, Any]) -> None:
        Manager.playSE(GameSystem.getDecisionSE())
        self.close()
        if self._onConfirmCallback is not None:
            self._onConfirmCallback(entry)
