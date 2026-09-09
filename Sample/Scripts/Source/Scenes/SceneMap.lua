local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Logging = require("Global.Utils.Logging")
local GameSystem = require("Source.System")
local EventKeys = require("Source.Configs.EventKeys")
local RegionDict = require("Source.Configs.RegionDict")
local GameplayScene = require("Source.Gameplay.GameplayScene")
local ConditionalActor = require("Source.ConditionalActor")
local Teleporter = require("Source.Teleporter")
local MapPath = require("Source.MapPath")
local SceneMapInteractions = require("Source.Scenes.SceneMap.Interactions")
local SceneMapAudioController = require("Source.SceneComponents.MapAudio")
local SceneMapBuilder = require("Source.SceneComponents.MapBuilder")
local RegionTitleController = require("Source.Scenes.SceneMap.RegionTitle.Controller")
local PlayerAttrHUD = require("Source.Windows.HUDPlayerAttr")
local WindowEquip = require("Source.Windows.WindowEquip")
local WindowAttrShop = require("Source.Windows.WindowAttrShop")
local WindowEnemyBook = require("Source.Windows.WindowEnemyBook")
local WindowEnemyEncyclopedia = require("Source.Windows.WindowEnemyEncyclopedia")
local ConfigWindow = require("Source.Windows.ConfigWindow")
local WindowFloorTeleporter = require("Source.Windows.WindowFloorTeleporter")
local WindowItem = require("Source.Windows.WindowItem")
local WindowMenu = require("Source.Windows.WindowMenu")
local WindowMessage = require("Source.Windows.WindowMessage")
local WindowSaveLoad = require("Source.Windows.WindowSaveLoad")
local WindowShop = require("Source.Windows.WindowShop")
local WindowPlayerName = require("Source.Windows.WindowPlayerName")

local Input = Engine.Input
local Direction = Engine.FocusDirection
local FocusGroup = GlobalCore.FocusGroup
local FocusNeighbor = GlobalCore.FocusNeighbor
local FocusTransition = GlobalCore.FocusTransition
local GlobalSystem = GlobalCore.System
local ManagerFunctions = GlobalFunctions.Manager

local WORLD_AMBIENT_TRANSITION_TIME = 0.5

---@param name    string
---@param control Engine.FunctionalBase
---@return GlobalCore.FocusGroup
local function createSingleControlFocusGroup(name, control)
    return FocusGroup.new(name, { control }, control)
end

---@param uiManager GlobalCore.UIManager
---@param ...       Engine.ControlBase
local function loadUiControls(uiManager, ...)
    ---@type Engine.ControlBase[]
    local controls = { ... }
    for _, control in ipairs(controls) do
        uiManager:loadUI(control)
    end
end

---@param from  sf.Color
---@param to    sf.Color
---@param alpha number
---@return sf.Color
local function interpolateColour(from, to, alpha)
    return sf.Color.new(
        math.round(math.lerp(from.r, to.r, alpha)), math.round(math.lerp(from.g, to.g, alpha)),
        math.round(math.lerp(from.b, to.b, alpha)), math.round(math.lerp(from.a, to.a, alpha))
    )
end

---@class (partial) Source.Scenes.SceneMap.SceneMap: Source.Gameplay.GameplayScene
local Scene = {}

---@diagnostic disable-next-line: unused
function Scene:onEnter()
    GlobalSystem.setTransition()
end

function Scene:setInst(inst)
    self._gameOverRequest = nil
    self.inst = inst
end

