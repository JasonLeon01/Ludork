local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local WorldGameMap = require("Global.WorldGameMap")
local Logging = require("Global.Utils.Logging")
local GameSystem = require("Source.System")
local EventKeys = require("Source.Configs.EventKeys")
local MapPath = require("Source.MapPath")
local SceneMapInteractions = require("Source.Scenes.SceneMap.Interactions")
local SceneMapAudioController = require("Source.SceneComponents.MapAudio")
local SceneMapBuilder = require("Source.SceneComponents.MapBuilder")
local RegionTitleUI = require("Source.UI.RegionTitle")
local PlayerAttrHUD = require("Source.Windows.HUDPlayerAttr")
local WindowEquipSelect = require("Source.Windows.WindowEquip.Select")
local WindowEquipSlot = require("Source.Windows.WindowEquip.Slot")
local WindowEquipStatus = require("Source.Windows.WindowEquip.Status")
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

local Input = Engine.Input
local Direction = Engine.FocusDirection
local FocusGroup = GlobalCore.FocusGroup
local FocusNeighbor = GlobalCore.FocusNeighbor
local FocusTransition = GlobalCore.FocusTransition
local SceneBase = GlobalCore.SceneBase
local GlobalSystem = GlobalCore.System
local ManagerFunctions = GlobalFunctions.Manager

local EQUIP_SLOT_WIDTH = 196
local EQUIP_SLOT_HEIGHT = 160
local EQUIP_SELECT_HEIGHT = 192
local EQUIP_STATUS_X = 384
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
        Engine.Round(Engine.Lerp(from.r, to.r, alpha)), Engine.Round(Engine.Lerp(from.g, to.g, alpha)),
        Engine.Round(Engine.Lerp(from.b, to.b, alpha)), Engine.Round(Engine.Lerp(from.a, to.a, alpha))
    )
end

---@class (partial) Source.Scenes.SceneMap.SceneMap: GlobalCore.SceneBase
local Scene = {}

---@alias SceneMapInteractionsState Source.Scenes.SceneMap.SceneMap

---@diagnostic disable-next-line: unused
function Scene:onEnter()
    GlobalSystem.setTransition()
end

function Scene:setInst(inst)
    self.inst = inst
end

