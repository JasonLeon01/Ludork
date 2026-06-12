# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
from typing import Any, Callable, Dict, List, Optional, Tuple, Union
from Engine import Color, Input, IntRect, Pair, Text, Texture, UI, Vector2f, Vector2i
from Engine.UI import Canvas, ListView
from Engine.UI.Base import FunctionalBase
from Engine.UI.FunctionalUI import FImage, FPlainText
from Global import Manager
from .Base import WindowSelectable
from ..Battler import DamageType
from ..Enemy import Enemy
from ..System import System as GameSystem


_WINDOW_SIZE = 352
_CELL_WIDTH = 320
_CELL_HEIGHT = 64
_ICON_AREA_WIDTH = 64
_TEXT_SIZE = 11
_NAME_TEXT_SIZE = 10


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

    def update(self, deltaTime: float) -> None:
        r"""\brief Update animated icon and render this row to its canvas.

        - \param deltaTime Elapsed time in seconds.
        """
        self._animateIcon(deltaTime)
        super().update(deltaTime)
        self.render()

    def _buildIcon(self, entry: Dict[str, Any]) -> None:
        if self._texture is None or self._rect is None:
            return
        self._icon = FImage(self._texture, self._rect)
        scale = entry.get("scale", Vector2f(1.0, 1.0))
        displayScale = Vector2f(max(0.01, abs(scale.x)), max(0.01, abs(scale.y)))
        iconW = max(1.0, float(self._rect.size.x) * displayScale.x)
        iconH = max(1.0, float(self._rect.size.y) * displayScale.y)
        fit = min(1.0, (_ICON_AREA_WIDTH - 8) / iconW, (_CELL_HEIGHT - 20) / iconH)
        displayScale = Vector2f(displayScale.x * fit, displayScale.y * fit)
        self._icon.setScale(displayScale)
        iconW = float(self._rect.size.x) * displayScale.x
        iconH = float(self._rect.size.y) * displayScale.y
        self._icon.setPosition(Vector2f((_ICON_AREA_WIDTH - iconW) / 2.0, (48.0 - iconH) / 2.0))
        self.addChild(self._icon)

    def _buildTexts(self, entry: Dict[str, Any]) -> None:
        name = FPlainText(UI.DefaultFont, self._fitText(entry.get("name", ""), _ICON_AREA_WIDTH - 4), _NAME_TEXT_SIZE)
        name.setPosition(Vector2f(0.0, 49.0))
        self.addChild(name)

        statTexts = [
            (Vector2f(64.0, 6.0), f"MAXHP:{entry.get('MAXHP', 0)}"),
            (Vector2f(150.0, 6.0), f"ATK:{entry.get('ATK', 0)}"),
            (Vector2f(236.0, 6.0), f"DEF:{entry.get('DEF', 0)}"),
            (Vector2f(64.0, 34.0), f"EXP:{entry.get('EXP', 0)}"),
            (Vector2f(150.0, 34.0), f"GOLD:{entry.get('GOLD', 0)}"),
            (Vector2f(236.0, 34.0), f"DMG:{entry.get('damage', '--')}"),
        ]
        for position, value in statTexts:
            text = FPlainText(UI.DefaultFont, value, _TEXT_SIZE)
            if value.endswith("--"):
                text.setColor(Color(255, 96, 96, 255))
            text.setPosition(position)
            self.addChild(text)

    def _fitText(self, text: str, maxWidth: int) -> str:
        if not text:
            return ""
        result = text
        while result and self._measureText(result) > maxWidth:
            result = result[:-1]
        if result != text and len(result) > 1:
            result = result[:-1] + "."
        return result

    def _measureText(self, text: str) -> float:
        from Engine import Scale

        charSize = int(_NAME_TEXT_SIZE * Scale)
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
        player,
        onClose: Optional[Callable[[], None]] = None,
    ) -> None:
        r"""\brief Construct the enemy handbook window.

        - \param rect Window rectangle.
        - \param player Player used to calculate displayed damage.
        - \param onClose Optional callback invoked when the window closes.
        """
        super().__init__(rect, None, _CELL_WIDTH, _CELL_HEIGHT)
        self._player = player
        self._onCloseCallback = onClose
        self._enemies: List[Dict[str, Any]] = []
        self.setActive(False)
        self.setVisible(False)

    def setPlayer(self, player) -> None:
        r"""\brief Rebind the player used for damage preview.

        - \param player The current player instance.
        """
        self._player = player

    def open(self, gameMap) -> None:
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

    def _refreshEnemies(self, gameMap) -> None:
        entries: List[Dict[str, Any]] = []
        seen = set()
        if gameMap is not None:
            for actor in gameMap.getAllActors():
                if not isinstance(actor, Enemy) or actor.isDestroyed():
                    continue
                entry = self._buildEntry(actor)
                key = self._entryKey(entry)
                if key in seen:
                    continue
                seen.add(key)
                entries.append(entry)
        self._enemies = entries
        listView = ListView(
            IntRect(Vector2i(0, 0), Vector2i(int(self.content.getSize().x), int(self.content.getSize().y))),
            _CELL_HEIGHT,
            True,
            1,
        )
        for entry in entries:
            cell = _EnemyBookCell(entry)
            listView.addChild(cell)
        self.setListView(listView)
        self.index = 0 if entries else None
        if self._rect.getParent() is not None:
            self.content.removeChild(self._rect)

    def _buildEntry(self, enemy: Enemy) -> Dict[str, Any]:
        damageType, damage = enemy.getDamage(self._player)
        return {
            "name": self._formatName(enemy.infoComp.name or getattr(enemy, "ID", "")),
            "MAXHP": int(enemy.infoComp.MAXHP),
            "ATK": int(enemy.getATK(self._player)),
            "DEF": int(enemy.getDEF(self._player)),
            "EXP": int(enemy.infoComp.EXP),
            "GOLD": int(enemy.infoComp.GOLD),
            "damage": "--" if damageType == DamageType.UNDEFEATABLE else int(damage),
            "texture": enemy.getTexture(),
            "texturePath": getattr(enemy, "texturePath", ""),
            "rect": copy.copy(enemy.getTextureRect()),
            "scale": enemy.getScale(),
            "animatable": enemy.getAnimatable(),
            "switchInterval": getattr(enemy, "switchInterval", 0.2),
        }

    def _entryKey(self, entry: Dict[str, Any]) -> Tuple[Any, ...]:
        rect = entry.get("rect")
        rectKey = None
        if rect is not None:
            rectKey = (rect.position.x, rect.position.y, rect.size.x, rect.size.y)
        scale = entry.get("scale", Vector2f(1.0, 1.0))
        return (
            entry.get("name", ""),
            entry.get("MAXHP", 0),
            entry.get("ATK", 0),
            entry.get("DEF", 0),
            entry.get("EXP", 0),
            entry.get("GOLD", 0),
            entry.get("damage", "--"),
            entry.get("texturePath", ""),
            rectKey,
            (scale.x, scale.y),
        )

    def _formatName(self, name: str) -> str:
        try:
            return str(name).format(**LOC_D())
        except Exception:
            return str(name)

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