function Scene:onCreate()
    self._gameplayRequestsActive = true
    self._gameOverRequest = nil
    local uiManager = assert(self:getUIManager(), "Scene map UI manager is unavailable")
    uiManager:setFocusNavigationEnabled(true)
    self.player = self.inst:getPlayer()
    self._mapBuilder = SceneMapBuilder.new()
    self._mapAudio = SceneMapAudioController.new()
    ---@type Source.Scenes.SceneMap.SceneMap[]
    local sceneRef = setmetatable({ self }, { __mode = "v" })
    self._playerHUD = PlayerAttrHUD.new(self.player, function ()
        local scene = sceneRef[1]
        if scene ~= nil then
            scene:openMenu()
        end
    end)
    self._messageWindow = WindowMessage.new()
    self._playerNameMoveEnabledBeforeOpen = true
    self._windowPlayerName = WindowPlayerName.new(self.player, function ()
        self.player:setMoveEnabled(self._playerNameMoveEnabledBeforeOpen)
        self:_blockMapInput(2)
    end)
    self._dialogueLocaleSource = nil
    self._windowItem = WindowItem.new(self.player)
    self._windowEquip = WindowEquip.new(self.player)
    self._shopMoveEnabledBeforeOpen = true
    self._windowShop = WindowShop.new(self.player, function ()
        self:_onShopClose()
    end)
    self._attrShopMoveEnabledBeforeOpen = true
    self._windowAttrShop = WindowAttrShop.new(self.player, function ()
        self:_onAttrShopClose()
    end)
    self._enemyBookMoveEnabledBeforeOpen = true
    self._windowEnemyBook = WindowEnemyBook.new(
        self.player,
        function ()
            self:_onEnemyBookClose()
        end,
        function (entry)
            self:_onEnemyBookConfirm(entry)
        end
    )
    self._windowEnemyEncyclopedia = WindowEnemyEncyclopedia.new(function ()
        self:_onEnemyEncyclopediaClose()
    end)
    self._floorTeleporterMoveEnabledBeforeOpen = true
    self._windowFloorTeleporter = WindowFloorTeleporter.new(
        self.inst,
        function (mapKey, telepoint, previewSize, previewScale, showTelepointMarker)
            return self:_buildFloorMapPreview(mapKey, telepoint, previewSize, previewScale, showTelepointMarker)
        end,
        function (mapKey, telepoint)
            self:_onFloorTeleporterConfirm(mapKey, telepoint)
        end,
        function ()
            self:_onFloorTeleporterClose()
        end,
        function (mapKey)
            return self._mapBuilder:resolveMapPath(mapKey, self:_getCurrentRegionMap())
        end,
        function ()
            self._mapBuilder:clearFloorMapPreviewCache()
        end
    )
    self._windowSaveLoad = WindowSaveLoad.new(
        false,
        function ()
            return self:_getSaveSource()
        end,
        function (reason)
            self:_onSaveLoadClose(reason)
        end,
        function (inst)
            self:applyLoadedGame(inst)
        end
    )
    self._configWindow = ConfigWindow.new(function ()
        self:_onConfigClose()
    end)
    self._windowMenu = WindowMenu.new(self.player, {
        item = self._windowItem,
        equip = self._windowEquip,
        saveLoad = self._windowSaveLoad,
        config = self._configWindow
    })
    self._blockingWindows = {
        self._windowShop, self._windowAttrShop, self._windowEnemyBook, self._windowEnemyEncyclopedia,
        self._windowFloorTeleporter, self._windowPlayerName
    }
    self._windowMenu:setMoveRestoreGuard(function ()
        return self:_canRestoreMoveAfterMenuClose()
    end)
    self:_registerFocusGroups()
    self._regionTitleUI = RegionTitleController.new(GlobalSystem.getGameSize())
    self._regionTitleUI:prepare()
    self._regionTitleText = self._regionTitleUI:getText()
    loadUiControls(
        uiManager, self._playerHUD, self._messageWindow, self._windowMenu, self._windowItem, self._windowEquip,
        self._windowShop, self._windowAttrShop, self._windowEnemyBook, self._windowEnemyEncyclopedia,
        self._windowFloorTeleporter, self._windowSaveLoad, self._configWindow, self._windowPlayerName
    )
    self._localeChangedToken = Engine.subscribe(EventKeys.LocaleChanged, function ()
        local scene = sceneRef[1]
        if scene ~= nil then
            scene:refreshLocale()
        end
    end)

    self._windowMenu:hideImmediate()
    self._gameMap = nil
    self._cachedMapFile = nil
    self._currentRegion = nil
    self._mapClickMoveBlockedUntilLateTick = false
    self._mapInputBlockFrames = 0
    self._pendingMenuOpen = false
    self._pendingFloorTransfer = nil
    self._pendingWorldTransfer = nil
    self._mapTransferInProgress = false
    self._worldEnvironmentKey = nil
    self._worldAmbientStartColour = nil
    self._worldAmbientTargetColour = nil
    self._worldAmbientTransitionElapsed = 0
    local startMap = self.inst:getCurrentMapPath() or GameSystem.GetStartMap()
    self:gotoMapAndPos(startMap, nil, true)
end

