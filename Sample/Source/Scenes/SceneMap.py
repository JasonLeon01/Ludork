# -*- encoding: utf-8 -*-

import os
import threading
from typing import Callable, List, Union, Optional, Dict, Any, Tuple
from Engine import (
    Pair,
    Vector2u,
    Vector2f,
    Color,
    Input,
    RenderTexture,
    Sprite,
    Text,
    Texture,
    UI,
)
from Engine.UI import RichText, TextStyle
from Engine.UI.Base import Direction
from Engine.Gameplay.Actors import Actor
from Engine.Utils import Inner, Render
from Global import SceneBase, GameMap
from Global import System as GlobalSystem
from Source import System
from Source.Configs.GeneralEnum import Item
from Source.SceneComponents import SceneMapAudioController, SceneMapBuilder
from Source.Windows import (
    PlayerAttrHUD,
    WindowMessage,
    WindowMenu,
    WindowItem,
    WindowEquipSlot,
    WindowEquipSelect,
    WindowEquipStatus,
    WindowSaveLoad,
    WindowShop,
    WindowAttrShop,
    WindowEnemyBook,
    WindowEnemyEncyclopedia,
    WindowFloorTeleporter,
)
from Source.Windows.Base import FocusGroup, FocusNeighbor, FocusTransition
from Source.Windows.WindowFloorTeleporter import GetDefaultFloorTeleporterRects
from Source.GameInstance import GameInstance


_SHOP_WIDTH = 352
_SHOP_COMMAND_HEIGHT = 64
_SHOP_ITEM_SIZE = 352
_ENEMY_BOOK_SIZE = 352
_ENEMY_ENCYCLOPEDIA_WIDTH = 640
_ENEMY_ENCYCLOPEDIA_HEIGHT = 480
_EQUIP_SLOT_WIDTH = 196
_EQUIP_SLOT_HEIGHT = 160
_EQUIP_SELECT_HEIGHT = 192
_EQUIP_STATUS_X = 384
_MAP_TRANSITION_NAME = ""
_MAP_TRANSITION_TIME = 0.5
_ENEMY_BOOK_ITEM_ID = Item.EnemyBook
_FLOOR_TELEPORTER_ITEM_ID = Item.Teleport
_REGION_TITLE_FONT_SIZE = 96
_REGION_TITLE_HOLD_TIME = 1.0
_REGION_TITLE_FADE_TIME = 1.0
_REGION_TITLE_TOTAL_TIME = _REGION_TITLE_HOLD_TIME + _REGION_TITLE_FADE_TIME


