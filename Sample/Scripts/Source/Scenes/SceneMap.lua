local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local Logging = require("Global.Utils.Logging")
local GameSystem = require("Source.System")
local Data = require("Source.Data")
local LocaleCore = require("Source.Locale.Core")
local GeneralEnum = require("Source.Configs.GeneralEnum")
local MapPath = require("Source.MapPath")
local SceneMapAudioController = require("Source.SceneComponents.MapAudio")
local SceneMapBuilder = require("Source.SceneComponents.MapBuilder")
local RegionTitleUI = require("Source.UI.RegionTitle")
local UiLayout = require("Source.UI.UiLayout")
local PlayerAttrHUD = require("Source.Windows.HUDPlayerAttr")
local EquipWindows = require("Source.Windows.WindowEquip")
local WindowAttrShopModule = require("Source.Windows.WindowAttrShop")
local WindowEnemyBookModule = require("Source.Windows.WindowEnemyBook")
local WindowEnemyEncyclopedia = require("Source.Windows.WindowEnemyEncyclopedia")
local FloorWindows = require("Source.Windows.WindowFloorTeleporter")
local WindowItem = require("Source.Windows.WindowItem")
local WindowMenu = require("Source.Windows.WindowMenu")
local WindowMessage = require("Source.Windows.WindowMessage")
local WindowSaveLoadModule = require("Source.Windows.WindowSaveLoad")
local WindowShop = require("Source.Windows.WindowShop")

local Input = Engine.Input
local Node = Engine.Node
local Direction = Engine.FocusDirection
local FocusGroup = GlobalCore.FocusGroup
local FocusNeighbor = GlobalCore.FocusNeighbor
local FocusTransition = GlobalCore.FocusTransition
local SceneBase = GlobalCore.SceneBase
local GlobalSystem = GlobalCore.System
local WindowEquipSlot = EquipWindows.WindowEquipSlot
local WindowEquipSelect = EquipWindows.WindowEquipSelect
local WindowEquipStatus = EquipWindows.WindowEquipStatus
local WindowAttrShop = WindowAttrShopModule.WindowAttrShop
local WindowEnemyBook = WindowEnemyBookModule.WindowEnemyBook
local WindowFloorTeleporter = FloorWindows.WindowFloorTeleporter
local WindowSaveLoad = WindowSaveLoadModule.WindowSaveLoad
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local ENEMY_BOOK_SIZE = 352
local ENEMY_ENCYCLOPEDIA_WIDTH = 640
local ENEMY_ENCYCLOPEDIA_HEIGHT = 480
local EQUIP_SLOT_WIDTH = 196
local EQUIP_SLOT_HEIGHT = 160
local EQUIP_SELECT_HEIGHT = 192
local EQUIP_STATUS_X = 384
local MAP_TRANSITION_NAME = ""
local MAP_TRANSITION_TIME = 0.5
local ENEMY_BOOK_ITEM_ID = GeneralEnum.Item.EnemyBook
---@cast ENEMY_BOOK_ITEM_ID string
local FLOOR_TELEPORTER_ITEM_ID = GeneralEnum.Item.Teleport
---@cast FLOOR_TELEPORTER_ITEM_ID string

---@param name string
---@param control any
---@return GlobalCore.FocusGroup
local function createSingleControlFocusGroup(name, control)
    ---@cast control Engine.FunctionalBase
    return FocusGroup.new(name, { control }, control)
end

---@param uiManager GlobalCore.UIManager
---@param control any
local function loadUiControl(uiManager, control)
    ---@cast control Engine.ControlBase
    uiManager:loadUI(control)
end

---@class Source.Scenes.SceneMap.SceneMap: GlobalCore.SceneBase
local Scene = {}

---@diagnostic disable-next-line: unused
function Scene:onEnter()
    GlobalSystem.setTransition()
end

function Scene:setInst(inst)
    self.inst = inst
end