function Scene:_registerFocusGroups()
    local uiManager = assert(self:getUIManager(), "Scene map UI manager is unavailable")
    local menuGroup = createSingleControlFocusGroup("menu", self._windowMenu)
    local itemGroup = createSingleControlFocusGroup("item", self._windowItem)
    itemGroup:setNeighbor(Direction.LEFT, menuGroup)

    local equipSlotControl, equipSelectControl = self._windowEquip:getFocusControls()
    local equipSlotGroup = createSingleControlFocusGroup("equip-slot", equipSlotControl)
    local equipSelectGroup = createSingleControlFocusGroup("equip-select", equipSelectControl)
    equipSlotGroup:setNeighbor(Direction.LEFT, menuGroup)
    equipSlotGroup:setNeighbor(Direction.RIGHT, FocusNeighbor.new(equipSelectGroup, FocusTransition.EXPLICIT))
    equipSelectGroup:setNeighbor(Direction.LEFT, FocusNeighbor.new(equipSlotGroup, FocusTransition.EXPLICIT))

    local shopItemWindow = self._windowShop:getItemWindow()
    local shopItemGroup = createSingleControlFocusGroup("shop-item", shopItemWindow)

    local floorCommandWindow = self._windowFloorTeleporter:getCommandWindow()
    local floorPreviewWindow = self._windowFloorTeleporter:getPreviewWindow()
    local floorCommandGroup = createSingleControlFocusGroup("floor-command", floorCommandWindow)
    local floorPreviewGroup = createSingleControlFocusGroup("floor-preview", floorPreviewWindow)
    floorCommandGroup:setNeighbor(Direction.RIGHT, FocusNeighbor.new(floorPreviewGroup, FocusTransition.EXPLICIT))
    floorPreviewGroup:setNeighbor(Direction.LEFT, FocusNeighbor.new(floorCommandGroup, FocusTransition.EXPLICIT))

    local saveSlotWindow = self._windowSaveLoad:getSlotWindow()
    local saveSlotGroup = createSingleControlFocusGroup("save-slot", saveSlotWindow)
    saveSlotGroup:setNeighbor(Direction.LEFT, menuGroup)

    local playerNameGroup = FocusGroup.new(
        "player-name", self._windowPlayerName:getFocusControls(), self._windowPlayerName
    )

    local groups = {
        menuGroup, itemGroup, equipSlotGroup, equipSelectGroup, shopItemGroup, floorCommandGroup, floorPreviewGroup,
        saveSlotGroup, playerNameGroup
    }
    for _, group in ipairs(groups) do
        uiManager:registerFocusGroup(group)
    end
end

function Scene:onQuit()
    ManagerFunctions.stopVoice()
    self._mapAudio:stopMapAudio()
    GlobalSystem.clearWeather()
    GlobalSystem.clearFog()
end

function Scene:onDestroy()
    self._gameplayRequestsActive = false
    self._gameOverRequest = nil
    if self._gameMap ~= nil then
        ConditionalActor.ReleaseMapMonitors(self._gameMap)
        self._gameMap:disposeStreaming()
    end
    ManagerFunctions.stopVoice()
    if self._localeChangedToken ~= nil then
        Engine.unsubscribe(self._localeChangedToken)
        self._localeChangedToken = nil
    end
    self._dialogueLocaleSource = nil
    self._mapAudio:stopMapAudio()
    self._messageWindow:hideImmediate()
    self._windowMenu:hideImmediate()
    self._windowItem:hideImmediate()
    self._windowEquip:hideImmediate()
    self._windowAttrShop:hideImmediate()
    self._windowEnemyBook:hideImmediate()
    self._windowEnemyEncyclopedia:hideImmediate()
    self._messageWindow:dispose()
    self._windowMenu:dispose()
    self._windowItem:dispose()
    self._windowEquip:dispose()
    self._windowAttrShop:dispose()
    self._windowEnemyBook:dispose()
    self._windowEnemyEncyclopedia:dispose()
    self._playerHUD:dispose()
    self._windowSaveLoad:dispose()
    self._windowShop:dispose()
    self._windowFloorTeleporter:dispose()
    self._configWindow:dispose()
    self._windowPlayerName:dispose()
    self._regionTitleUI:dispose()
end

