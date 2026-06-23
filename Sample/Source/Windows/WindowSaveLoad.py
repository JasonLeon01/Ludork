# -*- encoding: utf-8 -*-

from __future__ import annotations
from concurrent.futures import ThreadPoolExecutor, as_completed
import datetime
import os
from typing import Any, Callable, Dict, Optional, Union, Tuple
from Engine import (
    Color,
    Image as EngineImage,
    Input,
    IntRect,
    Pair,
    Texture,
    UI,
    Vector2f,
)
from Engine.UI import ListView
from Engine.UI import Image as UIImage
from Engine.UI.FunctionalUI import FPlainText
from Global import Manager
from .WindowCommand import WindowCommand
from .Base import WindowSelectable, WindowBase
from ..System import System as GameSystem
from ..GameInstance import GameInstance
from .. import Save


_SLOT_ROW_HEIGHT = 32
MAX_SAVE_SLOTS = 100

_DETAIL_WINDOW_SIZE = 256
_DETAIL_THUMB_WIDTH = 224
_DETAIL_THUMB_HEIGHT = 168
_DETAIL_TIMESTAMP_FONT_SIZE = 18
_DETAIL_TIMESTAMP_GAP = 8

_DEFAULT_COMMAND_RECT: Tuple[Pair[int], Pair[int]] = ((192, 0), (416, 64))
_DEFAULT_SLOT_RECT: Tuple[Pair[int], Pair[int]] = ((192, 64), (160, 256))
_DEFAULT_DETAIL_RECT: Tuple[Pair[int], Pair[int]] = (
    (352, 64),
    (_DETAIL_WINDOW_SIZE, _DETAIL_WINDOW_SIZE),
)


CloseReason = str
CLOSE_REASON_CANCEL: CloseReason = "cancel"
CLOSE_REASON_SAVED: CloseReason = "saved"
CLOSE_REASON_LOADED: CloseReason = "loaded"


def _getSaveFileMTime(slotIndex: int, filePath: str) -> Optional[Tuple[int, float]]:
    try:
        return slotIndex, os.path.getmtime(filePath)
    except OSError:
        return None


def _isNewerSaveFile(candidate: Tuple[int, float], current: Optional[Tuple[int, float]]) -> bool:
    if current is None:
        return True
    if candidate[1] != current[1]:
        return candidate[1] > current[1]
    return candidate[0] < current[0]


def _findLatestSaveSlotIndex(maxSlots: int) -> Optional[int]:
    saveFiles = [(idx, Save.GetSavePath(idx + 1)) for idx in range(maxSlots)]
    latest: Optional[Tuple[int, float]] = None
    try:
        maxWorkers = min(32, max(1, maxSlots))
        with ThreadPoolExecutor(max_workers=maxWorkers) as executor:
            futures = [executor.submit(_getSaveFileMTime, idx, filePath) for idx, filePath in saveFiles]
            for future in as_completed(futures):
                result = future.result()
                if result is None:
                    continue
                if _isNewerSaveFile(result, latest):
                    latest = result
    except Exception:
        latest = None
        for idx, filePath in saveFiles:
            result = _getSaveFileMTime(idx, filePath)
            if result is None:
                continue
            if _isNewerSaveFile(result, latest):
                latest = result
    if latest is None:
        return None
    return latest[0]


class WindowSaveCommand(WindowCommand):
    r"""\brief Horizontal load/save command bar; selecting a command picks the slot mode."""

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        owner: "WindowSaveLoad",
    ) -> None:
        r"""\brief Construct the save/load command bar.

        - \param rect The window rectangle.
        - \param owner The parent save/load UI coordinator.
        """
        commands = {
            "Load": {
                "text": LOC("MENU_LOAD"),
                "callback": lambda obj, kwargs: owner.onCommandConfirm("load"),
            },
            "Save": {
                "text": LOC("MENU_SAVE"),
                "callback": lambda obj, kwargs: owner.onCommandConfirm("save"),
            },
        }
        super().__init__(rect, commands, columns=2, rectHeight=32)
        self._owner = owner

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
            self._owner.closeByCancel()
            return
        super().onKeyDown(kwargs)

    def onMouseButtonDown(self, kwargs: Dict[str, Any]) -> bool:
        if kwargs["button"] == Input.Mouse.Button.Right:
            Input.getMouseButtonPressed(Input.Mouse.Button.Right, handled=True)
            Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True)
            self._owner.closeByCancel()
            return True
        return False


