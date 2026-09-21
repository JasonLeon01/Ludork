local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local ConditionalActor = require("Source.ConditionalActor")
local GameSystem = require("Source.System")
local LocaleCore = require("Source.Locale.Core")
---@type { Item: Source.Configs.GeneralEnum.Item }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local Teleporter = require("Source.Teleporter")
local RegionDict = require("Source.Configs.RegionDict")
local MapConstants = require("Source.Configs.MapConstants")

local AudioManager = GlobalCore.AudioManager
local Save = require("Source.Save")
local WindowTransition = require("Source.UIBase.WindowTransition")
local WindowSaveSlot = require("Source.Windows.WindowSaveLoad.Slot")

local Transition = GlobalCore.Transition
local Node = Engine.Node
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local MAP_TRANSITION_NAME = ""
local MAP_TRANSITION_TIME = 0.5
local MAP_INPUT_BLOCK_FRAMES = 2
local WINDOW_CLOSE_INPUT_BLOCK_FRAMES = 1
local ENEMY_BOOK_ITEM_ID = GeneralEnum.Item.EnemyBook
local FLOOR_TELEPORTER_ITEM_ID = GeneralEnum.Item.Teleport

local Scene = {}

---@param player     Source.Player.Player
---@param blockInput fun()
---@return fun()
local function suspendPlayerMovement(player, blockInput)
    local moveEnabled = player:getMoveEnabled()
    local restored = false
    player:setMoveEnabled(false)
    return function ()
        if restored then
            return
        end
        restored = true
        player:setMoveEnabled(moveEnabled)
        blockInput()
    end
end

---@param scene        Source.Scenes.SceneMap.SceneMap
---@param name         string
---@param localeArgs   table<string, any> | nil
---@param nodeFunction function
---@return Source.Scenes.SceneMap.DialogueLocaleContext
local function createDialogueLocaleContext(scene, name, localeArgs, nodeFunction)
    local instanceVars = {}
    if scene.inst ~= nil then
        instanceVars = copy(scene.inst:getVariables())
    end
    return {
        name = name,
        localeArgs = copy(localeArgs or {}),
        localVars = Scene.GetDialogueLocalVars(nodeFunction),
        instanceVars = instanceVars
    }
end

---@param text    string
---@param context Source.Scenes.SceneMap.DialogueLocaleContext
---@return string
local function formatDialogueText(text, context)
    text = LOC(text)
    local resolvedLocaleArgs = {}
    for key, value in pairs(context.localeArgs) do
        local resolvedValue = value
        if Class.isInstance(value, "string") then
            resolvedValue = LOC(value)
        end
        resolvedLocaleArgs[key] = resolvedValue
    end
    text = Engine.ApplyStringMappingFormat(text, resolvedLocaleArgs)
    text = Engine.ApplyStringMappingFormat(text, context.localVars)
    text = Engine.ApplyStringMappingFormat(text, context.instanceVars)
    return text
end

---@param self Source.Scenes.SceneMap.SceneMap
---@return boolean
local function menuIsVisible(self)
    local menu = self._windowMenu:peek()
    return menu ~= nil and menu:getVisible()
end