function Scene:onCreate()
    local uiManager = self:getUIManager()
    ---@cast uiManager GlobalCore.UIManager
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
    local floorListRect, floorPreviewRect = FloorWindows.GetDefaultFloorTeleporterRects()
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
    self._windowMenu = WindowMenu.new(self.player, {
        item = self._windowItem,
        equipSlot = self._windowEquipSlot,
        equipSelect = self._windowEquipSelect,
        equipStatus = self._windowEquipStatus,
        saveLoad = self._windowSaveLoad
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
    local uiWindows = {
        self._playerHUD, self._messageWindow, self._windowMenu, self._windowItem, self._windowEquipSlot,
        self._windowEquipSelect, self._windowEquipStatus, self._windowShop:getCommandWindow(),
        self._windowShop:getItemWindow(), self._windowAttrShop:getSelectable(), self._windowEnemyBook,
        self._windowEnemyEncyclopedia, self._windowFloorTeleporter:getCommandWindow(),
        self._windowFloorTeleporter:getPreviewWindow(), self._windowSaveLoad:getCommandWindow(),
        self._windowSaveLoad:getSlotWindow(), self._windowSaveLoad:getDetailWindow()
    }
    for _, window in ipairs(uiWindows) do
        loadUiControl(uiManager, window)
    end

    self._windowMenu:close()
    self._gameMap = nil
    self._cachedMapFile = nil
    self._currentRegion = nil
    self._mapClickMoveBlockedUntilLateTick = false
    self._mapInputBlockFrames = 0
    self._pendingMenuOpen = false
    self._pendingFloorTransfer = nil
    self._mapTransferInProgress = false
    local startMap = self.inst._cachedMap or GameSystem.getStartMap()
    self:gotoMapAndPos(startMap, nil, true)
end

function Scene:_registerFocusGroups()
    local uiManager = self:getUIManager()
    ---@cast uiManager GlobalCore.UIManager
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

    local saveCommandWindow = assert(self._windowSaveLoad:getCommandWindow())
    local saveCommandGroup = createSingleControlFocusGroup("save-command", saveCommandWindow)
    local saveSlotWindow = self._windowSaveLoad:getSlotWindow()
    local saveSlotGroup = createSingleControlFocusGroup("save-slot", saveSlotWindow)
    saveCommandGroup:setNeighbor(Direction.DOWN, FocusNeighbor.new(saveSlotGroup, FocusTransition.EXPLICIT))
    saveCommandGroup:setNeighbor(Direction.LEFT, menuGroup)
    saveSlotGroup:setNeighbor(Direction.UP, FocusNeighbor.new(saveCommandGroup, FocusTransition.EXPLICIT))
    saveSlotGroup:setNeighbor(Direction.LEFT, menuGroup)

    local groups = {
        menuGroup, itemGroup, equipSlotGroup, equipSelectGroup, shopCommandGroup, shopItemGroup, floorCommandGroup,
        floorPreviewGroup, saveCommandGroup, saveSlotGroup
    }
    for _, group in ipairs(groups) do
        uiManager:registerFocusGroup(group)
    end
end

function Scene:onQuit()
    self._mapAudio:stopMapAudio()
    GlobalSystem.clearWeather()
    GlobalSystem.clearFog()
end

function Scene:onDestroy()
    self._mapAudio:stopMapAudio()
    self._regionTitleUI:dispose()
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
    local HotKey = require("Source.Config.HotKey")

    for key, hotKeyConfig in pairs(HotKey) do
        local sceneType = hotKeyConfig.Scene
        if Class.isInstance(self, sceneType) then
            local casual = false
            if bool(hotKeyConfig.Filter) then
                for _, filter in ipairs(hotKeyConfig.Filter) do
                    if filter == "casual" then
                        casual = true
                        break
                    end
                end
            end
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

---@param sceneType table
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
    if self._mapTransferInProgress then
        return super(Scene, self).onTick(deltaTime)
    end
    self._mapClickMoveBlockedUntilLateTick = self:_isMapClickMoveBlocked()
    local gameMap = self:getGameMap()
    gameMap:onTick(deltaTime)
    gameMap:getTilemap():updateAutoTileAnimation(deltaTime)
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

function Scene:loadMap(mapPath)
    Logging.info("Loading map: %s", tostring(mapPath))
    local startTime = perfCounter()
    local mapFile, mapData = self._mapBuilder:loadMapData(mapPath, self:_getCurrentRegionMap())
    local gameMap = self._mapBuilder:generateGameMap(mapData)
    self._gameMap = gameMap
    gameMap:setScene(self)
    gameMap:setPersistentMapPath(mapFile)
    gameMap:applyTerrainDestructions(self.inst:getTerrainDestructions(mapFile))
    self._mapBuilder:applyAddedActors(gameMap, self.inst:getAddedActors(mapFile))
    gameMap:applyActorPositions(self.inst:getActorPositions(mapFile))
    gameMap:removeActorsByTags(self.inst:getDestroyedActors(mapFile))
    gameMap:spawnActor(self.player, "default")
    gameMap:setPlayer(self.player)
    self._mapAudio:playMapAudio(mapData)
    GlobalSystem.clearFog()
    GlobalSystem.applyFogFromMapData({
        fog = mapData.fog,
        fogPower = mapData.fogPower,
        fogOx = mapData.fogOx,
        fogOy = mapData.fogOy,
        fogDistort = mapData.fogDistort
    })
    self:_updateCurrentRegion(mapFile)
    Logging.info("Loaded map %s in %.3fs", mapFile, perfCounter() - startTime)
    return mapFile
end

function Scene:getGameMap()
    assert(self._gameMap ~= nil, "Scene map is not loaded")
    return self._gameMap
end

function Scene:showMessage(name, message, refActor)
    local refPosition = nil
    if refActor ~= nil then
        local gameMap = self:getGameMap()
        local camera = assert(gameMap:getCamera())
        local viewPosition = assert(camera:getViewPosition())
        refPosition = refActor:getPosition() - viewPosition + gameMap:getMapViewOffset()
        ---@cast refPosition sf.Vector2f
    end
    local originMoveEnabled = self.player:getMoveEnabled()
    local restored = false
    local function restoreMove()
        if restored then
            return
        end
        restored = true
        self.player:setMoveEnabled(originMoveEnabled)
        self:_blockMapInput(2)
    end
    self.player:setMoveEnabled(false)
    local localVars = Scene.GetDialogueLocalVars(Scene.showMessage)
    self._messageWindow:setMessage(
        refPosition, self:_formatDialogueText(name, localVars), self:_formatDialogueText(message, localVars), true,
        restoreMove
    )
    return function ()
        if self._messageWindow:isInDialogue() then
            return false
        end
        restoreMove()
        return true
    end
end

function Scene:showSelection(name, options, refActor, allowCancel)
    if allowCancel == nil then
        allowCancel = true
    end
    local refPosition = nil
    if refActor ~= nil then
        local gameMap = self:getGameMap()
        local camera = assert(gameMap:getCamera())
        local viewPosition = assert(camera:getViewPosition())
        refPosition = refActor:getPosition() - viewPosition + gameMap:getMapViewOffset()
        ---@cast refPosition sf.Vector2f
    end
    local originMoveEnabled = self.player:getMoveEnabled()
    local restored = false
    local function restoreMove()
        if restored then
            return
        end
        restored = true
        self.player:setMoveEnabled(originMoveEnabled)
        self:_blockMapInput(2)
    end
    self.player:setMoveEnabled(false)
    local localVars = Scene.GetDialogueLocalVars(Scene.showSelection)
    local formattedOptions = {}
    for _, option in ipairs(options) do
        formattedOptions[#formattedOptions + 1] = self:_formatDialogueText(option, localVars)
    end
    self._messageWindow:setMessage(
        refPosition, self:_formatDialogueText(name, localVars), formattedOptions, allowCancel, restoreMove
    )
    return function ()
        local selectionResult = self._messageWindow:getSelectionResult()
        if selectionResult == nil then
            return nil
        end
        restoreMove()
        return selectionResult
    end
end

function Scene:applyLoadedGame(inst)
    self.inst = inst
    self.player = inst:getPlayer()
    self:_rebindPlayerToUI()
    local mapPath = inst._cachedMap or GameSystem.getStartMap()
    local position = self.player:getMapPosition()
    self._cachedMapFile = nil
    self._currentRegion = nil
    self:gotoMapAndPos(mapPath, position)
end

-- Point HUD and sub-windows at the current player after load.
function Scene:_rebindPlayerToUI()
    self._windowItem._player = self.player
    self._windowEquipSlot._player = self.player
    self._windowEquipSelect._player = self.player
    self._windowEquipStatus:setPlayer(self.player)
    self._windowMenu._player = self.player
    self._windowShop:setPlayer(self.player)
    self._windowAttrShop:setPlayer(self.player)
    self._windowEnemyBook:setPlayer(self.player)
    self._playerHUD._player = self.player
end

function Scene:showEnemyBook()
    if (not self:_canOpenMenu() and not self:_canOpenItemOverlay()) or not self.player:hasItem(ENEMY_BOOK_ITEM_ID) then
        return
    end
    if not self._windowEnemyBook:getVisible() then
        self._enemyBookMoveEnabledBeforeOpen = self._windowMenu:isBlocking() or self.player:getMoveEnabled()
        self.player:setMoveEnabled(false)
    end
    self._windowEnemyBook:open(self:getGameMap())
    self:_blockMapInput(2)
end

function Scene:showFloorTeleporter()
    if (not self:_canOpenMenu() and not self:_canOpenItemOverlay()) or not self.player:hasItem(FLOOR_TELEPORTER_ITEM_ID)
        or not self:_canUseFloorTeleporterByAsideConstraint() then
        return
    end
    if not self._windowFloorTeleporter:getVisible() then
        self._floorTeleporterMoveEnabledBeforeOpen = self._windowMenu:isBlocking() or self.player:getMoveEnabled()
        self.player:setMoveEnabled(false)
    end
    self:_recordCurrentFloorTelepoint()
    self._windowFloorTeleporter:open(self.inst)
    self:_blockMapInput(2)
end

---@return boolean
function Scene:_canUseFloorTeleporterByAsideConstraint()
    local itemData = Data.getGeneralItemData(FLOOR_TELEPORTER_ITEM_ID)
    local kwargs = itemData.kwargs
    if kwargs == nil then
        return true
    end
    local flag = rawget(kwargs, "CanOnlyUseAsideTeleporter") or false
    if type(flag) == "string" then
        flag = Engine.evalDataExpression(flag)
    end
    if not bool(flag) then
        return true
    end
    local gameMap = self:getGameMap()
    local player = gameMap:getPlayer()
    if player == nil then
        return false
    end
    local Teleporter = require("Source.Teleporter")

    return Teleporter.isAsideOrOverlapping(gameMap:getAllActors(), player:getMapPosition())
end

function Scene:openMenu()
    if self:_canOpenMenu() then
        self._pendingMenuOpen = true
    end
end

function Scene:openShop(buyItemIDs, canSell)
    self._shopMoveEnabledBeforeOpen = self._windowMenu:isBlocking() or self.player:getMoveEnabled()
    self.player:setMoveEnabled(false)
    self._windowShop:open(buyItemIDs, canSell)
    return function ()
        return not self._windowShop:getVisible()
    end
end

function Scene:openAttrShop(actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName)
    self._attrShopMoveEnabledBeforeOpen = self._windowMenu:isBlocking() or self.player:getMoveEnabled()
    self.player:setMoveEnabled(false)
    self._windowAttrShop:open(
        actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName, Scene.GetAttrShopRect()
    )
    return function ()
        return not self._windowAttrShop:getVisible()
    end
end

function Scene:_onShopClose()
    self.player:setMoveEnabled(self._shopMoveEnabledBeforeOpen)
end

function Scene:_onAttrShopClose()
    self.player:setMoveEnabled(self._attrShopMoveEnabledBeforeOpen)
    self:_blockMapInput(1)
end

function Scene:_onEnemyBookClose()
    self.player:setMoveEnabled(self._enemyBookMoveEnabledBeforeOpen)
    self:_blockMapInput(1)
end

---@param entry table
function Scene:_onEnemyBookConfirm(entry)
    self._windowEnemyEncyclopedia:open(entry)
    self:_blockMapInput(2)
end

function Scene:_onEnemyEncyclopediaClose()
    self.player:setMoveEnabled(self._enemyBookMoveEnabledBeforeOpen)
    self:_blockMapInput(1)
end

function Scene:_onFloorTeleporterClose()
    self.player:setMoveEnabled(self._floorTeleporterMoveEnabledBeforeOpen)
    self:_blockMapInput(1)
end

---@param mapKey    string
---@param telepoint sf.Vector2u
function Scene:_onFloorTeleporterConfirm(mapKey, telepoint)
    local targetMap = self:resolveRegionMapPath(mapKey)
    self._windowFloorTeleporter:close()
    self:gotoMapAndPos(targetMap, telepoint)
    self.player:setMoveEnabled(self._floorTeleporterMoveEnabledBeforeOpen)
    self:_blockMapInput(2)
end

function Scene:_recordCurrentFloorTelepoint()
    if self._gameMap == nil or self._cachedMapFile == nil then
        return
    end
    local mapFile = self._cachedMapFile
    local telepoint = self:_findNearestFloorTelepoint()
    if telepoint == nil then
        return
    end
    self.inst:recordTelepoint(mapFile, sf.Vector2u.new(telepoint.x, telepoint.y))
end

---@return sf.Vector2i | nil
function Scene:_findNearestFloorTelepoint()
    local gameMap = self:getGameMap()
    local player = gameMap:getPlayer()
    if player == nil then
        return nil
    end
    local Teleporter = require("Source.Teleporter")

    local nearest = Teleporter._findNearestTeleporter(gameMap:getAllActors(), player:getMapPosition())
    return nearest ~= nil and nearest:getTeleportPosition() or nil
end

---@return sf.IntRect, sf.IntRect
function Scene.GetShopRects()
    return WindowShop.GetDefaultRects()
end

---@return sf.IntRect
function Scene.GetAttrShopRect()
    return WindowAttrShop.GetDefaultRect()
end

---@param nodeFunction function
---@return table<string, any>
function Scene.GetDialogueLocalVars(nodeFunction)
    local refLocal = Node.getRefLocal(nodeFunction)
    if not bool(refLocal) or refLocal.__activeNodeFunction__ ~= nodeFunction then
        return {}
    end
    local result = {}
    for key, value in pairs(refLocal) do
        if type(key) == "string" and key:sub(1, 2) ~= "__" then
            result[key] = value
        end
    end
    return result
end

---@param text      string
---@param localVars table<string, any>
---@return string
function Scene:_formatDialogueText(text, localVars)
    if type(text) ~= "string" then
        return tostring(text)
    end
    text = LOC(text)
    text = Engine.ApplyStringMappingFormat(text, localVars)
    if self.inst ~= nil then
        text = Engine.ApplyStringMappingFormat(text, self.inst:getVariables())
    end
    return text
end

---@return sf.IntRect
function Scene.GetEnemyBookRect()
    return UiLayout.GetCenteredRect(ENEMY_BOOK_SIZE, ENEMY_BOOK_SIZE)
end

---@return sf.IntRect
function Scene.GetEnemyEncyclopediaRect()
    return UiLayout.GetCenteredRect(ENEMY_ENCYCLOPEDIA_WIDTH, ENEMY_ENCYCLOPEDIA_HEIGHT)
end

---@return boolean
function Scene:_canRestoreMoveAfterMenuClose()
    return not self:_hasVisibleBlockingWindow()
end

---@return boolean
function Scene:_hasVisibleBlockingWindow()
    for _, window in ipairs(self._blockingWindows) do
        if window:getVisible() then
            return true
        end
    end
    return false
end

---@param frames integer
function Scene:_blockMapInput(frames)
    frames = frames or 1
    self._mapInputBlockFrames = math.max(self._mapInputBlockFrames, frames)
end

function Scene:requestFloorTransfer(targetMap, anchorPos, moveEnabled)
    if self._pendingFloorTransfer ~= nil then
        return false
    end
    self._pendingFloorTransfer = { targetMap = targetMap, anchorPos = anchorPos, moveEnabled = moveEnabled }
    GlobalSystem.freezeTransitionBackground()
    return true
end

function Scene:_processPendingFloorTransfer()
    if self._pendingFloorTransfer == nil or not GlobalSystem.isTransitionBackgroundFrozen() then
        return
    end
    local transferData = self._pendingFloorTransfer
    self._pendingFloorTransfer = nil
    self._mapTransferInProgress = true
    local targetMap = transferData.targetMap
    local anchorPos = transferData.anchorPos
    local moveEnabled = bool(transferData.moveEnabled)
    self:gotoMapAndPos(targetMap, anchorPos, true)
    local targetGameMap = self:getGameMap()
    local targetPlayer = targetGameMap:getPlayer()
    if targetPlayer == nil then
        self:_cancelFloorTransfer(moveEnabled)
        self._mapTransferInProgress = false
        return
    end
    local Teleporter = require("Source.Teleporter")

    local targetTeleporter = Teleporter._findNearestTeleporter(
        targetGameMap:getAllActors(), targetPlayer:getMapPosition()
    )
    if targetTeleporter == nil then
        self:_cancelFloorTransfer(moveEnabled)
        self._mapTransferInProgress = false
        return
    end
    local targetPos = targetTeleporter:getTeleportPosition()
    self:gotoMapAndPos(targetMap, targetPos)
    if self._cachedMapFile ~= nil then
        self.inst:recordTelepoint(self._cachedMapFile, sf.Vector2u.new(targetPos.x, targetPos.y))
    end
    targetPlayer:setMoveEnabled(moveEnabled)
    self._mapTransferInProgress = false
end

---@param moveEnabled boolean
function Scene:_cancelFloorTransfer(moveEnabled)
    self.player:setMoveEnabled(moveEnabled)
    GlobalSystem.cancelTransitionBackgroundFreeze()
    GlobalSystem.cancelPendingTransition()
end

-- Provide the GameInstance to persist when saving from this scene.
---
--- - @return The current scene GameInstance.
---@return Source.GameInstance.GameInstance
function Scene:_getSaveSource()
    return self.inst
end

-- React to the save/load UI closing.
---
--- - @param reason One of ``"cancel"``, ``"saved"``, or ``"loaded"``.
---@param reason string
function Scene:_onSaveLoadClose(reason)
    if reason == "cancel" then
        self._windowMenu:onSaveLoadClose()
        return
    end
    self._windowMenu:close()
end

function Scene:gotoMapAndPos(mapPath, pos, blockTransition)
    local targetMap = mapPath
    if bool(mapPath) then
        targetMap = self._mapBuilder:resolveMapPath(mapPath, self:_getCurrentRegionMap())
    end
    if bool(targetMap) and self._cachedMapFile ~= targetMap then
        targetMap = self:loadMap(targetMap)
        self._cachedMapFile = targetMap
    end
    self.inst:applyMapInfo(targetMap, pos)
    if not blockTransition then
        GlobalSystem.requestTransition(MAP_TRANSITION_NAME, MAP_TRANSITION_TIME)
    end
end

function Scene:recordAddedActor(actor)
    local layerName = self:getGameMap():getActorLayer(actor)
    if layerName ~= nil then
        assert(self._cachedMapFile ~= nil, "Scene map path is not loaded")
        self.inst:recordAddedActor(self._cachedMapFile, actor, layerName)
    end
end

function Scene:recordActorPosition(actor, position)
    assert(self._cachedMapFile ~= nil, "Scene map path is not loaded")
    self.inst:recordActorPosition(self._cachedMapFile, actor, position)
end

function Scene:recordDestroyedActor(actor)
    assert(self._cachedMapFile ~= nil, "Scene map path is not loaded")
    self.inst:recordDestroyedActor(self._cachedMapFile, actor)
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
    GlobalSystem.setWindowMapView(gameMap:getMapViewOffset())
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
        GameSystem.setSavedScreenImage(nil)
        return
    end
    if sourceSize.x == gameSize.x and sourceSize.y == gameSize.y then
        GameSystem.setSavedScreenImage(sourceTexture:copyToImage())
        return
    end
    local scaled = sf.RenderTexture.new(gameSize)
    scaled:clear(sf.Color.Transparent)
    local sprite = sf.Sprite.new(sourceTexture)
    sprite:setScale(sf.Vector2f.new(gameSize.x / sourceSize.x, gameSize.y / sourceSize.y))
    scaled:draw(sprite)
    scaled:display()
    GameSystem.setSavedScreenImage(scaled:getTexture():copyToImage())
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
    return self._cachedMapFile or self.inst._cachedMap or GameSystem.getStartMap()
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
    local RegionDict = require("Source.Config.RegionDict")

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
    RegionTitleUI.publish({
        region = region
    })
end

---@param deltaTime number
function Scene:_updateRegionTitle(deltaTime)
    self._regionTitleUI:update(deltaTime)
end

return class(Scene, SceneBase)