class WindowSaveSlot(WindowSelectable):
    r"""\brief Save-file slot list (1..MAX_SAVE_SLOTS) for load/save selection."""

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        owner: "WindowSaveLoad",
    ) -> None:
        r"""\brief Construct the save slot list window.

        - \param rect The window rectangle.
        - \param owner The parent save/load UI coordinator.
        """
        super().__init__(rect, None, None, _SLOT_ROW_HEIGHT)
        self._owner = owner
        listView = ListView(self.content.getNoTranslationRect(), 32, True, 1)
        for idx in range(MAX_SAVE_SLOTS):
            child = FPlainText(UI.DefaultFont, f"File {idx + 1}", UI.DefaultFontSize)
            child.addConfirmCallback(lambda obj, kwargs, slot=idx: self._owner.onSlotConfirm(slot))
            self._applyItem(child)
            listView.addChild(child)
        self.setListView(listView)

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
            self._owner.cancelSlotSelection()
            return
        super().onKeyDown(kwargs)

    def onTick(self, deltaTime: float) -> None:
        super().onTick(deltaTime)
        self._owner.notifySlotIndexMaybeChanged(self.index)

    def onMouseButtonDown(self, kwargs: Dict[str, Any]) -> bool:
        if kwargs["button"] == Input.Mouse.Button.Right:
            Input.getMouseButtonPressed(Input.Mouse.Button.Right, handled=True)
            Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True)
            self._owner.cancelSlotSelection()
            return True
        return False