---@param self            Source.Scenes.SceneMap.SceneMap
---@param previousEnabled boolean
local function restoreHotkeyOverlayMove(self, previousEnabled)
    if menuIsVisible(self) then
        return
    end
    self.player:setMoveEnabled(previousEnabled)
    self:_blockMapInput(WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.GetGameMap(self)
    assert(self._gameMap ~= nil, "Scene map is not loaded")
    return self._gameMap
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.ShowMessage(self, name, message, refActor, localeArgs)
    local refPosition = nil
    if refActor ~= nil then
        local gameMap = self:getGameMap()
        refPosition = gameMap:worldToUIScreenPosition(refActor:getPosition())
    end
    local restoreMove = suspendPlayerMovement(self.player, function ()
        self:_blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    end)
    ---@type Source.Scenes.SceneMap.DialogueMessageLocaleSource
    local dialogueSource = {
        kind = "message",
        context = createDialogueLocaleContext(self, name, localeArgs, Scene.ShowMessage),
        content = message
    }
    self._dialogueLocaleSource = dialogueSource
    local formattedName, formattedMessage = Scene.FormatDialogueMessageSource(dialogueSource)
    local messageWindow = self._messageWindow:get()
    messageWindow:setMessage(refPosition, formattedName, formattedMessage, restoreMove)
    return function ()
        if messageWindow:isInDialogue() then
            return false
        end
        restoreMove()
        return true
    end
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.ShowSelection(self, name, options, refActor, allowCancel, localeArgs)
    if allowCancel == nil then
        allowCancel = true
    end
    local refPosition = nil
    if refActor ~= nil then
        local gameMap = self:getGameMap()
        refPosition = gameMap:worldToUIScreenPosition(refActor:getPosition())
    end
    local restoreMove = suspendPlayerMovement(self.player, function ()
        self:_blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    end)
    ---@type Source.Scenes.SceneMap.DialogueSelectionLocaleSource
    local dialogueSource = {
        kind = "selection",
        context = createDialogueLocaleContext(self, name, localeArgs, Scene.ShowSelection),
        content = copy(options)
    }
    self._dialogueLocaleSource = dialogueSource
    local formattedName, formattedOptions = Scene.FormatDialogueSelectionSource(dialogueSource)
    local messageWindow = self._messageWindow:get()
    messageWindow:setSelection(refPosition, formattedName, formattedOptions, allowCancel, restoreMove)
    return function ()
        local selectionResult = messageWindow:getSelectionResult()
        if selectionResult == nil then
            return nil
        end
        restoreMove()
        return selectionResult
    end
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.ApplyLoadedGame(self, inst)
    self._gameOverRequest = nil
    self.inst = inst
    self.player = inst:getPlayer()
    self:_rebindPlayerToUI()
    local mapPath = inst:getCurrentMapPath() or GameSystem.GetStartMap()
    local position = self.player:getMapPosition()
    self._cachedMapFile = nil
    self._currentRegion = nil
    self:gotoMapAndPos(mapPath, position)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.ApplyPrimaryPlayer(self)
    self.player = self.inst:getPlayer()
    self:_rebindPlayerToUI()
    local mapPath = assert(self.inst:getCurrentMapPath(), "Primary player has no stored map")
    local position = self.player:getMapPosition()
    self._cachedMapFile = nil
    self._currentRegion = nil
    self:gotoMapAndPos(mapPath, position)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.RebindPlayerToUI(self)
    local windows = {
        self._windowItem, self._windowEquip, self._windowMenu, self._windowShop, self._windowAttrShop,
        self._windowEnemyBook, self._windowPlayerName
    }
    for _, lazyWindow in ipairs(windows) do
        local window = lazyWindow:peek()
        if window ~= nil then
            window:setPlayer(self.player)
        end
    end
    self._playerHUD:setPlayer(self.player)
end

---@param self Source.Scenes.SceneMap.SceneMap
---@return boolean
function Scene.ShowEnemyBook(self)
    if (not self:_canOpenMenu() and not self:_canOpenItemOverlay()) or not self.player:hasItem(ENEMY_BOOK_ITEM_ID) then
        return false
    end
    local window = self._windowEnemyBook:get()
    if not window:getVisible() then
        self._enemyBookMoveEnabledBeforeOpen = self:_isMenuBlocking() or self.player:getMoveEnabled()
        self.player:setMoveEnabled(false)
    end
    window:open(self:getGameMap())
    self:_blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    return true
end

---@param self Source.Scenes.SceneMap.SceneMap
---@return boolean
function Scene.ShowFloorTeleporter(self)
    if (not self:_canOpenMenu() and not self:_canOpenItemOverlay()) or not self.player:hasItem(FLOOR_TELEPORTER_ITEM_ID) then
        return false
    end
    local window = self._windowFloorTeleporter:get()
    if not window:getVisible() then
        self._floorTeleporterMoveEnabledBeforeOpen = self:_isMenuBlocking() or self.player:getMoveEnabled()
        self.player:setMoveEnabled(false)
    end
    window:open(self.inst)
    self:_blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    return true
end

---@param self Source.Scenes.SceneMap.SceneMap
---@return boolean
function Scene.OpenMenu(self)
    if not self:_canOpenMenu() then
        return false
    end
    self._pendingMenuOpen = true
    return true
end

---@param self Source.Scenes.SceneMap.SceneMap
---@param mode "load" | "save"
---@return boolean
function Scene.OpenSaveLoadUI(self, mode)
    if not self:_canOpenMenu() then
        return false
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self._pendingSaveLoadOpen = mode
    return true
end

---@param self Source.Scenes.SceneMap.SceneMap
---@return boolean
function Scene.OpenSaveUI(self)
    return Scene.OpenSaveLoadUI(self, "save")
end

---@param self Source.Scenes.SceneMap.SceneMap
---@return boolean
function Scene.OpenLoadUI(self)
    return Scene.OpenSaveLoadUI(self, "load")
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.ProcessPendingSaveLoadOpen(self)
    local mode = self._pendingSaveLoadOpen
    if mode == nil then
        return
    end
    self._pendingSaveLoadOpen = nil
    self._saveLoadMoveEnabledBeforeOpen = self.player:getMoveEnabled()
    self.player:setMoveEnabled(false)
    self._windowSaveLoad:get():open(WindowTransition.DEFAULT, mode)
    self:_blockMapInput(MAP_INPUT_BLOCK_FRAMES)
end

---@param self Source.Scenes.SceneMap.SceneMap
---@return boolean
function Scene.OpenItemUI(self)
    if not self:_canOpenMenu() then
        return false
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self._itemMoveEnabledBeforeOpen = self.player:getMoveEnabled()
    self.player:setMoveEnabled(false)
    self._windowItem:get():open(WindowTransition.DEFAULT)
    self:_blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    return true
end

---@param self Source.Scenes.SceneMap.SceneMap
---@return boolean
function Scene.OpenEquipUI(self)
    if not self:_canOpenMenu() then
        return false
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self._equipMoveEnabledBeforeOpen = self.player:getMoveEnabled()
    self.player:setMoveEnabled(false)
    self._windowEquip:get():open(WindowTransition.DEFAULT)
    self:_blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    return true
end

---@param self Source.Scenes.SceneMap.SceneMap
---@return boolean
function Scene.QuickSave(self)
    if not self:_canOpenMenu() then
        return false
    end
    self._pendingQuickSave = true
    return true
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.ProcessPendingQuickSave(self)
    if not self._pendingQuickSave then
        return
    end
    self._pendingQuickSave = false
    local instance = self:_getSaveSource()
    if instance == nil then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    local slot = Save.FindNextEmptySlot(WindowSaveSlot.MAX_SAVE_SLOTS)
    Save.SaveSlot(slot, instance, GameSystem.GetSavedScreenImage())
    AudioManager.playSound(GameSystem.GetSaveSE())
    self:showMessage("", "{QUICK_SAVE}", nil, { slot = slot })
end

---@param self Source.Scenes.SceneMap.SceneMap
---@return boolean
function Scene.QuickLoad(self)
    if not self:_canOpenMenu() then
        return false
    end
    local slot = Save.FindLatestSlot(WindowSaveSlot.MAX_SAVE_SLOTS)
    if slot == nil then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return true
    end
    local instance = Save.LoadGame(Save.GetSavePath(slot))
    if instance == nil then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return true
    end
    AudioManager.playSound(GameSystem.GetLoadSE())
    self:applyLoadedGame(instance)
    return true
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.OnHotkeySubMenuClose(self)
    local menu = self._windowMenu:peek()
    if menu ~= nil and menu:getVisible() then
        menu:onSubMenuClose()
        return
    end
    restoreHotkeyOverlayMove(self, true)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.OnHotkeyItemUsed(self)
    local menu = self._windowMenu:peek()
    if menu ~= nil and menu:getVisible() then
        menu:close()
        return
    end
    restoreHotkeyOverlayMove(self, true)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.OpenPlayerName(self)
    local window = self._windowPlayerName:get()
    if not window:getVisible() then
        self._playerNameMoveEnabledBeforeOpen = self.player:getMoveEnabled()
        self.player:setMoveEnabled(false)
        window:open()
        self:_blockMapInput(MAP_INPUT_BLOCK_FRAMES)
    end
    return function ()
        return not window:getVisible()
    end
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.OpenShop(self, buyItemIDs, canSell)
    self._shopMoveEnabledBeforeOpen = self:_isMenuBlocking() or self.player:getMoveEnabled()
    self.player:setMoveEnabled(false)
    local window = self._windowShop:get()
    window:open(buyItemIDs, canSell)
    return function ()
        return not window:getVisible()
    end
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.OpenAttrShop(self, actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName)
    self._attrShopMoveEnabledBeforeOpen = self:_isMenuBlocking() or self.player:getMoveEnabled()
    self.player:setMoveEnabled(false)
    local window = self._windowAttrShop:get()
    window:open(actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName)
    return function ()
        return not window:getVisible()
    end
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.OnShopClose(self)
    self.player:setMoveEnabled(self._shopMoveEnabledBeforeOpen)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.OnPlayerNameClose(self)
    self.player:setMoveEnabled(self._playerNameMoveEnabledBeforeOpen)
    self:_blockMapInput(MAP_INPUT_BLOCK_FRAMES)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.OnAttrShopClose(self)
    self.player:setMoveEnabled(self._attrShopMoveEnabledBeforeOpen)
    self:_blockMapInput(WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.OnEnemyBookClose(self)
    self.player:setMoveEnabled(self._enemyBookMoveEnabledBeforeOpen)
    self:_blockMapInput(WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
end

---@param entry Source.Windows.WindowEnemyBook.Entry
---@param self  Source.Scenes.SceneMap.SceneMap
function Scene.OnEnemyBookConfirm(self, entry)
    self._windowEnemyEncyclopedia:get():open(entry)
    self:_blockMapInput(MAP_INPUT_BLOCK_FRAMES)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.OnEnemyEncyclopediaClose(self)
    self.player:setMoveEnabled(self._enemyBookMoveEnabledBeforeOpen)
    self:_blockMapInput(WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.OnFloorTeleporterClose(self)
    self.player:setMoveEnabled(self._floorTeleporterMoveEnabledBeforeOpen)
    self:_blockMapInput(WINDOW_CLOSE_INPUT_BLOCK_FRAMES)
end

---@param mapKey    string
---@param telepoint sf.Vector2u
---@param self      Source.Scenes.SceneMap.SceneMap
function Scene.OnFloorTeleporterConfirm(self, mapKey, telepoint)
    local targetMap = self:resolveRegionMapPath(mapKey)
    local targetPosition = sf.Vector2i.new(telepoint.x, telepoint.y)
    ---@cast targetPosition sf.Vector2i
    local window = self._windowFloorTeleporter:peek()
    if window ~= nil then
        window:close()
    end
    self:gotoMapAndPos(targetMap, targetPosition)
    self.player:setMoveEnabled(self._floorTeleporterMoveEnabledBeforeOpen)
    self:_blockMapInput(MAP_INPUT_BLOCK_FRAMES)
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
        if Class.isInstance(key, "string") and key:sub(1, 2) ~= "__" then
            result[key] = value
        end
    end
    return result
end

function Scene.FormatDialogueMessageSource(source)
    return formatDialogueText(source.context.name, source.context), formatDialogueText(source.content, source.context)
end

function Scene.FormatDialogueSelectionSource(source)
    local formattedOptions = {}
    for _, option in ipairs(source.content) do
        formattedOptions[#formattedOptions + 1] = formatDialogueText(option, source.context)
    end
    return formatDialogueText(source.context.name, source.context), formattedOptions
end

---@return boolean
---@param self Source.Scenes.SceneMap.SceneMap
function Scene.CanRestoreMoveAfterMenuClose(self)
    return not self:_hasVisibleBlockingWindow()
end

---@return boolean
---@param self Source.Scenes.SceneMap.SceneMap
function Scene.HasVisibleBlockingWindow(self)
    for _, lazyWindow in ipairs(self._blockingWindows) do
        local window = lazyWindow:peek()
        if window ~= nil and window:getVisible() then
            return true
        end
    end
    return false
end

---@param frames integer
---@param self   Source.Scenes.SceneMap.SceneMap
function Scene.BlockMapInput(self, frames)
    frames = frames or WINDOW_CLOSE_INPUT_BLOCK_FRAMES
    self._mapInputBlockFrames = math.max(self._mapInputBlockFrames, frames)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.RequestTeleporterTransfer(self, targetMap, targetPosition, moveEnabled, findNearest, record)
    if self._pendingTeleporterTransfer ~= nil or self._pendingWorldTransfer ~= nil then
        return false
    end
    local savedPosition = sf.Vector2i.new(targetPosition.x, targetPosition.y)
    ---@cast savedPosition sf.Vector2i
    self._pendingTeleporterTransfer = {
        targetMap = targetMap,
        targetPosition = savedPosition,
        moveEnabled = moveEnabled,
        findNearest = findNearest,
        record = record
    }
    Transition.freezeTransitionBackground()
    return true
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.ProcessPendingTeleporterTransfer(self)
    if self._pendingTeleporterTransfer == nil or not Transition.isTransitionBackgroundFrozen() then
        return
    end
    local transferData = self._pendingTeleporterTransfer
    self._pendingTeleporterTransfer = nil
    self._mapTransferInProgress = true
    local targetMap = transferData.targetMap
    local targetPos = transferData.targetPosition
    local moveEnabled = bool(transferData.moveEnabled)
    self:gotoMapAndPos(targetMap, targetPos, true)
    local targetGameMap = self:getGameMap()
    local targetPlayer = targetGameMap:getPlayer()
    if targetPlayer == nil then
        self:_cancelTeleporterTransfer(moveEnabled)
        self._mapTransferInProgress = false
        return
    end

    local targetTag = ""
    if transferData.findNearest then
        local targetTeleporter = Teleporter.FindNearestTeleporter(
            targetGameMap:getAllActors(), targetPlayer:getMapPosition()
        )
        if targetTeleporter == nil then
            self:_cancelTeleporterTransfer(moveEnabled)
            self._mapTransferInProgress = false
            return
        end
        targetPos = targetTeleporter:getTeleportPosition()
        targetTag = targetTeleporter:getMapTag()
    end
    self:gotoMapAndPos(targetMap, targetPos)
    if transferData.record and self._cachedMapFile ~= nil then
        local savedTelepoint = sf.Vector2u.new(targetPos.x, targetPos.y)
        ---@cast savedTelepoint sf.Vector2u
        self.inst:recordTelepoint(self._cachedMapFile, savedTelepoint, targetTag)
    end
    targetPlayer:setMoveEnabled(moveEnabled)
    self._mapTransferInProgress = false
end

---@param moveEnabled boolean
---@param self        Source.Scenes.SceneMap.SceneMap
function Scene.CancelTeleporterTransfer(self, moveEnabled)
    self.player:setMoveEnabled(moveEnabled)
    Transition.cancelTransitionBackgroundFreeze()
    Transition.cancelPendingTransition()
end

---@param targetMap       string
---@param targetPosition  sf.Vector2i | nil
---@param blockTransition boolean
---@param self            Source.Scenes.SceneMap.SceneMap
function Scene.ApplyMapDestination(self, targetMap, targetPosition, blockTransition)
    if bool(targetMap) and self._cachedMapFile ~= targetMap then
        targetMap = self:loadMap(targetMap, targetPosition or self.player:getMapPosition())
        self._cachedMapFile = targetMap
    end
    self.inst:applyMapInfo(targetMap, targetPosition)
    if self._gameMap ~= nil and self._gameMap:isWorldMap() then
        assert(targetPosition ~= nil, "World map transfer requires a resolved target position: " .. targetMap)
        local worldMap = self._gameMap
        ---@cast worldMap Global.WorldGameMap.WorldGameMap
        worldMap:prepareViewportAt(targetPosition)
        self:_updateWorldEnvironment(0, true)
    end
    if not blockTransition then
        Transition.requestTransition(MAP_TRANSITION_NAME, MAP_TRANSITION_TIME)
    end
end

---@param targetMap      string
---@param targetPosition sf.Vector2i
---@param self           Source.Scenes.SceneMap.SceneMap
function Scene.QueueWorldTransfer(self, targetMap, targetPosition)
    assert(
        self._pendingWorldTransfer == nil and self._pendingTeleporterTransfer == nil,
        "A map transfer is already pending"
    )
    self._pendingWorldTransfer = { targetMap = targetMap, targetPosition = targetPosition }
    self._mapTransferInProgress = true
    Transition.freezeTransitionBackground()
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.ProcessPendingWorldTransfer(self)
    if self._pendingWorldTransfer == nil or not Transition.isTransitionBackgroundFrozen() then
        return
    end
    local transferData = self._pendingWorldTransfer
    self._pendingWorldTransfer = nil
    self:_applyMapDestination(transferData.targetMap, transferData.targetPosition, false)
    self._mapTransferInProgress = false
end

---@return Source.GameInstance.GameInstance
---@param self Source.Scenes.SceneMap.SceneMap
function Scene.GetSaveSource(self)
    return self.inst
end

---@param reason string
---@param self   Source.Scenes.SceneMap.SceneMap
function Scene.OnSaveLoadClose(self, reason)
    local menu = self._windowMenu:peek()
    if menu ~= nil and menu:getVisible() then
        if reason == "cancel" then
            menu:onSaveLoadClose()
            return
        end
        menu:close()
        return
    end
    restoreHotkeyOverlayMove(self, self._saveLoadMoveEnabledBeforeOpen)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.OnConfigClose(self)
    local menu = self._windowMenu:peek()
    if menu ~= nil then
        menu:onConfigClose()
    end
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.GotoMapAndPos(self, mapPath, pos, blockTransition)
    local targetMap = mapPath
    local targetPosition = pos
    if bool(mapPath) then
        local isChildEntry
        targetMap, targetPosition, isChildEntry = self._mapBuilder:resolveMapDestination(
            mapPath, self:_getCurrentRegionMap(), pos
        )
        if targetPosition == nil and bool(isChildEntry) and self._gameMap ~= nil and self._gameMap:isWorldMap()
            and self._cachedMapFile == targetMap then
            targetPosition = self.player:getMapPosition()
            local worldSize = self._gameMap:getSize()
            assert(targetPosition.x >= 0 and targetPosition.y >= 0
                    and targetPosition.x < worldSize.x and targetPosition.y < worldSize.y,
                "Current world position is outside the destination world")
        elseif targetPosition == nil
            and (bool(isChildEntry) or os.path.basename(targetMap) == MapConstants.WORLD_MANIFEST_FILE) then
            targetMap, targetPosition = self._mapBuilder:resolveMapDestination(
                mapPath, self:_getCurrentRegionMap(), self.player:getMapPosition()
            )
        end
    end
    ---@cast targetMap string
    local isWorldTarget = bool(targetMap) and os.path.basename(targetMap) == MapConstants.WORLD_MANIFEST_FILE
    if isWorldTarget and not blockTransition and not Transition.isTransitionBackgroundFrozen() then
        assert(targetPosition ~= nil, "World map transfer requires a resolved target position: " .. targetMap)
        self:_queueWorldTransfer(targetMap, targetPosition)
        return
    end
    self:_applyMapDestination(targetMap, targetPosition, blockTransition == true)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.TryCenterSymmetricTeleport(self)
    local gameMap = self:getGameMap()
    local player = gameMap:getPlayer()
    if player == nil then
        return false
    end
    local size = gameMap:getSize()
    local position = player:getMapPosition()
    local targetPosition = sf.Vector2i.new(size.x - 1 - position.x, size.y - 1 - position.y)
    ---@cast targetPosition sf.Vector2i
    if not gameMap:isPassable(player, targetPosition) then
        return false
    end
    player:setMapPosition(targetPosition)
    gameMap:updateActorOccupancy(player)
    return true
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.TryAdjacentFloorSamePos(self, step)
    local gameMap = self:getGameMap()
    local player = gameMap:getPlayer()
    if player == nil then
        return false
    end
    if not bool(self._cachedMapFile) then
        return false
    end
    local sourceMap = assert(self._cachedMapFile)
    ---@type string[]
    local regionMaps = RegionDict[self.inst:getCurrentRegion()] or {}
    local currentIndex = Teleporter.FindCurrentMapIndex(regionMaps, sourceMap)
    if currentIndex == nil then
        return false
    end
    local targetIndex = currentIndex + step
    if targetIndex < 1 or targetIndex > #regionMaps then
        return false
    end

    local targetMapKey = assert(regionMaps[targetIndex], "Adjacent region map index is unavailable")
    local targetMap = self:resolveRegionMapPath(targetMapKey)
    local sourcePosition = player:getMapPosition()
    local targetPosition = sf.Vector2i.new(sourcePosition.x, sourcePosition.y)
    ---@cast targetPosition sf.Vector2i
    if not self:_isMapPositionPassable(targetMap, player, targetPosition) then
        return false
    end

    self:gotoMapAndPos(targetMap, targetPosition, true)
    local targetGameMap = self:getGameMap()
    local targetPlayer = targetGameMap:getPlayer()
    if targetPlayer == nil or not targetGameMap:isPassable(targetPlayer, targetPosition) then
        self:gotoMapAndPos(sourceMap, sourcePosition, true)
        return false
    end
    Transition.requestTransition(MAP_TRANSITION_NAME, MAP_TRANSITION_TIME)
    return true
end

---@param mapPath  string
---@param actor    Engine.Actor
---@param position sf.Vector2i
---@return boolean
---@param self     Source.Scenes.SceneMap.SceneMap
function Scene.IsMapPositionPassable(self, mapPath, actor, position)
    local mapFile, mapData = self._mapBuilder:loadMapData(mapPath, self:_getCurrentRegionMap())
    assert(mapData.type ~= "worldMap", "Floor passability preview does not support world manifests: " .. mapFile)
    ---@cast mapData Source.SceneComponents.MapData
    local gameMap = self._mapBuilder:generateGameMap(mapData, nil, false, true)
    gameMap:applyTerrainDestructions(self.inst:getTerrainDestructions(mapFile))
    self._mapBuilder:applyAddedActors(gameMap, self.inst:getAddedActors(mapFile), false)
    gameMap:applyActorPositions(self.inst:getActorPositions(mapFile))
    gameMap:removeActorsByTags(self.inst:getDestroyedActors(mapFile))
    return gameMap:isPassable(actor, position)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.RecordAddedActor(self, actor)
    local layerName = self:getGameMap():getActorLayer(actor)
    if layerName ~= nil then
        assert(self._cachedMapFile ~= nil, "Scene map path is not loaded")
        self.inst:recordAddedActor(self._cachedMapFile, actor, layerName)
    end
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.RecordActorPosition(self, actor, position)
    assert(self._cachedMapFile ~= nil, "Scene map path is not loaded")
    self.inst:recordActorPosition(self._cachedMapFile, actor, position)
    local gameMap = self:getGameMap()
    if gameMap:isWorldMap() then
        local worldMap = gameMap
        ---@cast worldMap Global.WorldGameMap.WorldGameMap
        worldMap:recordWorldActorPosition(actor, position)
    end
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.RecordDestroyedActor(self, actor)
    assert(self._cachedMapFile ~= nil, "Scene map path is not loaded")
    self.inst:recordDestroyedActor(self._cachedMapFile, actor)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.RecordDestroyedActorTag(self, actorTag)
    assert(self._cachedMapFile ~= nil, "Scene map path is not loaded")
    self.inst:recordDestroyedActorTag(self._cachedMapFile, actorTag)
    if not bool(actorTag) then
        return
    end
    local gameMap = self:getGameMap()
    if not gameMap:isWorldMap() then
        return
    end
    local worldMap = gameMap
    ---@cast worldMap Global.WorldGameMap.WorldGameMap
    local actor = worldMap:getActorByTag(actorTag)
    if actor ~= nil then
        for _, listed in ipairs(actor:collectTree()) do
            if Class.isInstance(listed, ConditionalActor) then
                ---@cast listed Source.ConditionalActor
                listed:releaseConditionMonitor()
            end
        end
    end
    worldMap:suppressActorTag(actorTag)
end

---@param self Source.Scenes.SceneMap.SceneMap
function Scene.RecordTerrainDestructions(self, layerName, positions)
    if not bool(positions) then
        return
    end
    assert(self._cachedMapFile ~= nil, "Scene map path is not loaded")
    local gameMap = self:getGameMap()
    for _, position in ipairs(positions) do
        if gameMap:isWorldMap() then
            local worldMap = gameMap
            ---@cast worldMap Global.WorldGameMap.WorldGameMap
            local region, localPosition = worldMap:getRegionPosition(position)
            if region ~= nil then
                ---@cast localPosition sf.Vector2i
                self.inst:recordTerrainDestruction(
                    region.path, layerName, localPosition, worldMap:getTerrainTile(layerName, position)
                )
            end
        else
            self.inst:recordTerrainDestruction(
                self._cachedMapFile, layerName, position, gameMap:getTerrainTile(layerName, position)
            )
        end
    end
end

return Scene