class Scene(SceneBase):
    r"""\brief Main gameplay scene handling map rendering, actors, and player interaction."""

    def onEnter(self) -> None:
        r"""\brief Start with a transition effect."""
        GlobalSystem.setTransition()

    def setInst(self, inst: GameInstance) -> None:
        r"""\brief Set the game instance for this scene.

        - \param inst The GameInstance to use.
        """
        self.inst = inst

    def onCreate(self) -> None:
        r"""\brief Create player HUD, message window, menu, and load the starting map."""
        self.setHotKeyFilter("casual")
        self._uiManager.setFocusNavigationEnabled(True)
        self.player = self.inst.getPlayer()
        self._mapBuilder = SceneMapBuilder()
        self._mapAudio = SceneMapAudioController()
        self._playerHUD = PlayerAttrHUD(self.player)
        self._messageWindow = WindowMessage()
        self._windowItem = WindowItem(((192, 0), (256, 256)), self.player)
        self._windowEquipSlot = WindowEquipSlot(((192, 0), (_EQUIP_SLOT_WIDTH, _EQUIP_SLOT_HEIGHT)), self.player)
        self._windowEquipSelect = WindowEquipSelect(
            ((192, _EQUIP_SLOT_HEIGHT), (_EQUIP_SLOT_WIDTH, _EQUIP_SELECT_HEIGHT)),
            self.player,
        )
        self._windowEquipStatus = WindowEquipStatus(
            ((_EQUIP_STATUS_X, 0), (640 - _EQUIP_STATUS_X, _EQUIP_SLOT_HEIGHT + _EQUIP_SELECT_HEIGHT)),
            self.player,
        )
        self._windowEquipSlot.setEquipSelectWindow(self._windowEquipSelect)
        self._windowEquipSlot.setEquipStatusWindow(self._windowEquipStatus)
        self._windowEquipSelect.setEquipSlotWindow(self._windowEquipSlot)
        self._windowEquipSelect.setEquipStatusWindow(self._windowEquipStatus)
        shopCommandRect, shopItemRect = self._getShopRects()
        self._shopMoveEnabledBeforeOpen = True
        self._windowShop = WindowShop(self.player, shopCommandRect, shopItemRect, self._onShopClose)
        self._attrShopMoveEnabledBeforeOpen = True
        self._windowAttrShop = WindowAttrShop(self.player, self._onAttrShopClose)
        self._enemyBookMoveEnabledBeforeOpen = True
        self._windowEnemyBook = WindowEnemyBook(
            self._getEnemyBookRect(),
            self.player,
            self._onEnemyBookClose,
            self._onEnemyBookConfirm,
        )
        self._windowEnemyEncyclopedia = WindowEnemyEncyclopedia(
            self._getEnemyEncyclopediaRect(),
            self._onEnemyEncyclopediaClose,
        )
        self._floorTeleporterMoveEnabledBeforeOpen = True
        floorListRect, floorPreviewRect = GetDefaultFloorTeleporterRects()
        self._windowFloorTeleporter = WindowFloorTeleporter(
            self.inst,
            floorListRect,
            floorPreviewRect,
            self._buildFloorMapPreview,
            self._onFloorTeleporterConfirm,
            self._onFloorTeleporterClose,
            self._getFloorTelepointTag,
        )
        self._windowSaveLoad = WindowSaveLoad(
            getSaveSource=self._getSaveSource,
            onClose=self._onSaveLoadClose,
            onLoaded=self.applyLoadedGame,
        )
        self._windowMenu = WindowMenu(
            self.player,
            self._windowItem,
            self._messageWindow,
            self._windowEquipSlot,
            self._windowEquipSelect,
            self._windowSaveLoad,
        )
        self._windowMenu.setMoveRestoreGuard(self._canRestoreMoveAfterMenuClose)
        self._registerFocusGroups()
        self._regionTitleText = RichText(
            UI.DefaultFont,
            "",
            {
                "default": TextStyle(
                    _REGION_TITLE_FONT_SIZE,
                    Text.Style.Bold,
                    Color(232, 224, 204, 255),
                    Color(0, 0, 0, 170),
                    1.0,
                )
            },
        )
        self._regionTitleText.setVisible(False)
        self._uiManager.loadUI(self._playerHUD)
        self._uiManager.loadUI(self._messageWindow)
        self._uiManager.loadUI(self._windowMenu)
        self._uiManager.loadUI(self._windowItem)
        self._uiManager.loadUI(self._windowEquipSlot)
        self._uiManager.loadUI(self._windowEquipSelect)
        self._uiManager.loadUI(self._windowEquipStatus)
        self._uiManager.loadUI(self._windowShop.getCommandWindow())
        self._uiManager.loadUI(self._windowShop.getItemWindow())
        self._uiManager.loadUI(self._windowAttrShop.getSelectable())
        self._uiManager.loadUI(self._windowEnemyBook)
        self._uiManager.loadUI(self._windowEnemyEncyclopedia)
        self._uiManager.loadUI(self._windowFloorTeleporter.getCommandWindow())
        self._uiManager.loadUI(self._windowFloorTeleporter.getPreviewWindow())
        commandWindow = self._windowSaveLoad.getCommandWindow()
        if commandWindow is not None:
            self._uiManager.loadUI(commandWindow)
        self._uiManager.loadUI(self._windowSaveLoad.getSlotWindow())
        self._uiManager.loadUI(self._windowSaveLoad.getDetailWindow())

        self._windowMenu.close()

        self._gameMap: GameMap = None
        self._cachedMapFile: str = None
        self._currentRegion: Optional[str] = None
        self._regionTitleElapsed: float = _REGION_TITLE_TOTAL_TIME
        self._mapClickMoveBlockedUntilLateTick: bool = False
        self._mapInputBlockFrames: int = 0
        self._pendingMenuOpen: bool = False
        self._pendingFloorTransfer: Optional[Dict[str, Any]] = None
        self._pendingFloorTransferLock = threading.Lock()
        self._mapTransferInProgress: bool = False
        startMap = self.inst._cachedMap or System.getStartMap()
        self.gotoMapAndPos(startMap, blockTransition=True)

    def _registerFocusGroups(self) -> None:
        menuGroup = FocusGroup("menu", [self._windowMenu], self._windowMenu)
        itemGroup = FocusGroup("item", [self._windowItem], self._windowItem)
        itemGroup.setNeighbor(Direction.LEFT, menuGroup)

        equipSlotGroup = FocusGroup("equip-slot", [self._windowEquipSlot], self._windowEquipSlot)
        equipSelectGroup = FocusGroup("equip-select", [self._windowEquipSelect], self._windowEquipSelect)
        equipSlotGroup.setNeighbor(Direction.LEFT, menuGroup)
        equipSlotGroup.setNeighbor(
            Direction.RIGHT,
            FocusNeighbor(equipSelectGroup, FocusTransition.EXPLICIT),
        )
        equipSelectGroup.setNeighbor(
            Direction.LEFT,
            FocusNeighbor(equipSlotGroup, FocusTransition.EXPLICIT),
        )

        shopCommandWindow = self._windowShop.getCommandWindow()
        shopItemWindow = self._windowShop.getItemWindow()
        shopCommandGroup = FocusGroup("shop-command", [shopCommandWindow], shopCommandWindow)
        shopItemGroup = FocusGroup("shop-item", [shopItemWindow], shopItemWindow)
        shopCommandGroup.setNeighbor(
            Direction.DOWN,
            FocusNeighbor(shopItemGroup, FocusTransition.EXPLICIT),
        )
        shopItemGroup.setNeighbor(
            Direction.UP,
            FocusNeighbor(shopCommandGroup, FocusTransition.EXPLICIT),
        )

        floorCommandGroup = FocusGroup(
            "floor-command",
            [self._windowFloorTeleporter.getCommandWindow()],
            self._windowFloorTeleporter.getCommandWindow(),
        )
        floorPreviewGroup = FocusGroup(
            "floor-preview",
            [self._windowFloorTeleporter.getPreviewWindow()],
            self._windowFloorTeleporter.getPreviewWindow(),
        )
        floorCommandGroup.setNeighbor(
            Direction.RIGHT,
            FocusNeighbor(floorPreviewGroup, FocusTransition.EXPLICIT),
        )
        floorPreviewGroup.setNeighbor(
            Direction.LEFT,
            FocusNeighbor(floorCommandGroup, FocusTransition.EXPLICIT),
        )

        saveCommandWindow = self._windowSaveLoad.getCommandWindow()
        if saveCommandWindow is not None:
            saveCommandGroup = FocusGroup(
                "save-command",
                [saveCommandWindow],
                saveCommandWindow,
            )
            saveSlotGroup = FocusGroup(
                "save-slot",
                [self._windowSaveLoad.getSlotWindow()],
                self._windowSaveLoad.getSlotWindow(),
            )
            saveCommandGroup.setNeighbor(
                Direction.DOWN,
                FocusNeighbor(saveSlotGroup, FocusTransition.EXPLICIT),
            )
            saveCommandGroup.setNeighbor(Direction.LEFT, menuGroup)
            saveSlotGroup.setNeighbor(
                Direction.UP,
                FocusNeighbor(saveCommandGroup, FocusTransition.EXPLICIT),
            )
            saveSlotGroup.setNeighbor(Direction.LEFT, menuGroup)
            self._uiManager.registerFocusGroup(saveCommandGroup)
            self._uiManager.registerFocusGroup(saveSlotGroup)

        for group in [
            menuGroup,
            itemGroup,
            equipSlotGroup,
            equipSelectGroup,
            shopCommandGroup,
            shopItemGroup,
            floorCommandGroup,
            floorPreviewGroup,
        ]:
            self._uiManager.registerFocusGroup(group)

    def onQuit(self) -> None:
        r"""\brief Stop map BGM/BGS and weather when leaving this scene."""
        self._mapAudio.stopMapAudio()
        GlobalSystem.clearWeather()
        GlobalSystem.clearFog()

    def onDestroy(self) -> None:
        r"""\brief Ensure map BGM/BGS are stopped when scene is destroyed."""
        self._mapAudio.stopMapAudio()

    def onFixedTick(self, fixedDelta: float) -> None:
        r"""\brief Forward fixed timestep updates to the game map.

        - \param fixedDelta Fixed timestep in seconds.
        """
        if self._mapTransferInProgress:
            return super().onFixedTick(fixedDelta)
        self._gameMap.onFixedTick(fixedDelta)
        return super().onFixedTick(fixedDelta)

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Forward per-frame updates to the game map and handle menu open trigger.

        - \param deltaTime Elapsed time in seconds.
        """
        if self._mapTransferInProgress:
            return super().onTick(deltaTime)
        self._mapClickMoveBlockedUntilLateTick = self._isMapClickMoveBlocked()
        self._gameMap.onTick(deltaTime)
        self._gameMap.getTilemap().updateAutoTileAnimation(deltaTime)
        self._updateRegionTitle(deltaTime)
        if self._canOpenMenu() and self._isMenuOpenTriggered():
            self.openMenu()
        return super().onTick(deltaTime)

    def onLateTick(self, deltaTime: float) -> None:
        r"""\brief Forward late-update to the game map.

        - \param deltaTime Elapsed time in seconds.
        """
        if self._mapTransferInProgress:
            return super().onLateTick(deltaTime)
        if self._tryConfirmMessageByScreenClick():
            self._mapClickMoveBlockedUntilLateTick = False
        elif self._mapClickMoveBlockedUntilLateTick or self._isMapClickMoveBlocked():
            self._consumeMapClickMoveInput()
            self._mapClickMoveBlockedUntilLateTick = False
        if self._mapInputBlockFrames > 0:
            self._consumeMapClickMoveInput()
            self._mapInputBlockFrames -= 1
        self._gameMap.onLateTick(deltaTime)
        return super().onLateTick(deltaTime)

    def loadMap(self, mapPath: str) -> str:
        r"""\brief Load and generate a game map from data.

        - \param mapPath Path to the map data file.
        - \return Resolved map data file path.
        """
        mapFile, mapData = self._mapBuilder.loadMapData(mapPath, self._getCurrentRegionMap())
        self._gameMap = self._mapBuilder.generateGameMap(mapData)
        self._gameMap.setScene(self)
        self._gameMap.setPersistentMapPath(mapFile)
        self._gameMap.applyTerrainDestructions(self.inst.getTerrainDestructions(mapFile))
        self._gameMap.applyAddedActors(self.inst.getAddedActors(mapFile))
        self._gameMap.applyActorPositions(self.inst.getActorPositions(mapFile))
        destroyedActors = self.inst.getDestroyedActors(mapFile)
        self._gameMap.removeActorsByTags(destroyedActors)
        self._gameMap.spawnActor(self.player, "default")
        self._gameMap.setPlayer(self.player)
        self._mapAudio.playMapAudio(mapData)
        GlobalSystem.clearFog()
        GlobalSystem.applyFogFromMapData(mapData)
        self._updateCurrentRegion(mapFile)
        return mapFile

    @ReturnType(gameMap=GameMap)
    def getGameMap(self) -> GameMap:
        return self._gameMap

    @Latent(FinishedDialogue=(True,))
    def showMessage(self, name: str, message: str, refActor: Optional[Actor] = None) -> Callable[[], bool]:
        r"""\brief Show a dialogue message window.

        - \param name Speaker name.
        - \param message Message text.
        - \param refActor Optional reference actor for positioning.
        - \return A callable condition function that returns True when dialogue finishes.
        """
        refPosition: Optional[Vector2f] = None
        if refActor is not None:
            camera = self._gameMap.getCamera()
            assert camera
            refPosition = refActor.getPosition() - camera.getViewPosition() + self._gameMap.getMapViewOffset()
        originMoveEnabled = self.player.getMoveEnabled()
        restored = False

        def restoreMove() -> None:
            nonlocal restored
            if restored:
                return
            restored = True
            self.player.setMoveEnabled(originMoveEnabled)
            self._blockMapInput(2)

        self.player.setMoveEnabled(False)
        localVars = self._getDialogueLocalVars(type(self).showMessage)
        self._messageWindow.setMessage(
            refPosition,
            self._formatDialogueText(name, localVars),
            self._formatDialogueText(message, localVars),
            onFinished=restoreMove,
        )

        def condition() -> bool:
            if self._messageWindow.isInDialogue():
                return False
            restoreMove()
            return True

        return condition

    @Latent(Selected0=(0,), Selected1=(1,), Selected2=(2,), Selected3=(3,), Cancelled=(-1,))
    def showSelection(
        self, name: str, options: List[str], refActor: Optional[Actor] = None, allowCancel: bool = True
    ) -> Callable[[], Optional[int]]:
        r"""\brief Show a selection window with multiple options.

        - \param name Window title.
        - \param options List of option strings.
        - \param refActor Optional reference actor for positioning.
        - \param allowCancel Whether the player can cancel.
        - \return A callable that returns the selected option index, or -1 for cancel.
        """
        refPosition: Optional[Vector2f] = None
        if refActor is not None:
            camera = self._gameMap.getCamera()
            assert camera
            refPosition = refActor.getPosition() - camera.getViewPosition() + self._gameMap.getMapViewOffset()

        originMoveEnabled = self.player.getMoveEnabled()
        restored = False

        def restoreMove() -> None:
            nonlocal restored
            if restored:
                return
            restored = True
            self.player.setMoveEnabled(originMoveEnabled)
            self._blockMapInput(2)

        self.player.setMoveEnabled(False)
        localVars = self._getDialogueLocalVars(type(self).showSelection)
        self._messageWindow.setMessage(
            refPosition,
            self._formatDialogueText(name, localVars),
            [self._formatDialogueText(option, localVars) for option in options],
            allowCancel=allowCancel,
            onFinished=restoreMove,
        )

        def condition() -> Optional[int]:
            selectionResult = self._messageWindow.getSelectionResult()
            if selectionResult is None:
                return None
            restoreMove()
            return selectionResult

        return condition

    @ExecSplit(default=(None,))
    @TypeAdapter(pos=([tuple, list], Vector2u))
    def applyLoadedGame(self, inst: GameInstance) -> None:
        r"""\brief Apply a loaded game instance and force-reload the cached map.

        - \param inst The restored game instance from a save file.
        """
        self.inst = inst
        self.player = inst.getPlayer()
        self._rebindPlayerToUI()
        mapPath = inst._cachedMap or System.getStartMap()
        pos = self.player.getMapPosition().unpack()
        self._cachedMapFile = None
        self._currentRegion = None
        self.gotoMapAndPos(mapPath, pos)

    def _rebindPlayerToUI(self) -> None:
        r"""\brief Point HUD and sub-windows at the current player after load."""
        self._windowItem._player = self.player
        self._windowEquipSlot._player = self.player
        self._windowEquipSelect._player = self.player
        self._windowEquipStatus.setPlayer(self.player)
        self._windowMenu._player = self.player
        self._windowShop.setPlayer(self.player)
        self._windowAttrShop.setPlayer(self.player)
        self._windowEnemyBook.setPlayer(self.player)
        self._playerHUD._player = self.player

    @ExecSplit(default=(None,))
    def showEnemyBook(self) -> None:
        r"""\brief Show the current-map monster handbook."""
        if not self._canOpenMenu():
            return
        if not self.player.hasItem(_ENEMY_BOOK_ITEM_ID):
            return
        if not self._windowEnemyBook.getVisible():
            self._enemyBookMoveEnabledBeforeOpen = (
                True if self._windowMenu.isBlocking() else self.player.getMoveEnabled()
            )
            self.player.setMoveEnabled(False)
        self._windowEnemyBook.open(self._gameMap)
        self._blockMapInput(2)

    @ExecSplit(default=(None,))
    def showFloorTeleporter(self) -> None:
        r"""\brief Show the visited-floor teleporter preview window."""
        if not self._canOpenMenu():
            return
        if not self.player.hasItem(_FLOOR_TELEPORTER_ITEM_ID):
            return
        if not self._canUseFloorTeleporterByAsideConstraint():
            return
        if not self._windowFloorTeleporter.getVisible():
            self._floorTeleporterMoveEnabledBeforeOpen = (
                True if self._windowMenu.isBlocking() else self.player.getMoveEnabled()
            )
            self.player.setMoveEnabled(False)
        self._recordCurrentFloorTelepoint()
        self._windowFloorTeleporter.open(self.inst)
        self._blockMapInput(2)

    def _canUseFloorTeleporterByAsideConstraint(self) -> bool:
        from Engine.Utils.DataValue import evalDataExpression
        from Source import Data
        from Source.Teleporter import Teleporter

        itemData = Data.getGeneralItemData(_FLOOR_TELEPORTER_ITEM_ID)
        kwargs = itemData.get("kwargs") or {}
        if not isinstance(kwargs, dict):
            return True
        flag = kwargs.get("CanOnlyUseAsideTeleporter", False)
        if isinstance(flag, str):
            flag = evalDataExpression(flag)
        if not flag:
            return True
        player = self._gameMap.getPlayer()
        if player is None:
            return False
        return Teleporter.isAsideOrOverlapping(self._gameMap.getAllActors(), player.getMapPosition())

    @ExecSplit(default=(None,))
    def openMenu(self) -> None:
        r"""\brief Request the in-game menu to open on the next render pass."""
        if not self._canOpenMenu():
            return
        self._pendingMenuOpen = True

    def openShop(self, buyItemIDs: List[str], canSell: bool) -> Callable[[], bool]:
        r"""\brief Open the map-bound shop and wait until it closes.

        - \param buyItemIDs Item IDs available for purchase.
        - \param canSell Whether selling is available.
        - \return A condition callable that becomes True when the shop closes.
        """
        self._shopMoveEnabledBeforeOpen = True if self._windowMenu.isBlocking() else self.player.getMoveEnabled()
        self.player.setMoveEnabled(False)
        self._windowShop.open(buyItemIDs, canSell)

        def condition() -> bool:
            return not self._windowShop.getVisible()

        return condition

    def openAttrShop(
        self,
        actor: Actor,
        shopName: str,
        shopDescription: str,
        abilities: Dict[str, int],
        priceRef: Union[int, List[int]],
        priceIncrement: int,
        moneyName: str,
    ) -> Callable[[], bool]:
        r"""\brief Open the map-bound attribute shop and wait until it closes.

        - \param actor The actor whose avatar is shown.
        - \param shopName Locale key for the shop name.
        - \param shopDescription Locale key for the shop description.
        - \param abilities Mapping of player attribute names to purchased increments.
        - \param priceRef Mutable reference containing a shared price or ordered price list.
        - \param priceIncrement Amount added to the price after each purchase.
        - \param moneyName Player info component attribute used as currency.
        - \return A condition callable that becomes True when the shop closes.
        """
        self._attrShopMoveEnabledBeforeOpen = True if self._windowMenu.isBlocking() else self.player.getMoveEnabled()
        self.player.setMoveEnabled(False)
        self._windowAttrShop.open(
            actor,
            shopName,
            shopDescription,
            abilities,
            priceRef,
            priceIncrement,
            moneyName,
            self._getAttrShopRect(),
        )

        def condition() -> bool:
            return not self._windowAttrShop.getVisible()

        return condition

    def _onShopClose(self) -> None:
        self.player.setMoveEnabled(self._shopMoveEnabledBeforeOpen)

    def _onAttrShopClose(self) -> None:
        self.player.setMoveEnabled(self._attrShopMoveEnabledBeforeOpen)
        self._blockMapInput(1)

    def _onEnemyBookClose(self) -> None:
        self.player.setMoveEnabled(self._enemyBookMoveEnabledBeforeOpen)
        self._blockMapInput(1)

    def _onEnemyBookConfirm(self, entry: Dict[str, Any]) -> None:
        self._windowEnemyEncyclopedia.open(entry)
        self._blockMapInput(2)

    def _onEnemyEncyclopediaClose(self) -> None:
        self.player.setMoveEnabled(self._enemyBookMoveEnabledBeforeOpen)
        self._blockMapInput(1)

    def _onFloorTeleporterClose(self) -> None:
        self.player.setMoveEnabled(self._floorTeleporterMoveEnabledBeforeOpen)
        self._blockMapInput(1)

    def _onFloorTeleporterConfirm(self, mapKey: str, telepoint: Tuple[int, int]) -> None:
        targetMap = self.resolveRegionMapPath(mapKey)
        self._windowFloorTeleporter.close()
        self.gotoMapAndPos(targetMap, Vector2u(*telepoint))
        self.player.setMoveEnabled(self._floorTeleporterMoveEnabledBeforeOpen)
        self._blockMapInput(2)

    def _recordCurrentFloorTelepoint(self) -> None:
        if not self._gameMap or not self._cachedMapFile:
            return
        telepoint = self._findNearestFloorTelepoint()
        if telepoint is None:
            return
        self.inst.recordTelepoint(self._cachedMapFile, Vector2u(telepoint[0], telepoint[1]))

    def _findNearestFloorTelepoint(self) -> Optional[Tuple[int, int]]:
        from Source.Teleporter import Teleporter

        player = self._gameMap.getPlayer()
        if player is None:
            return None
        nearest = Teleporter._findNearestTeleporter(self._gameMap.getAllActors(), player.getMapPosition())
        if nearest is None:
            return None
        return nearest.getTeleportPosition()

    def _getShopRects(self):
        gameSize = GlobalSystem.getGameSize()
        totalHeight = _SHOP_COMMAND_HEIGHT + _SHOP_ITEM_SIZE
        x = int((gameSize.x - _SHOP_WIDTH) / 2)
        y = int((gameSize.y - totalHeight) / 2)
        return (
            ((x, y), (_SHOP_WIDTH, _SHOP_COMMAND_HEIGHT)),
            ((x, y + _SHOP_COMMAND_HEIGHT), (_SHOP_WIDTH, _SHOP_ITEM_SIZE)),
        )

    def _getAttrShopRect(self):
        gameSize = GlobalSystem.getGameSize()
        size = WindowAttrShop._SIZE
        x = int((gameSize.x - size) / 2)
        y = int((gameSize.y - size) / 2)
        return ((x, y), (size, size))

    def _getDialogueLocalVars(self, nodeFunction) -> Dict[str, Any]:
        refLocal = getattr(nodeFunction, "_refLocal", {})
        if not isinstance(refLocal, dict):
            return {}
        if refLocal.get("__activeNodeFunction__") is not nodeFunction:
            return {}
        return {key: value for key, value in refLocal.items() if isinstance(key, str) and not key.startswith("__")}

    def _formatDialogueText(self, text: str, localVars: Dict[str, Any]) -> str:
        if not isinstance(text, str):
            return str(text)
        text = Inner.ApplyStringLocaleFormat(text)
        text = Inner.ApplyStringMappingFormat(text, localVars)
        inst = getattr(self, "inst", None)
        if inst is not None:
            text = Inner.ApplyStringMappingFormat(text, inst.getVariables())
        return text

    def _getEnemyBookRect(self):
        gameSize = GlobalSystem.getGameSize()
        x = int((gameSize.x - _ENEMY_BOOK_SIZE) / 2)
        y = int((gameSize.y - _ENEMY_BOOK_SIZE) / 2)
        return ((x, y), (_ENEMY_BOOK_SIZE, _ENEMY_BOOK_SIZE))

    def _getEnemyEncyclopediaRect(self):
        gameSize = GlobalSystem.getGameSize()
        x = int((gameSize.x - _ENEMY_ENCYCLOPEDIA_WIDTH) / 2)
        y = int((gameSize.y - _ENEMY_ENCYCLOPEDIA_HEIGHT) / 2)
        return ((x, y), (_ENEMY_ENCYCLOPEDIA_WIDTH, _ENEMY_ENCYCLOPEDIA_HEIGHT))

    def _canRestoreMoveAfterMenuClose(self) -> bool:
        return not (
            self._windowShop.getVisible()
            or self._windowAttrShop.getVisible()
            or self._windowEnemyBook.getVisible()
            or self._windowEnemyEncyclopedia.getVisible()
            or self._windowFloorTeleporter.getVisible()
        )

    def _blockMapInput(self, frames: int = 1) -> None:
        self._mapInputBlockFrames = max(self._mapInputBlockFrames, frames)

    def requestFloorTransfer(self, targetMap: str, anchorPos: Tuple[int, int], moveEnabled: bool) -> bool:
        with self._pendingFloorTransferLock:
            if self._pendingFloorTransfer is not None:
                return False
            self._pendingFloorTransfer = {
                "targetMap": targetMap,
                "anchorPos": anchorPos,
                "moveEnabled": moveEnabled,
            }
        GlobalSystem.freezeTransitionBackground()
        return True

    def _processPendingFloorTransfer(self) -> None:
        with self._pendingFloorTransferLock:
            if self._pendingFloorTransfer is None:
                return
            if not GlobalSystem.isTransitionBackgroundFrozen():
                return
            transferData = self._pendingFloorTransfer
            self._pendingFloorTransfer = None

        from Source.Teleporter import Teleporter

        self._mapTransferInProgress = True
        try:
            targetMap = transferData["targetMap"]
            anchorPos = transferData["anchorPos"]
            moveEnabled = bool(transferData["moveEnabled"])
            self.gotoMapAndPos(targetMap, anchorPos, True)
            targetGameMap = self.getGameMap()
            targetPlayer = targetGameMap.getPlayer()
            if targetPlayer is None:
                self._cancelFloorTransfer(moveEnabled)
                return
            targetTeleporter = Teleporter._findNearestTeleporter(
                targetGameMap.getAllActors(),
                targetPlayer.getMapPosition(),
            )
            if targetTeleporter is None:
                self._cancelFloorTransfer(moveEnabled)
                return
            targetPos = targetTeleporter.getTeleportPosition()
            self.gotoMapAndPos(targetMap, targetPos)
            if self._cachedMapFile:
                self.inst.recordTelepoint(self._cachedMapFile, Vector2u(targetPos[0], targetPos[1]))
            targetPlayer.setMoveEnabled(moveEnabled)
        finally:
            self._mapTransferInProgress = False

    def _cancelFloorTransfer(self, moveEnabled: bool) -> None:
        self.player.setMoveEnabled(moveEnabled)
        GlobalSystem.cancelTransitionBackgroundFreeze()
        GlobalSystem.cancelPendingTransition()

    def _getSaveSource(self) -> GameInstance:
        r"""\brief Provide the GameInstance to persist when saving from this scene.

        - \return The current scene GameInstance.
        """
        return self.inst

    def _onSaveLoadClose(self, reason: str) -> None:
        r"""\brief React to the save/load UI closing.

        - \param reason One of ``"cancel"``, ``"saved"``, or ``"loaded"``.
        """
        if reason == "cancel":
            self._windowMenu._onSaveLoadClose()
            return
        self._windowMenu.close()

    def gotoMapAndPos(
        self,
        mapPath: str,
        pos: Optional[Union[Vector2u, Pair[int], List[int]]] = None,
        blockTransition: bool = False,
    ) -> None:
        r"""\brief Transition to a map and set the player position.

        - \param mapPath Path to the map data file.
        - \param pos The position to place the player.
        - \param blockTransition Whether to skip the map transition effect.
        """
        targetMap = self._mapBuilder.resolveMapPath(mapPath, self._getCurrentRegionMap()) if mapPath else mapPath
        if targetMap and self._cachedMapFile != targetMap:
            targetMap = self.loadMap(targetMap)
            self._cachedMapFile = targetMap
        self.inst.applyMapInfo(targetMap, pos)
        if not blockTransition:
            GlobalSystem.requestTransition(_MAP_TRANSITION_NAME, _MAP_TRANSITION_TIME)

    @ExecSplit(default=(None,))
    def recordAddedActor(self, actor: Actor) -> None:
        r"""\brief Record an added actor for persistence.

        - \param actor The added actor.
        """
        layerName = self._gameMap.getActorLayer(actor)
        if layerName is None:
            return
        self.inst.recordAddedActor(self._cachedMapFile, actor, layerName)

    @ExecSplit(default=(None,))
    def recordActorPosition(self, actor: Actor) -> None:
        r"""\brief Record an actor position change for persistence.

        - \param actor The moved actor.
        """
        self.inst.recordActorPosition(self._cachedMapFile, actor)

    @ExecSplit(default=(None,))
    def recordDestroyedActor(self, actor: Actor) -> None:
        r"""\brief Record a destroyed actor for persistence.

        - \param actor The destroyed actor.
        """
        self.inst.recordDestroyedActor(self._cachedMapFile, actor)

    def playBgm(self, bgm: str, bgmFilter: Any = None) -> None:
        r"""\brief Replace the current map BGM.

        - \param bgm Music filename under Assets/Musics.
        - \param bgmFilter Optional music filter to apply.
        """
        self._mapAudio.playBgm(bgm, bgmFilter)

    def setBgmFilter(self, attr: str, value: Any) -> None:
        r"""\brief Set a filter attribute on the current BGM music.

        - \param attr The filter attribute name.
        - \param value The filter attribute value.
        """
        self._mapAudio.setBgmFilter(attr, value)

    def setBgsFilter(self, attr: str, value: Any) -> None:
        r"""\brief Set a filter attribute on the current BGS music.

        - \param attr The filter attribute name.
        - \param value The filter attribute value.
        """
        self._mapAudio.setBgsFilter(attr, value)

    def _drawSceneAnims(self) -> None:
        r"""\brief Draw map animations in screen space aligned with the camera view."""
        animSnapshot = self.getAnims()
        if not animSnapshot:
            return
        GlobalSystem.setWindowMapView(self._gameMap.getMapViewOffset())
        for anim in animSnapshot:
            worldPosition = anim.getPosition()
            drawPosition = self._gameMap.worldToMapViewPosition(worldPosition)
            anim.setPosition(drawPosition)
            GlobalSystem.draw(anim)
            anim.setPosition(worldPosition)
        GlobalSystem.setWindowDefaultView()

    def _drawCommonTipOverlay(self) -> None:
        super()._drawCommonTipOverlay()
        if self._regionTitleText.getVisible():
            GlobalSystem.draw(self._regionTitleText)

    def _renderHandle(self, deltaTime: float) -> None:
        self._gameMap.show()
        super()._renderHandle(deltaTime)
        self._processPendingFloorTransfer()
        if self._pendingMenuOpen:
            self._pendingMenuOpen = False
            self._captureScreenSnapshot()
            self._windowMenu.open()

    def _captureScreenSnapshot(self) -> None:
        canvas = GlobalSystem.getCanvas()
        sourceTex = canvas.getTexture()
        srcSize = sourceTex.getSize()
        gameSize = GlobalSystem.getGameSize()
        if srcSize.x == 0 or srcSize.y == 0:
            System.setSavedScreenImage(None)
            return
        if srcSize.x == gameSize.x and srcSize.y == gameSize.y:
            System.setSavedScreenImage(sourceTex.copyToImage())
            return
        scaledRT = RenderTexture(gameSize)
        scaledRT.clear(Color.Transparent)
        sprite = Sprite(sourceTex)
        sprite.setScale(Vector2f(gameSize.x / srcSize.x, gameSize.y / srcSize.y))
        scaledRT.draw(sprite)
        scaledRT.display()
        System.setSavedScreenImage(scaledRT.getTexture().copyToImage())

    def _isMenuOpenTriggered(self) -> bool:
        return Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True)

    def _canOpenMenu(self) -> bool:
        return (
            not self._pendingMenuOpen
            and not self._windowMenu.isBlocking()
            and not self._messageWindow.isInDialogue()
            and not self._windowShop.getVisible()
            and not self._windowAttrShop.getVisible()
            and not self._windowEnemyBook.getVisible()
            and not self._windowEnemyEncyclopedia.getVisible()
            and not self._windowFloorTeleporter.getVisible()
        )

    def _isMapClickMoveBlocked(self) -> bool:
        return (
            self._messageWindow.isInDialogue()
            or self._windowMenu.isBlocking()
            or self._windowShop.getVisible()
            or self._windowAttrShop.getVisible()
            or self._windowEnemyBook.getVisible()
            or self._windowEnemyEncyclopedia.getVisible()
            or self._windowFloorTeleporter.getVisible()
            or self._mapInputBlockFrames > 0
        )

    def _consumeMapClickMoveInput(self) -> None:
        Input.isMouseButtonTriggered(Input.Mouse.Button.Left, handled=True)
        Input.isTouchBegan(handled=True)
        Input.isTouchTriggered(handled=True)

    def _tryConfirmMessageByScreenClick(self) -> bool:
        if not self._messageWindow.isAwaitingMessageConfirm():
            return False
        clicked = Input.isMouseButtonTriggered(Input.Mouse.Button.Left, handled=False)
        touched = Input.isTouchBegan(handled=False)
        if not clicked and not touched:
            return False
        self._messageWindow.confirmMessage()
        self._consumeMapClickMoveInput()
        return True

    def _buildFloorMapPreview(
        self,
        mapKey: str,
        telepoint: Tuple[int, int],
        previewSize: int,
        previewScale: float,
        showTelepointMarker: bool = False,
    ) -> Optional[Texture]:
        return self._mapBuilder.buildFloorMapPreview(
            self.inst,
            self._getCurrentRegionMap(),
            mapKey,
            telepoint,
            previewSize,
            previewScale,
            showTelepointMarker,
        )

    def _getFloorTelepointTag(self, mapKey: str, telepoint: Tuple[int, int]) -> Optional[str]:
        return self._mapBuilder.getFloorTelepointTag(self._getCurrentRegionMap(), mapKey, telepoint)

    def resolveRegionMapPath(self, mapKey: str) -> str:
        return self._mapBuilder.resolveRegionMapPath(mapKey, self._getCurrentRegionMap())

    def _getCurrentRegionMap(self) -> str:
        return self._cachedMapFile or self.inst._cachedMap or System.getStartMap()

    def _updateCurrentRegion(self, mapFile: str) -> None:
        region = self._findRegionForMap(mapFile)
        if region == self._currentRegion:
            return
        self._currentRegion = region
        if region is None:
            return
        self.inst.setCurrentRegion(region)
        self._showRegionTitle(region)

    def _findRegionForMap(self, mapFile: str) -> Optional[str]:
        from Source.Config.RegionDict import RegionDict

        currentName = self._normaliseRegionMapName(mapFile)
        currentBaseName = os.path.basename(currentName)
        for region, regionMaps in RegionDict.items():
            for regionMap in regionMaps:
                regionMapName = self._normaliseRegionMapName(regionMap)
                if regionMapName == currentName:
                    return region
                if "/" not in regionMapName and regionMapName == currentBaseName:
                    return region
        return None

    @staticmethod
    def _normaliseRegionMapName(mapPath: str) -> str:
        path = str(mapPath).replace("\\", "/")
        while path.startswith("./"):
            path = path[2:]
        marker = "Data/Maps/"
        markerIndex = path.find(marker)
        if markerIndex != -1:
            path = path[markerIndex + len(marker) :]
        path = os.path.splitext(path)[0]
        return path

    def _showRegionTitle(self, region: str) -> None:
        self._regionTitleText.setString(LOC(region))
        self._layoutRegionTitle()
        self._regionTitleElapsed = 0.0
        self._regionTitleText.setColour(Color(255, 255, 255, 255))
        self._regionTitleText.setVisible(True)

    def _layoutRegionTitle(self) -> None:
        bounds = self._regionTitleText.getLocalBounds()
        origin = Vector2f(bounds.position.x + bounds.size.x / 2.0, bounds.position.y + bounds.size.y / 2.0)
        self._regionTitleText.setOrigin(origin)
        gameSize = GlobalSystem.getGameSize()
        self._regionTitleText.setPosition(Vector2f(float(gameSize.x) / 2.0, float(gameSize.y) / 2.0))

    def _updateRegionTitle(self, deltaTime: float) -> None:
        if not self._regionTitleText.getVisible():
            return
        if GlobalSystem.isTransitionPending() or GlobalSystem.isInTransition():
            return
        self._regionTitleElapsed += deltaTime
        if self._regionTitleElapsed <= _REGION_TITLE_HOLD_TIME:
            return
        fadeElapsed = self._regionTitleElapsed - _REGION_TITLE_HOLD_TIME
        if fadeElapsed >= _REGION_TITLE_FADE_TIME:
            self._regionTitleText.setVisible(False)
            return
        alpha = int(255 * (1.0 - fadeElapsed / _REGION_TITLE_FADE_TIME))
        self._regionTitleText.setColour(Color(255, 255, 255, alpha))