class WindowSaveDetail(WindowBase):
    r"""\brief Save-file detail panel showing the current slot's screenshot and timestamp.

    Renders the snapshot horizontally filling the content area at a 4:3 ratio
    and displays the file's last-modified timestamp underneath. When the slot
    has no save file on disk, both the snapshot and timestamp stay hidden.
    """

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
    ) -> None:
        r"""\brief Construct the detail panel.

        - \param rect The window rectangle (expected 256x256).
        """
        super().__init__(rect)
        self._currentSlot: Optional[int] = None
        self._cachedFilePath: str = ""
        self._cachedFileMTime: float = -1.0
        self._thumbTexture: Optional[Texture] = None
        placeholderTexture = Texture(EngineImage(self._thumbSize(), Color.Transparent))
        placeholderTexture.setSmooth(True)
        self._thumbTexture = placeholderTexture
        self._thumbnail = UIImage(self._thumbTexture)
        self._thumbnail.setPosition(Vector2f(0.0, 0.0))
        self._thumbnail.setVisible(False)
        self.content.addChild(self._thumbnail)
        self._timestampText = FPlainText(UI.DefaultFont, "", _DETAIL_TIMESTAMP_FONT_SIZE)
        self._timestampText.setPosition(
            Vector2f(0.0, float(_DETAIL_THUMB_HEIGHT + _DETAIL_TIMESTAMP_GAP))
        )
        self._timestampText.setVisible(False)
        self.content.addChild(self._timestampText)

    def setSlot(self, slot: Optional[int]) -> None:
        r"""\brief Set the slot index to display, or ``None`` to clear the panel.

        - \param slot Zero-based slot index or ``None``.
        """
        if slot == self._currentSlot:
            self._refreshIfFileChanged()
            return
        self._currentSlot = slot
        self._cachedFilePath = ""
        self._cachedFileMTime = -1.0
        self._refreshContent()

    def refresh(self) -> None:
        r"""\brief Force-refresh the panel against the current slot's save file."""
        self._cachedFilePath = ""
        self._cachedFileMTime = -1.0
        self._refreshContent()

    def onTick(self, deltaTime: float) -> None:
        super().onTick(deltaTime)
        self._refreshIfFileChanged()

    @staticmethod
    def _thumbSize():
        from Engine import Vector2u

        return Vector2u(_DETAIL_THUMB_WIDTH, _DETAIL_THUMB_HEIGHT)

    def _refreshIfFileChanged(self) -> None:
        if self._currentSlot is None:
            return
        filePath = Save.GetSavePath(self._currentSlot + 1)
        if not os.path.exists(filePath):
            if self._cachedFilePath != "" or self._thumbnail.getVisible():
                self._cachedFilePath = ""
                self._cachedFileMTime = -1.0
                self._hideContent()
            return
        try:
            mtime = os.path.getmtime(filePath)
        except OSError:
            return
        if filePath == self._cachedFilePath and mtime == self._cachedFileMTime:
            return
        self._cachedFilePath = filePath
        self._cachedFileMTime = mtime
        self._loadAndDisplay(filePath, mtime)

    def _refreshContent(self) -> None:
        if self._currentSlot is None:
            self._hideContent()
            return
        filePath = Save.GetSavePath(self._currentSlot + 1)
        if not os.path.exists(filePath):
            self._hideContent()
            return
        try:
            mtime = os.path.getmtime(filePath)
        except OSError:
            self._hideContent()
            return
        self._cachedFilePath = filePath
        self._cachedFileMTime = mtime
        self._loadAndDisplay(filePath, mtime)

    def _loadAndDisplay(self, filePath: str, mtime: float) -> None:
        instance = Save.LoadGame(filePath)
        if instance is None:
            self._hideContent()
            return
        screenshot = instance.getScreenshot()
        if not self._applyScreenshot(screenshot):
            self._thumbnail.setVisible(False)
        self._timestampText.setString(self._formatTimestamp(mtime))
        self._timestampText.setVisible(True)

    def _applyScreenshot(self, screenshot: Optional[Any]) -> bool:
        if not screenshot:
            return False
        try:
            buffer = bytes(screenshot)
            image = EngineImage(buffer, len(buffer))
            texture = Texture(image)
            texture.setSmooth(True)
        except Exception:
            return False
        self._thumbTexture = texture
        self._thumbnail.setTexture(texture, True)
        imageSize = image.getSize()
        if imageSize.x <= 0 or imageSize.y <= 0:
            self._thumbnail.setVisible(False)
            return False
        scaleX = float(_DETAIL_THUMB_WIDTH) / float(imageSize.x)
        scaleY = float(_DETAIL_THUMB_HEIGHT) / float(imageSize.y)
        self._thumbnail.setScale(Vector2f(scaleX, scaleY))
        self._thumbnail.setPosition(Vector2f(0.0, 0.0))
        self._thumbnail.setVisible(True)
        return True

    def _hideContent(self) -> None:
        self._thumbnail.setVisible(False)
        self._timestampText.setVisible(False)
        self._timestampText.setString("")

    @staticmethod
    def _formatTimestamp(mtime: float) -> str:
        return datetime.datetime.fromtimestamp(mtime).strftime("%Y-%m-%d %H:%M:%S")


