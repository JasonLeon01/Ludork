# -*- encoding: utf-8 -*-

import os
from typing import Callable, List, Union, Optional, Dict, Any
from Engine import Pair, Vector2u, Vector2f, Color, Filters, Music, Input, RenderTexture, Sprite
import Engine
from Engine.Gameplay import Tilemap, TileLayer, TileLayerData
from Engine.Gameplay.Actors import Actor
from Engine.Utils import File
from Global import Manager, SceneBase, GameMap, Camera, Light
from Global import System as GlobalSystem
from Source import Data, System
from Source.Windows.HUDPlayerAttr import PlayerAttrHUD
from Source.Windows.WindowMessage import WindowMessage
from Source.Windows.WindowMenu import WindowMenu
from Source.Windows.WindowItem import WindowItem
from Source.Windows.WindowEquip import WindowEquipSlot, WindowEquipSelect
from Source.Windows.WindowSaveLoad import WindowSaveLoad
from Source.Windows.WindowShop import WindowShop
from Source.Windows.WindowEnemyBook import WindowEnemyBook
from Source.GameInstance import GameInstance


_SHOP_WIDTH = 352
_SHOP_COMMAND_HEIGHT = 64
_SHOP_ITEM_SIZE = 352
_ENEMY_BOOK_SIZE = 352