function Scene:refreshLocale()
    if self._dialogueLocaleSource ~= nil and self._messageWindow:isInDialogue() then
        if self._dialogueLocaleSource.kind == "selection" then
            ---@cast self._dialogueLocaleSource Source.Scenes.SceneMap.DialogueSelectionLocaleSource
            local name, options = Scene.FormatDialogueSelectionSource(self._dialogueLocaleSource)
            self._messageWindow:refreshSelection(name, options)
        else
            ---@cast self._dialogueLocaleSource Source.Scenes.SceneMap.DialogueMessageLocaleSource
            local name, message = Scene.FormatDialogueMessageSource(self._dialogueLocaleSource)
            self._messageWindow:refreshMessage(name, message)
        end
    end
    self._windowMenu:refreshRows()
    self._windowItem:refreshLocale()
    self._windowEquip:refreshLocale()
    self._windowShop:refreshLocale()
    self._windowAttrShop:refreshLocale()
    self._windowEnemyBook:refreshLocale()
    self._windowEnemyEncyclopedia:refreshLocale()
    self._windowFloorTeleporter:refreshLocale()
    self._windowPlayerName:refreshLocale()
end

function Scene:onFixedTick(fixedDelta)
    if not self._mapTransferInProgress then
        self:getGameMap():onFixedTick(fixedDelta)
    end
    return super(Scene, self).onFixedTick(fixedDelta)
end

function Scene:onInput()
    if self:isInputBlocked() then
        return
    end
    local HotKey = require("Source.Configs.HotKey")

    for key, hotKeyConfig in pairs(HotKey) do
        if Class.isInstance(self, hotKeyConfig.Scene) and hotKeyConfig.Filter ~= nil
            and table.contains(hotKeyConfig.Filter, "casual") then
            local functionWhenPressed = hotKeyConfig.FunctionWhenPressed
            if functionWhenPressed ~= nil and Input.getKeyPressed(key, false) then
                functionWhenPressed(self)
                Input.getKeyPressed(key, true)
            end
            local functionWhenReleased = hotKeyConfig.FunctionWhenReleased
            if functionWhenReleased ~= nil and Input.getKeyReleased(key, false) then
                functionWhenReleased(self)
                Input.getKeyReleased(key, true)
            end
        end
    end
end

function Scene:onTick(deltaTime)
    self._mapAudio:onTick(deltaTime)
    if self._dialogueLocaleSource ~= nil and not self._messageWindow:isInDialogue() then
        self._dialogueLocaleSource = nil
    end
    if self._mapTransferInProgress then
        return super(Scene, self).onTick(deltaTime)
    end
    self._mapClickMoveBlockedUntilLateTick = self:_isMapClickMoveBlocked()
    local gameMap = self:getGameMap()
    gameMap:onTick(deltaTime)
    gameMap:updateAutoTileAnimation(deltaTime)
    self:_updateWorldEnvironment(deltaTime)
    self:_updateRegionTitle(deltaTime)
    if self:_canOpenMenu() and Scene.IsMenuOpenTriggered() then
        self:openMenu()
    end
    return super(Scene, self).onTick(deltaTime)
end

function Scene:onLateTick(deltaTime)
    if self._mapTransferInProgress then
        return super(Scene, self).onLateTick(deltaTime)
    end
    if self._mapClickMoveBlockedUntilLateTick or self:_isMapClickMoveBlocked() then
        Scene.ConsumeMapClickMoveInput()
        self._mapClickMoveBlockedUntilLateTick = false
    end
    if self._mapInputBlockFrames > 0 then
        Scene.ConsumeMapClickMoveInput()
        self._mapInputBlockFrames = self._mapInputBlockFrames - 1
    end
    self:getGameMap():onLateTick(deltaTime)
    return super(Scene, self).onLateTick(deltaTime)
end

function Scene:loadMap(mapPath, initialPosition)
    Logging.info("Loading map: %s", mapPath)
    local startTime = perfCounter()
    local mapFile, mapData = self._mapBuilder:loadMapData(mapPath, self:_getCurrentRegionMap())
    ---@type GameMap
    local gameMap
    if mapData.type == "worldMap" then
        ---@cast mapData Source.SceneComponents.WorldMapData
        gameMap = self._mapBuilder:generateWorldGameMap(mapFile, mapData, self.inst, initialPosition)
    else
        ---@cast mapData Source.SceneComponents.MapData
        gameMap = self._mapBuilder:generateGameMap(mapData, nil, false)
    end
    if self._gameMap ~= nil then
        ConditionalActor.ReleaseMapMonitors(self._gameMap)
        self._gameMap:disposeStreaming()
    end
    self._gameOverRequest = nil
    self._gameMap = gameMap
    self._cachedMapFile = mapFile
    gameMap:setScene(self)
    self.inst:applyMapInfo(mapFile, initialPosition)
    if not gameMap:isWorldMap() then
        gameMap:applyTerrainDestructions(self.inst:getTerrainDestructions(mapFile))
        self._mapBuilder:applyAddedActors(gameMap, self.inst:getAddedActors(mapFile), false)
        gameMap:applyActorPositions(self.inst:getActorPositions(mapFile))
        gameMap:removeActorsByTags(self.inst:getDestroyedActors(mapFile))
    end
    gameMap:setPlayer(self.player)
    gameMap:spawnActor(self.player, "default")
    self._worldEnvironmentKey = nil
    self._worldAmbientStartColour = nil
    self._worldAmbientTargetColour = nil
    self._worldAmbientTransitionElapsed = 0
    if not gameMap:isWorldMap() then
        ---@cast mapData Source.SceneComponents.MapData
        self._mapAudio:playMapAudio(mapData)
        GlobalSystem.clearFog()
        GlobalSystem.applyFogFromMapData(GlobalCore.MapFogSettings.new({
                fog = mapData.fog,
                fogPower = mapData.fogPower,
                fogOx = mapData.fogOx,
                fogOy = mapData.fogOy,
                fogDistort = mapData.fogDistort
            }))
    end
    self:_updateCurrentRegion(mapFile)
    Logging.info("Loaded map %s in %.3fs", mapFile, perfCounter() - startTime)
    return mapFile