function Scene:onCreate()
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
    self._dialogueLocaleSource = nil
    self._windowItem = WindowItem.new(Engine.ToIntRect(192, 0, 256, 256), self.player)
    self._windowEquipSlot = WindowEquipSlot.new(
        Engine.ToIntRect(192, 0, EQUIP_SLOT_WIDTH, EQUIP_SLOT_HEIGHT), self.player
    )
    self._windowEquipSelect = WindowEquipSelect.new(
        Engine.ToIntRect(192, EQUIP_SLOT_HEIGHT, EQUIP_SLOT_WIDTH, EQUIP_SELECT_HEIGHT), self.player
    )
    self._windowEquipStatus = WindowEquipStatus.new(
        Engine.ToIntRect(EQUIP_STATUS_X, 0, 640 - EQUIP_STATUS_X, EQUIP_SLOT_HEIGHT + EQUIP_SELECT_HEIGHT), self.player
    )
    self._windowEquipSlot:setEquipSelectWindow(self._windowEquipSelect)
    self._windowEquipSlot:setEquipStatusWindow(self._windowEquipStatus)
    self._windowEquipSelect:setEquipSlotWindow(self._windowEquipSlot)
    self._windowEquipSelect:setEquipStatusWindow(self._windowEquipStatus)
    local shopCommandRect, shopItemRect = Scene.GetShopRects()
    self._shopMoveEnabledBeforeOpen = true
    self._windowShop = WindowShop.new(self.player, shopCommandRect, shopItemRect, function ()
        self:_onShopClose()
    end)
    self._attrShopMoveEnabledBeforeOpen = true
    self._windowAttrShop = WindowAttrShop.new(self.player, function ()
        self:_onAttrShopClose()
    end)
    self._enemyBookMoveEnabledBeforeOpen = true
    self._windowEnemyBook = WindowEnemyBook.new(
        Scene.GetEnemyBookRect(), self.player,
        function ()
            self:_onEnemyBookClose()
        end,
        function (entry)
            self:_onEnemyBookConfirm(entry)
        end
    )
    self._windowEnemyEncyclopedia = WindowEnemyEncyclopedia.new(Scene.GetEnemyEncyclopediaRect(), function ()
        self:_onEnemyEncyclopediaClose()
    end)
    self._floorTeleporterMoveEnabledBeforeOpen = true
    local floorListRect, floorPreviewRect = WindowFloorTeleporter.GetDefaultFloorTeleporterRects()
    self._windowFloorTeleporter = WindowFloorTeleporter.new(
        self.inst, floorListRect, floorPreviewRect,
        function (mapKey, telepoint, previewSize, previewScale, showTelepointMarker)
            return self:_buildFloorMapPreview(mapKey, telepoint, previewSize, previewScale, showTelepointMarker)
        end,
        function (mapKey, telepoint)
            self:_onFloorTeleporterConfirm(mapKey, telepoint)
        end,
        function ()
            self:_onFloorTeleporterClose()
        end,
        function (mapKey, telepoint)
            return self:_getFloorTelepointTag(mapKey, telepoint)
        end,
        function (mapKey)
            return self._mapBuilder:resolveMapPath(mapKey, self:_getCurrentRegionMap())
        end,
        function ()
            self._mapBuilder:clearFloorMapPreviewCache()
        end
    )
    self._windowSaveLoad = WindowSaveLoad.new(
        nil, nil, nil, false,
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
        equipSlot = self._windowEquipSlot,
        equipSelect = self._windowEquipSelect,
        equipStatus = self._windowEquipStatus,
        saveLoad = self._windowSaveLoad,
        config = self._configWindow
    })
    self._blockingWindows = {
        self._windowShop, self._windowAttrShop, self._windowEnemyBook, self._windowEnemyEncyclopedia,
        self._windowFloorTeleporter
    }
    self._windowMenu:setMoveRestoreGuard(function ()
        return self:_canRestoreMoveAfterMenuClose()
    end)
    self:_registerFocusGroups()
    self._regionTitleUI = RegionTitleUI.new(GlobalSystem.getGameSize())
    self._regionTitleUI:prepare()
    self._regionTitleText = self._regionTitleUI:getText()
    local tabWindow = assert(self._windowSaveLoad:getTabWindow(), "Map save tab window is missing")
    loadUiControls(
        uiManager, self._playerHUD, self._messageWindow, self._windowMenu, self._windowItem, self._windowEquipSlot,
        self._windowEquipSelect, self._windowEquipStatus, self._windowShop:getCommandWindow(),
        self._windowShop:getItemWindow(), self._windowAttrShop:getSelectable(), self._windowEnemyBook,
        self._windowEnemyEncyclopedia, self._windowFloorTeleporter:getCommandWindow(),
        self._windowFloorTeleporter:getPreviewWindow(), tabWindow, self._windowSaveLoad:getSlotWindow(),
        self._windowSaveLoad:getDetailWindow(), self._configWindow
    )
    self._localeChangedToken = Engine.subscribe(EventKeys.LocaleChanged, function ()
        local scene = sceneRef[1]
        if scene ~= nil then
            scene:_refreshMapUiLocale()
        end
    end)

    self._windowMenu:close()
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
    local startMap = self.inst._cachedMap or GameSystem.GetStartMap()
    self:gotoMapAndPos(startMap, nil, true)
end

