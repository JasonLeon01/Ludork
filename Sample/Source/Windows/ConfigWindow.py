# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Callable, Dict, List, Optional, Union, Tuple
from Engine import Pair, Image, IntRect, Vector2i, UI, Vector2f, Input, TypeAdapter
from Engine.UI import Canvas, ListView
from Global import Manager, System
from .Base import WindowSelectable
from .General import ConfigCheckBoxRow, ConfigSettingRow


_DEFAULT_RECT: Tuple[Pair[int], Pair[int]] = ((80, 48), (480, 320))
_DROPBOX_WIDTH: int = 200
_CHECKBOX_SIZE: int = 32
_ROW_HEIGHT: int = 32
_LANGUAGE_ITEMS: List[str] = ["en_GB", "zh_CN"]
_SCALE_ITEMS: List[str] = ["1", "1.25", "1.5", "2.0"]
_FRAMERATE_ITEMS: List[str] = ["30", "60", "90", "120"]


class ConfigWindow(WindowSelectable):
    r"""\brief In-game configuration window backed by a selectable setting list.

    Each setting row combines a label and an interactive control.
    """

    @TypeAdapter(rect=([tuple, list], IntRect, lambda pos, size: IntRect(Vector2i(*pos), Vector2i(*size))))
    def __init__(
        self,
        onClose: Optional[Callable[[], None]] = None,
    ) -> None:
        r"""\brief Construct the configuration window.

        - \param rect        Window rectangle in logical UI units
        - \param windowSkin  Optional windowskin image
        - \param onClose     Optional callback when the window is closed
        """
        windowSkin = Manager.loadSystem(UI.DefaultWindowskinName, smooth=True).copyToImage()
        contentWidth = _DEFAULT_RECT[1][0]
        super().__init__(_DEFAULT_RECT, None, contentWidth, _ROW_HEIGHT, windowSkin)
        self._onClose = onClose
        self._open = False
        contentSize = self.content.getSize()
        listView = ListView(
            IntRect(
                Vector2i(0, 0),
                Vector2i(int(contentSize.x), int(contentSize.y)),
            ),
            _ROW_HEIGHT,
            False,
            1,
        )
        self._languageRow = ConfigSettingRow(
            LOC("language"),
            _LANGUAGE_ITEMS,
            int(contentSize.x),
            _DROPBOX_WIDTH,
            windowSkin,
            self._findSelectedIndex(_LANGUAGE_ITEMS, System.getLanguage()),
        )
        self._scaleRow = ConfigSettingRow(
            LOC("scale"),
            _SCALE_ITEMS,
            int(contentSize.x),
            _DROPBOX_WIDTH,
            windowSkin,
            self._findSelectedIndex(_SCALE_ITEMS, System.getScale()),
        )
        self._framerateRow = ConfigSettingRow(
            LOC("framerate"),
            _FRAMERATE_ITEMS,
            int(contentSize.x),
            _DROPBOX_WIDTH,
            windowSkin,
            self._findSelectedIndex(_FRAMERATE_ITEMS, System.getFrameRate()),
        )
        self._verticalSyncRow = ConfigCheckBoxRow(
            LOC("verticalsync"),
            int(contentSize.x),
            _CHECKBOX_SIZE,
            windowSkin,
            System.getVerticalSync(),
            self._onVerticalSyncCheckedChanged,
        )
        self._musicOnRow = ConfigCheckBoxRow(
            LOC("musicon"),
            int(contentSize.x),
            _CHECKBOX_SIZE,
            windowSkin,
            System.getMusicOn(),
            self._onMusicOnCheckedChanged,
        )
        self._soundOnRow = ConfigCheckBoxRow(
            LOC("soundon"),
            int(contentSize.x),
            _CHECKBOX_SIZE,
            windowSkin,
            System.getSoundOn(),
            self._onSoundOnCheckedChanged,
        )
        self._voiceOnRow = ConfigCheckBoxRow(
            LOC("voiceon"),
            int(contentSize.x),
            _CHECKBOX_SIZE,
            windowSkin,
            System.getVoiceOn(),
            self._onVoiceOnCheckedChanged,
        )
        self._dropBoxRows = [self._languageRow, self._scaleRow, self._framerateRow]
        self._settingRows = [
            self._languageRow,
            self._scaleRow,
            self._framerateRow,
            self._verticalSyncRow,
            self._musicOnRow,
            self._soundOnRow,
            self._voiceOnRow,
        ]
        for row in self._dropBoxRows:
            listView.addChild(row)
            row.addConfirmCallback(self._makeSettingRowConfirmCallback(row))
            row.getDropBox().setOnExpandedChanged(self._onDropBoxExpandedChanged)
        self._languageRow.getDropBox().setOnSelectedIndexChanged(self._onLanguageSelectedIndexChanged)
        self._scaleRow.getDropBox().setOnSelectedIndexChanged(self._onScaleSelectedIndexChanged)
        self._framerateRow.getDropBox().setOnSelectedIndexChanged(self._onFrameRateSelectedIndexChanged)
        listView.addChild(self._verticalSyncRow)
        listView.addChild(self._musicOnRow)
        listView.addChild(self._soundOnRow)
        listView.addChild(self._voiceOnRow)
        self.setListView(listView)
        self.setVisible(False)
        self.setActive(False)

    def getLanguageDropBox(self):
        r"""\brief Get the language DropBox on the first settings row.

        - \return  Language DropBox coordinator
        """
        return self._languageRow.getDropBox()

    def getScaleDropBox(self):
        r"""\brief Get the scale DropBox on the scale settings row.

        - \return  Scale DropBox coordinator
        """
        return self._scaleRow.getDropBox()

    def getFramerateDropBox(self):
        r"""\brief Get the framerate DropBox on the framerate settings row.

        - \return  Framerate DropBox coordinator
        """
        return self._framerateRow.getDropBox()

    def getVerticalSyncCheckBox(self):
        r"""\brief Get the vertical-sync CheckBox on the settings list.

        - \return  Vertical-sync CheckBox coordinator
        """
        return self._verticalSyncRow.getCheckBox()

    def getMusicOnCheckBox(self):
        r"""\brief Get the music-enabled CheckBox on the settings list.

        - \return  Music-enabled CheckBox coordinator
        """
        return self._musicOnRow.getCheckBox()

    def getSoundOnCheckBox(self):
        r"""\brief Get the sound-enabled CheckBox on the settings list.

        - \return  Sound-enabled CheckBox coordinator
        """
        return self._soundOnRow.getCheckBox()

    def getVoiceOnCheckBox(self):
        r"""\brief Get the voice-enabled CheckBox on the settings list.

        - \return  Voice-enabled CheckBox coordinator
        """
        return self._voiceOnRow.getCheckBox()

    def isOpen(self) -> bool:
        r"""\brief Check whether this window is currently open.

        - \return  True if open, False otherwise
        """
        return self._open

    def open(self) -> None:
        r"""\brief Show and activate the configuration window."""
        self._open = True
        self.setVisible(True)
        self.setActive(True)

    def close(self) -> None:
        r"""\brief Hide and deactivate the configuration window."""
        self._open = False
        self._collapseAllDropBoxes()
        self.setVisible(False)
        self.setActive(False)
        if self._onClose is not None:
            self._onClose()

    def update(self, deltaTime: float) -> None:
        r"""\brief Update the window, delegating input to an expanded DropBox when needed.

        - \param deltaTime  Elapsed time in seconds
        """
        expandedRow = self._getExpandedSettingRow()
        if expandedRow is not None:
            expandedRow.update(deltaTime)
            Canvas.update(self, deltaTime)
            self._rect.setVisible(False)
            return
        super().update(deltaTime)
        self._rect.setVisible(False)

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Close the window on cancel when no DropBox is expanded.

        - \param kwargs  Event arguments
        """
        if not self.getActive():
            return
        if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
            self.close()
            return
        super().onKeyDown(kwargs)

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Close the window on right-click when visible and active."""
        if not self.getActive() or not self.getVisible():
            return
        if Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True):
            self.close()

    def _makeSettingRowConfirmCallback(self, row: ConfigSettingRow) -> Callable:
        def _onSettingRowConfirm(obj: ConfigSettingRow, kwargs: Dict[str, Any]) -> None:
            row.getDropBox().setExpanded(True)

        return _onSettingRowConfirm

    def _onDropBoxExpandedChanged(self, expanded: bool) -> None:
        if expanded:
            self.setActive(False)
            expandedRow = self._getExpandedSettingRow()
            for row in self._settingRows:
                row.setActive(row is expandedRow)
        elif self._open:
            self.setActive(True)
            for row in self._settingRows:
                row.setActive(True)

    def _getExpandedSettingRow(self) -> Optional[ConfigSettingRow]:
        for row in self._dropBoxRows:
            if row.getDropBox().isExpanded():
                return row
        return None

    def _anyDropBoxExpanded(self) -> bool:
        return self._getExpandedSettingRow() is not None

    def _collapseAllDropBoxes(self) -> None:
        for row in self._dropBoxRows:
            row.getDropBox().setExpanded(False)

    def _onVerticalSyncCheckedChanged(self, checked: bool) -> None:
        System.setVerticalSync(checked)

    def _onLanguageSelectedIndexChanged(self, index: int) -> None:
        System.saveLanguage(_LANGUAGE_ITEMS[index])
        self._showRestartMindTip()

    def _onScaleSelectedIndexChanged(self, index: int) -> None:
        System.saveScale(float(_SCALE_ITEMS[index]))
        self._showRestartMindTip()

    def _onFrameRateSelectedIndexChanged(self, index: int) -> None:
        System.setFrameRate(int(_FRAMERATE_ITEMS[index]))

    def _onMusicOnCheckedChanged(self, checked: bool) -> None:
        System.setMusicOn(checked)

    def _onSoundOnCheckedChanged(self, checked: bool) -> None:
        System.setSoundOn(checked)

    def _onVoiceOnCheckedChanged(self, checked: bool) -> None:
        System.setVoiceOn(checked)

    def _showRestartMindTip(self) -> None:
        scene = System.getScene()
        if scene is not None:
            scene.addCommonTip(LOC("CONFIG_MIND_RESTART"))

    @staticmethod
    def _findSelectedIndex(items: List[str], value: Union[str, int, float]) -> int:
        textValue = str(value)
        if textValue in items:
            return items.index(textValue)
        try:
            numericValue = float(value)
        except (TypeError, ValueError):
            return 0
        for index, item in enumerate(items):
            try:
                if float(item) == numericValue:
                    return index
            except ValueError:
                continue
        return 0

    def _getRectWidth(self) -> int:
        r"""\brief Selection width spans the full content area.

        - \return  Content width in logical UI units
        """
        return int(self.content.getSize().x)

    def _getRectPosition(self) -> Optional[Vector2f]:
        r"""\brief Selection rect in content space; rows are full-width without ListView column inset.

        - \return  Top-left of the selection rectangle, or None when no index
        """
        if self.index is None:
            return None
        columns = self._getColumns()
        x = (self.index % columns) * self._rectWidth
        y = (self.index // columns) * self._rectHeight
        return Vector2f(float(x), float(y))