end

function Scene:_updateWorldEnvironment(deltaTime, force)
    if self._gameMap == nil or not self._gameMap:isWorldMap() then
        return
    end
    local worldMap = self._gameMap
    ---@cast worldMap Global.WorldGameMap.WorldGameMap
    local region = worldMap:getRegionPosition(self.player:getMapPosition())
    local regionPath = region ~= nil and region.path or ""
    local mapData = worldMap:getEnvironmentDataAt(self.player:getMapPosition())
    local environmentKey = regionPath .. ":" .. (mapData ~= nil and "loaded" or "empty")
    if force or self._worldEnvironmentKey ~= environmentKey then
        self._worldEnvironmentKey = environmentKey
        local targetColour
        if mapData == nil then
            self._mapAudio:playMapAudio({}, 0.5)
            targetColour = sf.Color.new(255, 255, 255, 255)
        else
            self._mapAudio:playMapAudio(mapData, 0.5)
            targetColour = mapData.ambientLight
        end
        if self._worldAmbientTargetColour == nil then
            self._worldAmbientTargetColour = copy(targetColour)
            worldMap:setAmbientLight(self._worldAmbientTargetColour)
        elseif self._worldAmbientTargetColour ~= targetColour then
            self._worldAmbientStartColour = copy(worldMap:getAmbientLight())
            self._worldAmbientTargetColour = copy(targetColour)
            self._worldAmbientTransitionElapsed = 0
        end
    end
    if self._worldAmbientStartColour == nil then
        return
    end
    self._worldAmbientTransitionElapsed = math.clamp(
        self._worldAmbientTransitionElapsed + deltaTime, 0.0, WORLD_AMBIENT_TRANSITION_TIME
    )
    local alpha = self._worldAmbientTransitionElapsed / WORLD_AMBIENT_TRANSITION_TIME
    worldMap:setAmbientLight(
        interpolateColour(self._worldAmbientStartColour, assert(self._worldAmbientTargetColour), alpha)
    )
    if self._worldAmbientTransitionElapsed >= WORLD_AMBIENT_TRANSITION_TIME then
        self._worldAmbientStartColour = nil
    end
end

function Scene:playBgm(bgm, bgmFilter)
    self._mapAudio:playBgm(bgm, bgmFilter)
end

function Scene:setBgmFilter(attr, value)
    self._mapAudio:setBgmFilter(attr, value)
end

function Scene:setBgsFilter(attr, value)
    self._mapAudio:setBgsFilter(attr, value)
end

-- Draw map animations in screen space aligned with the camera view.
function Scene:_drawSceneAnims()
    local animSnapshot = self:getAnims()
    if not bool(animSnapshot) then
        return
    end
    local gameMap = self:getGameMap()
    GlobalSystem.setWindowMapView(gameMap:getMapViewRect())
    for _, anim in ipairs(animSnapshot) do
        local worldPosition = anim:getPosition()
        local drawPosition = gameMap:worldToMapViewPosition(worldPosition)
        anim:setPosition(drawPosition)
        GlobalSystem.draw(anim)
        anim:setPosition(worldPosition)
    end
    GlobalSystem.setWindowDefaultView()
end

function Scene:_drawCommonTipOverlay()
    super(Scene, self)._drawCommonTipOverlay()
    if self._regionTitleUI:getVisible() then
        self._regionTitleUI:draw()
    end