function Scene:_registerFocusGroups()
    local uiManager = assert(self:getUIManager(), "Scene map UI manager is unavailable")
    local menuGroup = createSingleControlFocusGroup("menu", self._windowMenu)
    local itemGroup = createSingleControlFocusGroup("item", self._windowItem)
    itemGroup:setNeighbor(Direction.LEFT, menuGroup)

    local equipSlotGroup = createSingleControlFocusGroup("equip-slot", self._windowEquipSlot)
    local equipSelectGroup = createSingleControlFocusGroup("equip-select", self._windowEquipSelect)
    equipSlotGroup:setNeighbor(Direction.LEFT, menuGroup)
    equipSlotGroup:setNeighbor(Direction.RIGHT, FocusNeighbor.new(equipSelectGroup, FocusTransition.EXPLICIT))
    equipSelectGroup:setNeighbor(Direction.LEFT, FocusNeighbor.new(equipSlotGroup, FocusTransition.EXPLICIT))

    local shopCommandWindow = self._windowShop:getCommandWindow()
    local shopItemWindow = self._windowShop:getItemWindow()
    local shopCommandGroup = createSingleControlFocusGroup("shop-command", shopCommandWindow)
    local shopItemGroup = createSingleControlFocusGroup("shop-item", shopItemWindow)
    shopCommandGroup:setNeighbor(Direction.DOWN, FocusNeighbor.new(shopItemGroup, FocusTransition.EXPLICIT))
    shopItemGroup:setNeighbor(Direction.UP, FocusNeighbor.new(shopCommandGroup, FocusTransition.EXPLICIT))

    local floorCommandWindow = self._windowFloorTeleporter:getCommandWindow()
    local floorPreviewWindow = self._windowFloorTeleporter:getPreviewWindow()
    local floorCommandGroup = createSingleControlFocusGroup("floor-command", floorCommandWindow)
    local floorPreviewGroup = createSingleControlFocusGroup("floor-preview", floorPreviewWindow)
    floorCommandGroup:setNeighbor(Direction.RIGHT, FocusNeighbor.new(floorPreviewGroup, FocusTransition.EXPLICIT))
    floorPreviewGroup:setNeighbor(Direction.LEFT, FocusNeighbor.new(floorCommandGroup, FocusTransition.EXPLICIT))

    local saveSlotWindow = self._windowSaveLoad:getSlotWindow()
    local saveSlotGroup = createSingleControlFocusGroup("save-slot", saveSlotWindow)
    saveSlotGroup:setNeighbor(Direction.LEFT, menuGroup)

    local groups = {
        menuGroup, itemGroup, equipSlotGroup, equipSelectGroup, shopCommandGroup, shopItemGroup, floorCommandGroup,
        floorPreviewGroup, saveSlotGroup
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
    if self._gameMap ~= nil then
        self._gameMap:disposeStreaming()
    end
    ManagerFunctions.stopVoice()
    if self._localeChangedToken ~= nil then
        Engine.unsubscribe(self._localeChangedToken)
        self._localeChangedToken = nil
    end
    self._dialogueLocaleSource = nil
    self._mapAudio:stopMapAudio()
    self._windowSaveLoad:dispose()
    self._configWindow:dispose()
    self._regionTitleUI:dispose()
end

function Scene:_refreshMapUiLocale()
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
    self._windowEquipSlot:refreshLocale()
    self._windowShop:getCommandWindow():refreshRows()
    self._windowAttrShop:refreshLocale()
    self._windowEnemyBook:refreshLocale()
    self._windowEnemyEncyclopedia:refreshLocale()
    self._windowFloorTeleporter:refreshLocale()
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
        local sceneType = hotKeyConfig.Scene
        if Class.isInstance(self, sceneType) then
            local casual = bool(hotKeyConfig.Filter) and table.contains(hotKeyConfig.Filter, "casual")
            if casual then
                local functionWhenPressed = hotKeyConfig.FunctionWhenPressed
                if Scene.IsHotKeySceneMethod(sceneType, functionWhenPressed) and Input.getKeyPressed(key, false) then
                    functionWhenPressed(self)
                    Input.getKeyPressed(key, true)
                end
                local functionWhenReleased = hotKeyConfig.FunctionWhenReleased
                if Scene.IsHotKeySceneMethod(sceneType, functionWhenReleased) and Input.getKeyReleased(key, false) then
                    functionWhenReleased(self)
                    Input.getKeyReleased(key, true)
                end
            end
        end
    end
end

---@param sceneType Class.ClassType<any>
---@param function_ function | nil
---@return boolean
function Scene.IsHotKeySceneMethod(sceneType, function_)
    if not bool(function_) then
        return false
    end
    for _, classType in ipairs(Class.getMro(sceneType)) do
        for _, value in pairs(classType) do
            if value == function_ then
                return true
            end
        end
    end
    return false
end

function Scene:onTick(deltaTime)
    WorldGameMap.StepGarbageCollector()
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
    if self:_tryConfirmMessageByScreenClick() then
        self._mapClickMoveBlockedUntilLateTick = false
    elseif self._mapClickMoveBlockedUntilLateTick or self:_isMapClickMoveBlocked() then
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
        gameMap = self._mapBuilder:generateGameMap(mapData)
    end
    if self._gameMap ~= nil then
        self._gameMap:disposeStreaming()
    end
    self._gameMap = gameMap
    gameMap:setScene(self)
    if not gameMap:isWorldMap() then
        gameMap:applyTerrainDestructions(self.inst:getTerrainDestructions(mapFile))
        self._mapBuilder:applyAddedActors(gameMap, self.inst:getAddedActors(mapFile))
        gameMap:applyActorPositions(self.inst:getActorPositions(mapFile))
        gameMap:removeActorsByTags(self.inst:getDestroyedActors(mapFile))
    end
    if gameMap:isWorldMap() then
        gameMap:setPlayer(self.player)
        gameMap:spawnActor(self.player, "default")
    else
        gameMap:spawnActor(self.player, "default")
        gameMap:setPlayer(self.player)
    end
    self._worldEnvironmentKey = nil
    self._worldAmbientStartColour = nil
    self._worldAmbientTargetColour = nil
    self._worldAmbientTransitionElapsed = 0
    if not gameMap:isWorldMap() then
        ---@cast mapData Source.SceneComponents.MapData
        self._mapAudio:playMapAudio(mapData)
        GlobalSystem.clearFog()
        GlobalSystem.applyFogFromMapData({
            fog = mapData.fog,
            fogPower = mapData.fogPower,
            fogOx = mapData.fogOx,
            fogOy = mapData.fogOy,
            fogDistort = mapData.fogDistort
        })
    end
    self:_updateCurrentRegion(mapFile)
    Logging.info("Loaded map %s in %.3fs", mapFile, perfCounter() - startTime)
    if gameMap:isWorldMap() then
        local worldMap = gameMap
        ---@cast worldMap Global.WorldGameMap.WorldGameMap
        worldMap:activateStreamingGarbageCollector()
    end
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
    self._worldAmbientTransitionElapsed = Engine.Clamp(
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
    if sourceSize.x == gameSize.x and sourceSize.y == gameSize.y then
        GameSystem.SetSavedScreenImage(sourceTexture:copyToImage())
        return
    end
    local scaled = sf.RenderTexture.new(gameSize)
    scaled:clear(sf.Color.Transparent)
    local sprite = sf.Sprite.new(sourceTexture)
    sprite:setScale(sf.Vector2f.new(gameSize.x / sourceSize.x, gameSize.y / sourceSize.y))
    scaled:draw(sprite)
    scaled:display()
    GameSystem.SetSavedScreenImage(scaled:getTexture():copyToImage())
end

---@return boolean
function Scene.IsMenuOpenTriggered()
    return Input.isMouseButtonTriggered(sf.Mouse.Button.Right, true)
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

---@return boolean
function Scene:_tryConfirmMessageByScreenClick()
    if not self._messageWindow:isAwaitingMessageConfirm() then
        return false
    end
    local clicked = Input.isMouseButtonTriggered(sf.Mouse.Button.Left, false)
    local touched = Input.isTouchTap(false)
    if not clicked and not touched then
        return false
    end
    self._messageWindow:confirmMessage()
    Scene.ConsumeMapClickMoveInput()
    return true
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

---@param mapKey    string
---@param telepoint sf.Vector2u
---@return string | nil
function Scene:_getFloorTelepointTag(mapKey, telepoint)
    return self._mapBuilder:getFloorTelepointTag(self:_getCurrentRegionMap(), mapKey, telepoint)
end

function Scene:resolveRegionMapPath(mapKey)
    return self._mapBuilder:resolveRegionMapPath(mapKey, self:_getCurrentRegionMap())
end

---@return string
function Scene:_getCurrentRegionMap()
    return self._cachedMapFile or self.inst._cachedMap or GameSystem.GetStartMap()
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
    ---@type table<string, string[]>
    local RegionDict = require("Source.Configs.RegionDict")

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
    RegionTitleUI.Publish({
        region = region
    })
end

---@param deltaTime number
function Scene:_updateRegionTitle(deltaTime)
    self._regionTitleUI:update(deltaTime)
end

function Scene:getGameMap()
    return SceneMapInteractions.getGameMap(self)
end

function Scene:showMessage(name, message, refActor, localeArgs)
    return SceneMapInteractions.showMessage(self, name, message, refActor, localeArgs)
end

function Scene:showSelection(name, options, refActor, allowCancel, localeArgs)
    return SceneMapInteractions.showSelection(self, name, options, refActor, allowCancel, localeArgs)
end

function Scene:applyLoadedGame(inst)
    return SceneMapInteractions.applyLoadedGame(self, inst)
end

function Scene:_rebindPlayerToUI()
    return SceneMapInteractions._rebindPlayerToUI(self)
end

function Scene:showEnemyBook()
    return SceneMapInteractions.showEnemyBook(self)
end

function Scene:showFloorTeleporter()
    return SceneMapInteractions.showFloorTeleporter(self)
end

function Scene:openMenu()
    return SceneMapInteractions.openMenu(self)
end

function Scene:openShop(buyItemIDs, canSell)
    return SceneMapInteractions.openShop(self, buyItemIDs, canSell)
end

function Scene:openAttrShop(actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName)
    return SceneMapInteractions.openAttrShop(
        self, actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName
    )
end

function Scene:_onShopClose()
    return SceneMapInteractions._onShopClose(self)
end

function Scene:_onAttrShopClose()
    return SceneMapInteractions._onAttrShopClose(self)
end

function Scene:_onEnemyBookClose()
    return SceneMapInteractions._onEnemyBookClose(self)
end

function Scene:_onEnemyBookConfirm(entry)
    return SceneMapInteractions._onEnemyBookConfirm(self, entry)
end

function Scene:_onEnemyEncyclopediaClose()
    return SceneMapInteractions._onEnemyEncyclopediaClose(self)
end

function Scene:_onFloorTeleporterClose()
    return SceneMapInteractions._onFloorTeleporterClose(self)
end

function Scene:_onFloorTeleporterConfirm(mapKey, telepoint)
    return SceneMapInteractions._onFloorTeleporterConfirm(self, mapKey, telepoint)
end

function Scene:_recordCurrentFloorTelepoint()
    return SceneMapInteractions._recordCurrentFloorTelepoint(self)
end

function Scene:_findNearestFloorTelepoint()
    return SceneMapInteractions._findNearestFloorTelepoint(self)
end

function Scene.GetShopRects()
    return SceneMapInteractions.GetShopRects()
end

function Scene.GetAttrShopRect()
    return SceneMapInteractions.GetAttrShopRect()
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

function Scene.GetEnemyBookRect()
    return SceneMapInteractions.GetEnemyBookRect()
end

function Scene.GetEnemyEncyclopediaRect()
    return SceneMapInteractions.GetEnemyEncyclopediaRect()
end

function Scene:_canRestoreMoveAfterMenuClose()
    return SceneMapInteractions._canRestoreMoveAfterMenuClose(self)
end

function Scene:_hasVisibleBlockingWindow()
    return SceneMapInteractions._hasVisibleBlockingWindow(self)
end

function Scene:_blockMapInput(frames)
    return SceneMapInteractions._blockMapInput(self, frames)
end

function Scene:requestFloorTransfer(targetMap, anchorPos, moveEnabled)
    return SceneMapInteractions.requestFloorTransfer(self, targetMap, anchorPos, moveEnabled)
end

function Scene:_processPendingFloorTransfer()
    return SceneMapInteractions._processPendingFloorTransfer(self)
end

function Scene:_cancelFloorTransfer(moveEnabled)
    return SceneMapInteractions._cancelFloorTransfer(self, moveEnabled)
end

function Scene:_applyMapDestination(targetMap, targetPosition, blockTransition)
    return SceneMapInteractions._applyMapDestination(self, targetMap, targetPosition, blockTransition)
end

function Scene:_queueWorldTransfer(targetMap, targetPosition)
    return SceneMapInteractions._queueWorldTransfer(self, targetMap, targetPosition)
end

function Scene:_processPendingWorldTransfer()
    return SceneMapInteractions._processPendingWorldTransfer(self)
end

function Scene:_getSaveSource()
    return SceneMapInteractions._getSaveSource(self)
end

function Scene:_onSaveLoadClose(reason)
    return SceneMapInteractions._onSaveLoadClose(self, reason)
end

function Scene:_onConfigClose()
    return SceneMapInteractions._onConfigClose(self)
end

function Scene:gotoMapAndPos(mapPath, pos, blockTransition)
    return SceneMapInteractions.gotoMapAndPos(self, mapPath, pos, blockTransition)
end

function Scene:tryCenterSymmetricTeleport()
    return SceneMapInteractions.tryCenterSymmetricTeleport(self)
end

function Scene:tryAdjacentFloorSamePos(step)
    return SceneMapInteractions.tryAdjacentFloorSamePos(self, step)
end

function Scene:_isMapPositionPassable(mapPath, actor, position)
    return SceneMapInteractions._isMapPositionPassable(self, mapPath, actor, position)
end

function Scene:recordAddedActor(actor)
    return SceneMapInteractions.recordAddedActor(self, actor)
end

function Scene:recordActorPosition(actor, position)
    return SceneMapInteractions.recordActorPosition(self, actor, position)
end

function Scene:recordDestroyedActor(actor)
    return SceneMapInteractions.recordDestroyedActor(self, actor)
end

function Scene:recordDestroyedActorTag(actorTag)
    return SceneMapInteractions.recordDestroyedActorTag(self, actorTag)
end

function Scene:recordTerrainDestructions(layerName, positions)
    return SceneMapInteractions.recordTerrainDestructions(self, layerName, positions)
end

return class(Scene, SceneBase)