class WindowSaveLoad:
    r"""\brief Integrated save/load UI: command bar, slot list, and detail panel.

    Owner-agnostic coordinator. Hosts pass callbacks for close and load events
    instead of being referenced directly, so the same UI can serve the in-game
    menu, the title screen, or any other entry point.
    """

    def __init__(
        self,
        commandRect: Union[IntRect, Tuple[Pair[int], Pair[int]]] = _DEFAULT_COMMAND_RECT,
        slotRect: Union[IntRect, Tuple[Pair[int], Pair[int]]] = _DEFAULT_SLOT_RECT,
        detailRect: Union[IntRect, Tuple[Pair[int], Pair[int]]] = _DEFAULT_DETAIL_RECT,
        loadOnly: bool = False,
        getSaveSource: Optional[Callable[[], Optional[GameInstance]]] = None,
        onClose: Optional[Callable[[CloseReason], None]] = None,
        onLoaded: Optional[Callable[[GameInstance], None]] = None,
    ) -> None:
        r"""\brief Construct the save/load UI coordinator and child windows.

        - \param commandRect Rectangle for the load/save command bar (ignored when load-only).
        - \param slotRect Rectangle for the save slot list window.
        - \param detailRect Rectangle for the save detail panel.
        - \param loadOnly When True, no save command is exposed and the slot list opens directly.
        - \param getSaveSource Callable returning the GameInstance to persist when saving.
        - \param onClose Callback invoked after the UI closes, with the close reason.
        - \param onLoaded Callback invoked with the loaded GameInstance after a successful load.
        """
        self._loadOnly = loadOnly
        self._getSaveSource = getSaveSource
        self._onCloseCallback = onClose
        self._onLoadedCallback = onLoaded
        self._mode = "load"
        self._commandWindow: Optional[WindowSaveCommand] = (
            None if loadOnly else WindowSaveCommand(commandRect, self)
        )
        self._slotWindow = WindowSaveSlot(slotRect, self)
        self._detailWindow = WindowSaveDetail(detailRect)
        if self._commandWindow is not None:
            self._commandWindow.setActive(False)
            self._commandWindow.setVisible(False)
        self._slotWindow.setActive(False)
        self._slotWindow.setVisible(False)
        self._detailWindow.setActive(False)
        self._detailWindow.setVisible(False)
        self._lastSlotIndex: Optional[int] = None
        self._selectLatestSaveSlot()

    def getCommandWindow(self) -> Optional[WindowSaveCommand]:
        r"""\brief Get the horizontal load/save command window.

        - \return The command window instance, or None when running in load-only mode.
        """
        return self._commandWindow

    def getSlotWindow(self) -> WindowSaveSlot:
        r"""\brief Get the save slot list window.

        - \return The slot list window instance.
        """
        return self._slotWindow

    def getDetailWindow(self) -> WindowSaveDetail:
        r"""\brief Get the save detail panel window.

        - \return The detail panel instance.
        """
        return self._detailWindow

    def getVisible(self) -> bool:
        r"""\brief Get the visibility of the save/load UI.

        - \return Whether the slot list is visible (treated as the canonical state).
        """
        return self._slotWindow.getVisible()

    def setVisible(self, visible: bool) -> None:
        r"""\brief Set the visibility of all save/load child windows.

        - \param visible Whether to show or hide the windows.
        """
        if self._commandWindow is not None:
            self._commandWindow.setVisible(visible)
        self._slotWindow.setVisible(visible)
        self._detailWindow.setVisible(visible)

    def open(self) -> None:
        r"""\brief Open the save/load UI.

        In load-only mode the slot list is activated directly. Otherwise the
        command bar is activated first and the user picks load/save before
        choosing a slot.
        """
        self.setVisible(True)
        self._lastSlotIndex = None
        if self._loadOnly:
            self._mode = "load"
            self._slotWindow.setActive(True)
        else:
            assert self._commandWindow is not None
            self._commandWindow.setActive(True)
            self._slotWindow.setActive(False)
        self.notifySlotIndexMaybeChanged(self._slotWindow.index)

    def close(self) -> None:
        r"""\brief Close the save/load UI and deactivate all child windows."""
        self.setVisible(False)
        if self._commandWindow is not None:
            self._commandWindow.setActive(False)
        self._slotWindow.setActive(False)
        self._detailWindow.setActive(False)

    def closeByCancel(self) -> None:
        r"""\brief Close the save/load UI via cancel and notify the host."""
        Manager.playSE(GameSystem.getCancelSE())
        self._closeWithReason(CLOSE_REASON_CANCEL)

    def cancelSlotSelection(self) -> None:
        r"""\brief Cancel from the slot list.

        In load-only mode this closes the UI; otherwise focus returns to the
        command bar so the user can pick a different mode.
        """
        if self._loadOnly:
            self.closeByCancel()
            return
        self.focusCommand()

    def onCommandConfirm(self, mode: str) -> None:
        r"""\brief Confirm the load/save command and switch focus to the slot list.

        - \param mode The selected mode, either ``"load"`` or ``"save"``.
        """
        Manager.playSE(GameSystem.getDecisionSE())
        self._mode = mode
        self.focusSlotList()

    def focusSlotList(self) -> None:
        r"""\brief Switch focus from the command bar to the slot list."""
        if self._commandWindow is not None:
            self._commandWindow.setActive(False)
        self._slotWindow.setActive(True)
        self._lastSlotIndex = None
        self.notifySlotIndexMaybeChanged(self._slotWindow.index)

    def focusCommand(self) -> None:
        r"""\brief Switch focus from the slot list to the command bar."""
        if self._commandWindow is None:
            return
        self._commandWindow.setActive(True)
        self._slotWindow.setActive(False)

    def notifySlotIndexMaybeChanged(self, index: Optional[int]) -> None:
        r"""\brief Notify the coordinator that the slot list cursor index may have changed.

        - \param index The current zero-based slot index, or ``None`` if no selection.
        """
        if index == self._lastSlotIndex:
            return
        self._lastSlotIndex = index
        self._detailWindow.setSlot(index)

    def _selectLatestSaveSlot(self) -> None:
        latestIndex = _findLatestSaveSlotIndex(MAX_SAVE_SLOTS)
        if latestIndex is not None:
            self._slotWindow.index = latestIndex

    def onSlotConfirm(self, slot: int) -> None:
        r"""\brief Handle slot confirmation for saving or loading.

        - \param slot Zero-based slot index selected by the player.
        """
        slotNumber = slot + 1
        if self._mode == "save":
            self._handleSave(slotNumber)
        else:
            self._handleLoad(slotNumber)

    def _handleSave(self, slotNumber: int) -> None:
        if self._getSaveSource is None:
            Manager.playSE(GameSystem.getBuzzerSE())
            return
        inst = self._getSaveSource()
        if inst is None:
            Manager.playSE(GameSystem.getBuzzerSE())
            return
        filePath = Save.GetSavePath(slotNumber)
        screenImage = GameSystem.getSavedScreenImage()
        if screenImage is not None:
            try:
                encoded = screenImage.saveToMemory("png")
            except Exception:
                encoded = None
            inst.setScreenshot(encoded)
        else:
            inst.setScreenshot(None)
        Save.SaveGame(filePath, inst)
        Manager.playSE(GameSystem.getSaveSE())
        self._detailWindow.refresh()
        self._closeWithReason(CLOSE_REASON_SAVED)

    def _handleLoad(self, slotNumber: int) -> None:
        filePath = Save.GetSavePath(slotNumber)
        if not os.path.exists(filePath):
            Manager.playSE(GameSystem.getBuzzerSE())
            return
        inst = Save.LoadGame(filePath)
        if inst is None:
            Manager.playSE(GameSystem.getBuzzerSE())
            return
        Manager.playSE(GameSystem.getLoadSE())
        self._closeWithReason(CLOSE_REASON_LOADED)
        if self._onLoadedCallback is not None:
            self._onLoadedCallback(inst)

    def _closeWithReason(self, reason: CloseReason) -> None:
        self.close()
        if self._onCloseCallback is not None:
            self._onCloseCallback(reason)