end

---@param deltaTime number
function Scene:_renderHandle(deltaTime)
    self:getGameMap():show()
    super(Scene, self)._renderHandle(deltaTime)
    self:_processPendingFloorTransfer()
    self:_processPendingWorldTransfer()
    if self._pendingMenuOpen then
        self._pendingMenuOpen = false
        Scene.CaptureScreenSnapshot()
        self._windowMenu:open()
    end
end

function Scene.CaptureScreenSnapshot()
    local canvas = GlobalSystem.getCanvas()
    local sourceTexture = canvas:getTexture()
    local sourceSize = sourceTexture:getSize()
    local gameSize = GlobalSystem.getGameSize()
    if sourceSize.x == 0 or sourceSize.y == 0 then
        GameSystem.SetSavedScreenImage(nil)
        return
    end
    local scaled = sf.RenderTexture.new(gameSize)
    scaled:clear(sf.Color.Black)
    local sprite = sf.Sprite.new(sourceTexture)
    sprite:setScale(sf.Vector2f.new(gameSize.x / sourceSize.x, gameSize.y / sourceSize.y))
    scaled:draw(sprite)
    scaled:display()
    GameSystem.SetSavedScreenImage(scaled:getTexture():copyToImage())
end

---@return boolean
function Scene.IsMenuOpenTriggered()
    return Input.isMouseButtonTriggered(sf.Mouse.Button.Right, true)
        or Input.isAnyJoystickButtonTriggered(Engine.JoystickButton.getMenu(), true)
end

---@return boolean
function Scene:_canOpenMenu()
    return not self._pendingMenuOpen and not self._windowMenu:isBlocking()
        and not self._messageWindow:isInDialogue() and not self:_hasVisibleBlockingWindow()
end

---@return boolean
function Scene:_canOpenItemOverlay()
    return self._windowMenu:getVisible() and self._windowItem:getVisible()
        and not self._messageWindow:isInDialogue() and not self:_hasVisibleBlockingWindow()
end

---@return boolean
function Scene:_isMapClickMoveBlocked()
    return self._messageWindow:isInDialogue() or self._windowMenu:isBlocking()
        or self:_hasVisibleBlockingWindow() or self._mapInputBlockFrames > 0
end

function Scene.ConsumeMapClickMoveInput()
    Input.isMouseButtonTriggered(sf.Mouse.Button.Left, true)
    Input.isTouchTap(true)
    Input.isTouchTriggered(true)
end

---@param mapKey              string
---@param telepoint           sf.Vector2u
---@param previewSize         integer
---@param previewScale        number
---@param showTelepointMarker boolean
---@return sf.Texture
function Scene:_buildFloorMapPreview(mapKey, telepoint, previewSize, previewScale, showTelepointMarker)
    return self._mapBuilder:buildFloorMapPreview(
        self.inst, self:_getCurrentRegionMap(), mapKey, telepoint, previewSize, previewScale, showTelepointMarker
    )
end

function Scene:resolveRegionMapPath(mapKey)
    return self._mapBuilder:resolveRegionMapPath(mapKey, self:_getCurrentRegionMap())
end

---@return string
function Scene:_getCurrentRegionMap()
    return self._cachedMapFile or self.inst:getCurrentMapPath() or GameSystem.GetStartMap()
end

---@param mapFile string
function Scene:_updateCurrentRegion(mapFile)
    local region = Scene.FindRegionForMap(mapFile)
    if region == self._currentRegion then
        return
    end
    self._currentRegion = region
    if region ~= nil then
        self.inst:setCurrentRegion(region)
        Scene.ShowRegionTitle(region)
    end
end

---@param mapFile string
---@return string | nil
function Scene.FindRegionForMap(mapFile)
    local currentName = Scene.NormaliseRegionMapName(mapFile)
    local currentBaseName = os.path.basename(currentName)
    for region, regionMaps in pairs(RegionDict) do
        for _, regionMap in ipairs(regionMaps) do
            local regionMapName = Scene.NormaliseRegionMapName(regionMap)
            if regionMapName == currentName
                or (not regionMapName:find("/", 1, true) and regionMapName == currentBaseName) then
                return region
            end
        end
    end
    return nil
end

---@param mapPath string
---@return string
function Scene.NormaliseRegionMapName(mapPath)
    return (os.path.splitext(MapPath.Normalise(mapPath)))
end

---@param region string
function Scene.ShowRegionTitle(region)
    RegionTitleController.Publish({
        region = region
    })
end