class Scene(SceneBase):
    r"""\brief Main gameplay scene handling map rendering, actors, and player interaction."""

    def onEnter(self) -> None:
        r"""\brief Start with a transition effect."""
        GlobalSystem.setTransition(Manager.loadTransition("012-Random04.png"), 0.5)

    def setInst(self, inst: GameInstance) -> None:
        r"""\brief Set the game instance for this scene.

        - \param inst The GameInstance to use.
        """
        self.inst = inst

    def onCreate(self) -> None:
        r"""\brief Create player HUD, message window, menu, and load the starting map."""
        self.player = self.inst.getPlayer()
        self._playerHUD = PlayerAttrHUD(self.player)
        self._messageWindow = WindowMessage()
        self._windowItem = WindowItem(((192, 0), (256, 256)), self.player)
        self._windowEquipSlot = WindowEquipSlot(((192, 0), (196, 256)), self.player)
        self._windowEquipSelect = WindowEquipSelect(((384, 0), (256, 256)), self.player)
        self._windowEquipSlot.setEquipSelectWindow(self._windowEquipSelect)
        self._windowEquipSelect.setEquipSlotWindow(self._windowEquipSlot)
        shopCommandRect, shopItemRect = self._getShopRects()
        self._shopMoveEnabledBeforeOpen = True
        self._windowShop = WindowShop(self.player, shopCommandRect, shopItemRect, self._onShopClose)
        self._enemyBookMoveEnabledBeforeOpen = True
        self._windowEnemyBook = WindowEnemyBook(self._getEnemyBookRect(), self.player, self._onEnemyBookClose)
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
        self._uiManager.loadUI(self._playerHUD)
        self._uiManager.loadUI(self._messageWindow)
        self._uiManager.loadUI(self._windowMenu)
        self._uiManager.loadUI(self._windowItem)
        self._uiManager.loadUI(self._windowEquipSlot)
        self._uiManager.loadUI(self._windowEquipSelect)
        self._uiManager.loadUI(self._windowShop.getCommandWindow())
        self._uiManager.loadUI(self._windowShop.getItemWindow())
        self._uiManager.loadUI(self._windowEnemyBook)
        commandWindow = self._windowSaveLoad.getCommandWindow()
        if commandWindow is not None:
            self._uiManager.loadUI(commandWindow)
        self._uiManager.loadUI(self._windowSaveLoad.getSlotWindow())
        self._uiManager.loadUI(self._windowSaveLoad.getDetailWindow())

        self._windowMenu.close()

        self._gameMap: GameMap = None
        self._cachedMapFile: str = None
        self._currentBgmMusic: Optional[Music] = None
        self._currentBgmFile: str = ""
        self._currentBgsMusic: Optional[Music] = None
        self._currentBgsFile: str = ""
        self._mapClickMoveBlockedUntilLateTick: bool = False
        self._mapInputBlockFrames: int = 0
        self._pendingMenuOpen: bool = False
        startMap = self.inst._cachedMap or System.getStartMap()
        self.gotoMapAndPos(startMap)

    def getGameMap(self) -> GameMap:
        return self._gameMap

    def onQuit(self) -> None:
        r"""\brief Stop map BGM/BGS and weather when leaving this scene."""
        self._stopMapAudio()
        GlobalSystem.clearWeather()
        GlobalSystem.clearFog()

    def onDestroy(self) -> None:
        r"""\brief Ensure map BGM/BGS are stopped when scene is destroyed."""
        self._stopMapAudio()

    def onFixedTick(self, fixedDelta: float) -> None:
        r"""\brief Forward fixed timestep updates to the game map.

        - \param fixedDelta Fixed timestep in seconds.
        """
        self._gameMap.onFixedTick(fixedDelta)
        return super().onFixedTick(fixedDelta)

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Forward per-frame updates to the game map and handle menu open trigger.

        - \param deltaTime Elapsed time in seconds.
        """
        self._mapClickMoveBlockedUntilLateTick = self._isMapClickMoveBlocked()
        self._gameMap.onTick(deltaTime)
        self._gameMap.getTilemap().updateAutoTileAnimation(deltaTime)
        if (
            not self._windowMenu.isBlocking()
            and not self._messageWindow.isInDialogue()
            and not self._windowShop.getVisible()
            and not self._windowEnemyBook.getVisible()
        ):
            if not self._pendingMenuOpen and self._isMenuOpenTriggered():
                self._pendingMenuOpen = True
        return super().onTick(deltaTime)

    def onLateTick(self, deltaTime: float) -> None:
        r"""\brief Forward late-update to the game map.

        - \param deltaTime Elapsed time in seconds.
        """
        if self._mapClickMoveBlockedUntilLateTick or self._isMapClickMoveBlocked():
            self._consumeMapClickMoveInput()
            self._mapClickMoveBlockedUntilLateTick = False
        if self._mapInputBlockFrames > 0:
            self._consumeMapClickMoveInput()
            self._mapInputBlockFrames -= 1
        self._gameMap.onLateTick(deltaTime)
        return super().onLateTick(deltaTime)

    def loadMap(self, mapPath: str) -> None:
        r"""\brief Load and generate a game map from data.

        - \param mapPath Path to the map data file.
        """
        mapPath = os.path.join("./Data/Maps", mapPath)
        mapData = File.loadData(mapPath)
        self._gameMap = self._generateGameMap(mapData)
        self._gameMap.setScene(self)
        self._gameMap.spawnActor(self.player, "default")
        self._gameMap.setPlayer(self.player)
        self._playMapAudio(mapData)
        GlobalSystem.clearFog()
        GlobalSystem.applyFogFromMapData(mapData)
        destroyedActors = self.inst.getDestroyedActors(mapPath)
        if destroyedActors:
            for actorTag in destroyedActors:
                actorList = self._gameMap.getAllActorsByTag(actorTag)
                if actorList:
                    for actor in actorList:
                        actor.destroy()

    @Latent(FinishedDialogue=(True,))
    def showMessage(self, refActorTag: str, name: str, message: str) -> Callable[[], bool]:
        r"""\brief Show a dialogue message window.

        - \param refActorTag Tag of the reference actor for positioning.
        - \param name Speaker name.
        - \param message Message text.
        - \return A callable condition function that returns True when dialogue finishes.
        """
        refPosition: Optional[Vector2f] = None
        if bool(refActorTag):
            actors = self._gameMap.getAllActorsByTag(refActorTag)
            if actors:
                actor = actors[0]
                camera = self._gameMap.getCamera()
                assert camera
                refPosition = actor.getPosition() - camera.getViewPosition()
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
        self._messageWindow.setMessage(
            refPosition,
            name,
            message,
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
        self, refActorTag: str, name: str, options: List[str], allowCancel: bool
    ) -> Callable[[], Optional[int]]:
        r"""\brief Show a selection window with multiple options.

        - \param refActorTag Tag of the reference actor for positioning.
        - \param name Window title.
        - \param options List of option strings.
        - \param allowCancel Whether the player can cancel.
        - \return A callable that returns the selected option index, or -1 for cancel.
        """
        refPosition: Optional[Vector2f] = None
        if bool(refActorTag):
            actors = self._gameMap.getAllActorsByTag(refActorTag)
            if actors:
                actor = actors[0]
                camera = self._gameMap.getCamera()
                assert camera
                refPosition = actor.getPosition() - camera.getViewPosition()

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
        self._messageWindow.setMessage(
            refPosition,
            name,
            options,
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
        self.gotoMapAndPos(mapPath, pos)

    def _rebindPlayerToUI(self) -> None:
        r"""\brief Point HUD and sub-windows at the current player after load."""
        self._windowItem._player = self.player
        self._windowEquipSlot._player = self.player
        self._windowEquipSelect._player = self.player
        self._windowMenu._player = self.player
        self._windowShop.setPlayer(self.player)
        self._windowEnemyBook.setPlayer(self.player)
        self._playerHUD._player = self.player

    @ExecSplit(default=(None,))
    def showEnemyBook(self) -> None:
        r"""\brief Show the current-map monster handbook."""
        if not self._windowEnemyBook.getVisible():
            self._enemyBookMoveEnabledBeforeOpen = (
                True if self._windowMenu.isBlocking() else self.player.getMoveEnabled()
            )
            self.player.setMoveEnabled(False)
        self._windowEnemyBook.open(self._gameMap)
        self._blockMapInput(2)

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

    def _onShopClose(self) -> None:
        self.player.setMoveEnabled(self._shopMoveEnabledBeforeOpen)

    def _onEnemyBookClose(self) -> None:
        self.player.setMoveEnabled(self._enemyBookMoveEnabledBeforeOpen)
        self._blockMapInput(1)

    def _getShopRects(self):
        gameSize = GlobalSystem.getGameSize()
        totalHeight = _SHOP_COMMAND_HEIGHT + _SHOP_ITEM_SIZE
        x = int((gameSize.x - _SHOP_WIDTH) / 2)
        y = int((gameSize.y - totalHeight) / 2)
        return (
            ((x, y), (_SHOP_WIDTH, _SHOP_COMMAND_HEIGHT)),
            ((x, y + _SHOP_COMMAND_HEIGHT), (_SHOP_WIDTH, _SHOP_ITEM_SIZE)),
        )

    def _getEnemyBookRect(self):
        gameSize = GlobalSystem.getGameSize()
        x = int((gameSize.x - _ENEMY_BOOK_SIZE) / 2)
        y = int((gameSize.y - _ENEMY_BOOK_SIZE) / 2)
        return ((x, y), (_ENEMY_BOOK_SIZE, _ENEMY_BOOK_SIZE))

    def _canRestoreMoveAfterMenuClose(self) -> bool:
        return not (self._windowShop.getVisible() or self._windowEnemyBook.getVisible())

    def _blockMapInput(self, frames: int = 1) -> None:
        self._mapInputBlockFrames = max(self._mapInputBlockFrames, frames)

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

    def gotoMapAndPos(self, mapPath: str, pos: Optional[Union[Vector2u, Pair[int], List[int]]] = None) -> None:
        r"""\brief Transition to a map and set the player position.

        - \param mapPath Path to the map data file.
        - \param pos The position to place the player.
        """
        if mapPath and self._cachedMapFile != mapPath:
            self._cachedMapFile = mapPath
            self.loadMap(mapPath)
        self.inst.applyMapInfo(mapPath, pos)

    @ExecSplit(default=(None,))
    def recordDestroyedActor(self, actor: Actor) -> None:
        r"""\brief Record a destroyed actor for persistence.

        - \param actor The destroyed actor.
        """
        self.inst.recordDestroyedActor(self._cachedMapFile, actor)

    def setBgmFilter(self, attr: str, value: Any) -> None:
        r"""\brief Set a filter attribute on the current BGM music.

        - \param attr The filter attribute name.
        - \param value The filter attribute value.
        """
        if self._currentBgmMusic is None:
            return
        data: Dict[str, Any] = {attr: value}
        if attr == "loopPoint" and isinstance(value, (Pair, tuple, list)):
            data["loopPoint"] = {"start": value[0], "end": value[1]}
        filterObj = self._buildMusicFilter(data)
        if filterObj is not None:
            from Global.Manager.Mgr_Audio import AudioManager

            AudioManager.setMusicFilter(self._currentBgmMusic, filterObj)

    def setBgsFilter(self, attr: str, value: Any) -> None:
        r"""\brief Set a filter attribute on the current BGS music.

        - \param attr The filter attribute name.
        - \param value The filter attribute value.
        """
        if self._currentBgsMusic is None:
            return
        data: Dict[str, Any] = {attr: value}
        if attr == "loopPoint" and isinstance(value, (Pair, tuple, list)):
            data["loopPoint"] = {"start": value[0], "end": value[1]}
        filterObj = self._buildMusicFilter(data)
        if filterObj is not None:
            from Global.Manager.Mgr_Audio import AudioManager

            AudioManager.setMusicFilter(self._currentBgsMusic, filterObj)

    def _drawSceneAnims(self) -> None:
        r"""\brief Draw map animations in screen space aligned with the camera view."""
        animSnapshot = self.getAnims()
        if not animSnapshot:
            return
        camera = self._gameMap.getCamera()
        viewPos = camera.getViewPosition() if camera else None
        GlobalSystem.setWindowMapView()
        for anim in animSnapshot:
            worldPosition = anim.getPosition()
            drawPosition = worldPosition
            if viewPos is not None:
                drawPosition = worldPosition - viewPos
            anim.setPosition(drawPosition)
            GlobalSystem.draw(anim)
            anim.setPosition(worldPosition)
        GlobalSystem.setWindowDefaultView()

    def _renderHandle(self, deltaTime: float) -> None:
        self._gameMap.show()
        super()._renderHandle(deltaTime)
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

    def _playMapAudio(self, mapData: Dict[str, Any]) -> None:
        bgm = mapData.get("bgm", "")
        bgmFilter = self._buildMusicFilter(mapData.get("bgmFilter", {}))
        reuseBgm = bool(bgm) and self._currentBgmMusic is not None and self._currentBgmFile == bgm
        if reuseBgm:
            if bgmFilter is not None:
                from Global.Manager.Mgr_Audio import AudioManager

                AudioManager.setMusicFilter(self._currentBgmMusic, bgmFilter)
            Cast(Music, self._currentBgmMusic).setLooping(True)
        else:
            if self._currentBgmMusic is not None:
                Manager.stopMusic("BGM")
                self._currentBgmMusic = None
            self._currentBgmFile = ""
        if bgm:
            if not reuseBgm:
                self._currentBgmMusic = Manager.playMusic("BGM", bgm, bgmFilter)
                if self._currentBgmMusic is not None:
                    self._currentBgmMusic.setLooping(True)
                    self._currentBgmFile = bgm
        bgs = mapData.get("bgs", "")
        bgsFilter = self._buildMusicFilter(mapData.get("bgsFilter", {}))
        reuseBgs = bool(bgs) and self._currentBgsMusic is not None and self._currentBgsFile == bgs
        if reuseBgs:
            if bgsFilter is not None:
                from Global.Manager.Mgr_Audio import AudioManager

                AudioManager.setMusicFilter(self._currentBgsMusic, bgsFilter)
            Cast(Music, self._currentBgsMusic).setLooping(True)
        else:
            if self._currentBgsMusic is not None:
                Manager.stopMusic("BGS")
                self._currentBgsMusic = None
            self._currentBgsFile = ""
        if bgs:
            if not reuseBgs:
                self._currentBgsMusic = Manager.playMusic("BGS", bgs, bgsFilter)
                if self._currentBgsMusic is not None:
                    self._currentBgsMusic.setLooping(True)
                    self._currentBgsFile = bgs

    def _stopMapAudio(self) -> None:
        if self._currentBgmMusic is not None:
            Manager.stopMusic("BGM")
            self._currentBgmMusic = None
        self._currentBgmFile = ""
        if self._currentBgsMusic is not None:
            Manager.stopMusic("BGS")
            self._currentBgsMusic = None
        self._currentBgsFile = ""

    def _buildSoundFilter(self, data: Dict[str, Any]) -> Optional[Filters.SoundFilter]:
        if not data:
            return None
        kwargs: Dict[str, Any] = {}
        for key in ("loop", "offset", "pitch", "pan", "volume"):
            if key in data:
                kwargs[key] = data[key]
        if not kwargs:
            return None
        return Filters.SoundFilter(**kwargs)

    def _buildMusicFilter(self, data: Dict[str, Any]) -> Optional[Filters.MusicFilter]:
        if not data:
            return None
        kwargs: Dict[str, Any] = {}
        for key in ("loop", "offset", "pitch", "pan", "volume"):
            if key in data:
                kwargs[key] = data[key]
        if "loopPoint" in data and isinstance(data["loopPoint"], dict):
            lp = data["loopPoint"]
            kwargs["loopPoint"] = (float(lp.get("start", 0.0)), float(lp.get("end", 0.0)))
            if "offset" not in kwargs:
                start = float(lp.get("start", 0.0))
                if start > 0.0:
                    kwargs["offset"] = start
        if not kwargs:
            return None
        return Filters.MusicFilter(**kwargs)

    def _isMenuOpenTriggered(self) -> bool:
        if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
            return True
        return Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True)

    def _isMapClickMoveBlocked(self) -> bool:
        return (
            self._messageWindow.isInDialogue()
            or self._windowMenu.isBlocking()
            or self._windowShop.getVisible()
            or self._windowEnemyBook.getVisible()
            or self._mapInputBlockFrames > 0
        )

    def _consumeMapClickMoveInput(self) -> None:
        Input.isMouseButtonTriggered(Input.Mouse.Button.Left, handled=True)
        Input.isTouchBegan(handled=True)
        Input.isTouchTriggered(handled=True)

    def _generateTilemap(self, data: Dict[str, List[List[Any]]], width: int, height: int) -> Tilemap:
        mapLayers = []
        for layerName, layerData in data.items():
            name = layerData["layerName"]
            layerTileset = Data.getTileset(layerData["layerTileset"])
            layerTiles = layerData["tiles"]
            tiles: List[List[Optional[int]]] = []
            for y in range(height):
                tiles.append([])
                for x in range(width):
                    tileNumber = layerTiles[y][x]
                    tiles[-1].append(tileNumber)
            rawAutoTiles = layerData.get("autoTiles")
            autoTilePool = []
            autoTileIndexByKey: Dict[str, int] = {}
            autoTileGrid: List[List[Optional[int]]] = []
            if isinstance(rawAutoTiles, list):
                for y in range(height):
                    row: List[Optional[int]] = []
                    rawRow = rawAutoTiles[y] if y < len(rawAutoTiles) else None
                    for x in range(width):
                        cell = rawRow[x] if isinstance(rawRow, list) and x < len(rawRow) else None
                        if isinstance(cell, str) and cell and Data.hasAutoTile(cell):
                            if cell not in autoTileIndexByKey:
                                autoTileIndexByKey[cell] = len(autoTilePool)
                                autoTilePool.append(Data.getAutoTile(cell))
                            row.append(autoTileIndexByKey[cell])
                        elif isinstance(cell, int) and 0 <= cell < len(autoTilePool):
                            row.append(cell)
                        else:
                            row.append(None)
                    autoTileGrid.append(row)
            else:
                for _ in range(height):
                    autoTileGrid.append([None] * width)
            tileLayerData = TileLayerData(
                name,
                layerTileset,
                tiles,
                autoTileGrid,
                autoTilePool,
            )
            autoTileTextures = [Manager.loadAutotile(entry.fileName) for entry in autoTilePool]
            autoTileFrameCounts = []
            for texture in autoTileTextures:
                size = texture.getSize()
                cellSize = Engine.CellSize
                frames = size.x // (3 * cellSize) if cellSize > 0 else 1
                if frames < 1:
                    frames = 1
                autoTileFrameCounts.append(frames)
            layer = TileLayer(
                tileLayerData,
                Manager.loadTileset(tileLayerData.layerTileset.fileName),
                autoTileTextures,
                autoTileFrameCounts,
            )
            mapLayers.append(layer)
        return Tilemap(mapLayers)

    def _generateGameMap(self, data: Dict[str, Any], camera: Optional[Camera] = None) -> GameMap:
        mapName = data["mapName"]
        width = data["width"]
        height = data["height"]
        layers = data["layers"]
        tilemap = self._generateTilemap(layers, width, height)
        ambientLight = data.get("ambientLight", [255, 255, 255, 255])
        lights = data.get("lights", [])
        actors = data.get("actors", [])
        result = GameMap(mapName, tilemap, camera)
        result.setAmbientLight(Color(*ambientLight))
        for lightData in lights:
            result.addLight(Light.fromDict(lightData))
        for layerName, actorDatas in actors.items():
            for actorData in actorDatas:
                actor = Data.genActorFromData(actorData, layerName)
                if actor is None:
                    continue
                result.spawnActor(actor, layerName)
        return result
