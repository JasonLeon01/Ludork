# -*- encoding: utf-8 -*-

import os
from typing import Callable, List, Union, Optional, Dict, Any
from Engine import Pair, Vector2u, Vector2f, Color, Filters, Music, Input
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
from Source.GameInstance import GameInstance


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
        self._windowMenu = WindowMenu(self.player, self._windowItem, self._messageWindow)
        self._uiManager.loadUI(self._playerHUD)
        self._uiManager.loadUI(self._messageWindow)
        self._uiManager.loadUI(self._windowMenu)
        self._uiManager.loadUI(self._windowItem)

        self._windowMenu.close()

        self._gameMap: GameMap = None
        self._cachedMapFile: str = None
        self._currentBgmMusic: Optional[Music] = None
        self._currentBgmFile: str = ""
        self._currentBgsMusic: Optional[Music] = None
        self._currentBgsFile: str = ""
        self._mapClickMoveBlockedUntilLateTick: bool = False
        self.gotoMapAndPos(System.getStartMap())

    def onQuit(self) -> None:
        r"""\brief Stop map BGM/BGS when leaving this scene."""
        self._stopMapAudio()

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
        if not self._windowMenu.isBlocking() and not self._messageWindow.isInDialogue():
            if self._isMenuOpenTriggered():
                self._windowMenu.open()
        return super().onTick(deltaTime)

    def onLateTick(self, deltaTime: float) -> None:
        r"""\brief Forward late-update to the game map.

        - \param deltaTime Elapsed time in seconds.
        """
        if self._mapClickMoveBlockedUntilLateTick:
            self._consumeMapClickMoveInput()
            self._mapClickMoveBlockedUntilLateTick = False
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
        self.player.setMoveEnabled(False)
        self._messageWindow.setMessage(
            refPosition,
            name,
            message,
            onFinished=lambda: self.player.setMoveEnabled(originMoveEnabled),
        )

        def condition() -> bool:
            if self._messageWindow.isInDialogue():
                return False
            self.player.setMoveEnabled(originMoveEnabled)
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
        self.player.setMoveEnabled(False)
        self._messageWindow.setMessage(
            refPosition,
            name,
            options,
            allowCancel=allowCancel,
            onFinished=lambda: self.player.setMoveEnabled(originMoveEnabled),
        )

        def condition() -> Optional[int]:
            selectionResult = self._messageWindow.getSelectionResult()
            if selectionResult is None:
                return None
            self.player.setMoveEnabled(originMoveEnabled)
            return selectionResult

        return condition

    @ExecSplit(default=(None,))
    @TypeAdapter(pos=([tuple, list], Vector2u))
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

    def _renderHandle(self, deltaTime: float) -> None:
        self._gameMap.show()
        super()._renderHandle(deltaTime)

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
        return self._messageWindow.isInDialogue() or self._windowMenu.isBlocking()

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
            tileLayerData = TileLayerData(
                name,
                layerTileset,
                tiles,
            )
            layer = TileLayer(tileLayerData, Manager.loadTileset(tileLayerData.layerTileset.fileName))
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