---@param deltaTime number
function Scene:_updateRegionTitle(deltaTime)
    self._regionTitleUI:update(deltaTime)
end

function Scene:getGameMap()
    return SceneMapInteractions.GetGameMap(self)
end

function Scene:getGameInstance()
    return self.inst
end

function Scene:requestFloorStep(teleporter, step)
    assert(step == 1 or step == -1, "Floor transfer step must be 1 or -1")
    if not self._gameplayRequestsActive or GlobalSystem.getScene() ~= self or self._mapTransferInProgress
        or self._pendingFloorTransfer ~= nil or self._pendingWorldTransfer ~= nil or self._gameMap == nil
        or teleporter:isDestroyed() or not teleporter:isVisibleInHierarchy() or teleporter:getMap() ~= self._gameMap then
        return false
    end
    local player = self._gameMap:getPlayer()
    if player == nil or not bool(self._cachedMapFile) then
        return false
    end
    ---@cast self._cachedMapFile string
    local regionMaps = RegionDict[self.inst:getCurrentRegion()] or {}
    local currentIndex = Teleporter.FindCurrentMapIndex(regionMaps, self._cachedMapFile)
    if currentIndex == nil then
        return false
    end
    local targetIndex = currentIndex + step
    if targetIndex < 1 or targetIndex > #regionMaps then
        return false
    end
    local targetMapKey = regionMaps[targetIndex]
    ---@cast targetMapKey string
    local anchorPosition = teleporter:getTeleportPosition()
    local targetMap = self:resolveRegionMapPath(targetMapKey)
    local moveEnabled = player:getMoveEnabled()
    player:setMoveEnabled(false)
    if not self:requestFloorTransfer(targetMap, anchorPosition, moveEnabled) then
        player:setMoveEnabled(moveEnabled)
        return false
    end
    local sourceTelepoint = sf.Vector2u.new(anchorPosition.x, anchorPosition.y)
    ---@cast sourceTelepoint sf.Vector2u
    self.inst:recordTelepoint(self._cachedMapFile, sourceTelepoint, teleporter:getMapTag())
    GlobalCore.AudioManager.playSound(teleporter.stairSE)
    return true
end

function Scene:requestGameOver(player, delay)
    assert(math.isFinite(delay) and delay >= 0, "Game over delay must be finite and non-negative")
    if not self._gameplayRequestsActive or GlobalSystem.getScene() ~= self
        or self._gameMap == nil or player ~= self.player
        or self._gameMap:getPlayer() ~= player or player:isDestroyed()
        or player:getMap() ~= self._gameMap or self._gameOverRequest ~= nil then
        return
    end
    local request = { player = player, gameMap = self._gameMap }
    self._gameOverRequest = request
    local function finishGameOver()
        if not self._gameplayRequestsActive or self._gameOverRequest ~= request
            or GlobalSystem.getScene() ~= self or self.player ~= request.player
            or self._gameMap ~= request.gameMap or request.gameMap:getPlayer() ~= request.player
            or request.player:isDestroyed() or request.player:getMap() ~= request.gameMap then
            return
        end
        local SceneGameOver = require("Source.Scenes.SceneGameOver")

        self._gameplayRequestsActive = false
        self._gameOverRequest = nil
        GlobalSystem.setScene(SceneGameOver.new())
    end
    if delay == 0 then
        finishGameOver()
    else
        self:addTimer(delay, finishGameOver)
    end
end

function Scene:showMessage(name, message, refActor, localeArgs)
    return SceneMapInteractions.ShowMessage(self, name, message, refActor, localeArgs)
end

function Scene:showSelection(name, options, refActor, allowCancel, localeArgs)
    return SceneMapInteractions.ShowSelection(self, name, options, refActor, allowCancel, localeArgs)
end

function Scene:applyLoadedGame(inst)
    return SceneMapInteractions.ApplyLoadedGame(self, inst)
end

function Scene:_rebindPlayerToUI()
    return SceneMapInteractions.RebindPlayerToUI(self)
end

function Scene:showEnemyBook()
    return SceneMapInteractions.ShowEnemyBook(self)
end

function Scene:showFloorTeleporter()
    return SceneMapInteractions.ShowFloorTeleporter(self)
end

function Scene:openMenu()
    return SceneMapInteractions.OpenMenu(self)
end

function Scene:openShop(buyItemIDs, canSell)
    return SceneMapInteractions.OpenShop(self, buyItemIDs, canSell)
end

function Scene:openAttrShop(actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName)
    return SceneMapInteractions.OpenAttrShop(
        self, actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName
    )
end

function Scene:_onShopClose()
    return SceneMapInteractions.OnShopClose(self)
end

function Scene:_onAttrShopClose()
    return SceneMapInteractions.OnAttrShopClose(self)
end

function Scene:_onEnemyBookClose()
    return SceneMapInteractions.OnEnemyBookClose(self)
end

function Scene:_onEnemyBookConfirm(entry)
    return SceneMapInteractions.OnEnemyBookConfirm(self, entry)
end

function Scene:_onEnemyEncyclopediaClose()
    return SceneMapInteractions.OnEnemyEncyclopediaClose(self)
end

function Scene:_onFloorTeleporterClose()
    return SceneMapInteractions.OnFloorTeleporterClose(self)
end

function Scene:_onFloorTeleporterConfirm(mapKey, telepoint)
    return SceneMapInteractions.OnFloorTeleporterConfirm(self, mapKey, telepoint)
end

function Scene.GetDialogueLocalVars(nodeFunction)
    return SceneMapInteractions.GetDialogueLocalVars(nodeFunction)
end

function Scene.FormatDialogueMessageSource(source)
    return SceneMapInteractions.FormatDialogueMessageSource(source)
end

function Scene.FormatDialogueSelectionSource(source)
    return SceneMapInteractions.FormatDialogueSelectionSource(source)
end

function Scene:_canRestoreMoveAfterMenuClose()
    return SceneMapInteractions.CanRestoreMoveAfterMenuClose(self)
end

function Scene:openPlayerName()
    return SceneMapInteractions.OpenPlayerName(self)
end

function Scene:_hasVisibleBlockingWindow()
    return SceneMapInteractions.HasVisibleBlockingWindow(self)
end

function Scene:_blockMapInput(frames)
    return SceneMapInteractions.BlockMapInput(self, frames)
end

function Scene:requestFloorTransfer(targetMap, anchorPos, moveEnabled)
    return SceneMapInteractions.RequestFloorTransfer(self, targetMap, anchorPos, moveEnabled)
end

function Scene:_processPendingFloorTransfer()
    return SceneMapInteractions.ProcessPendingFloorTransfer(self)
end

function Scene:_cancelFloorTransfer(moveEnabled)
    return SceneMapInteractions.CancelFloorTransfer(self, moveEnabled)
end

function Scene:_applyMapDestination(targetMap, targetPosition, blockTransition)
    return SceneMapInteractions.ApplyMapDestination(self, targetMap, targetPosition, blockTransition)
end

function Scene:_queueWorldTransfer(targetMap, targetPosition)
    return SceneMapInteractions.QueueWorldTransfer(self, targetMap, targetPosition)
end

function Scene:_processPendingWorldTransfer()
    return SceneMapInteractions.ProcessPendingWorldTransfer(self)
end

function Scene:_getSaveSource()
    return SceneMapInteractions.GetSaveSource(self)
end

function Scene:_onSaveLoadClose(reason)
    return SceneMapInteractions.OnSaveLoadClose(self, reason)
end

function Scene:_onConfigClose()
    return SceneMapInteractions.OnConfigClose(self)
end

function Scene:gotoMapAndPos(mapPath, pos, blockTransition)
    return SceneMapInteractions.GotoMapAndPos(self, mapPath, pos, blockTransition)
end

function Scene:tryCenterSymmetricTeleport()
    return SceneMapInteractions.TryCenterSymmetricTeleport(self)
end

function Scene:tryAdjacentFloorSamePos(step)
    return SceneMapInteractions.TryAdjacentFloorSamePos(self, step)
end

function Scene:_isMapPositionPassable(mapPath, actor, position)
    return SceneMapInteractions.IsMapPositionPassable(self, mapPath, actor, position)
end

function Scene:recordAddedActor(actor)
    return SceneMapInteractions.RecordAddedActor(self, actor)
end

function Scene:recordActorPosition(actor, position)
    return SceneMapInteractions.RecordActorPosition(self, actor, position)
end

function Scene:recordDestroyedActor(actor)
    return SceneMapInteractions.RecordDestroyedActor(self, actor)
end

function Scene:recordDestroyedActorTag(actorTag)
    return SceneMapInteractions.RecordDestroyedActorTag(self, actorTag)
end

function Scene:recordTerrainDestructions(layerName, positions)
    return SceneMapInteractions.RecordTerrainDestructions(self, layerName, positions)
end

return class(Scene, GameplayScene)
