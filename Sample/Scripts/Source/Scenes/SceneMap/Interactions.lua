local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local LocaleCore = require("Source.Locale.Core")
---@type { Item: Source.Configs.GeneralEnum.Item }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local UiLayout = require("Source.UI.UiLayout")
local WindowAttrShop = require("Source.Windows.WindowAttrShop")
local WindowShop = require("Source.Windows.WindowShop")

local Node = Engine.Node
local GlobalSystem = GlobalCore.System
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local ENEMY_BOOK_SIZE = 352
local ENEMY_ENCYCLOPEDIA_WIDTH = 640
local ENEMY_ENCYCLOPEDIA_HEIGHT = 480
local MAP_TRANSITION_NAME = ""
local MAP_TRANSITION_TIME = 0.5
local ENEMY_BOOK_ITEM_ID = GeneralEnum.Item.EnemyBook
local FLOOR_TELEPORTER_ITEM_ID = GeneralEnum.Item.Teleport

---@type SceneMapInteractionsState
local Scene = {}

---@param scene Source.Scenes.SceneMap.SceneMap
---@return fun()
local function suspendPlayerMovement(scene)
    local moveEnabled = scene.player:getMoveEnabled()
    local restored = false
    scene.player:setMoveEnabled(false)
    return function ()
        if restored then
            return
        end
        restored = true
        scene.player:setMoveEnabled(moveEnabled)
        scene:_blockMapInput(2)
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
        if type(value) == "string" then
            resolvedValue = LOC(value)
        end
        resolvedLocaleArgs[key] = resolvedValue
    end
    text = Engine.ApplyStringMappingFormat(text, resolvedLocaleArgs)
    text = Engine.ApplyStringMappingFormat(text, context.localVars)
    text = Engine.ApplyStringMappingFormat(text, context.instanceVars)
    return text
end

function Scene:getGameMap()
    assert(self._gameMap ~= nil, "Scene map is not loaded")
    return self._gameMap
end

function Scene:showMessage(name, message, refActor, localeArgs)
    local refPosition = nil
    if refActor ~= nil then
        local gameMap = self:getGameMap()
        refPosition = gameMap:worldToUIScreenPosition(refActor:getPosition())
    end
    local restoreMove = suspendPlayerMovement(self)
    ---@type Source.Scenes.SceneMap.DialogueMessageLocaleSource
    local dialogueSource = {
        kind = "message",
        context = createDialogueLocaleContext(self, name, localeArgs, Scene.showMessage),
        content = message
    }
    self._dialogueLocaleSource = dialogueSource
    local formattedName, formattedMessage = Scene.FormatDialogueMessageSource(dialogueSource)
    self._messageWindow:setMessage(refPosition, formattedName, formattedMessage, restoreMove)
    return function ()
        if self._messageWindow:isInDialogue() then
            return false
        end
        restoreMove()
        return true
    end
end

function Scene:showSelection(name, options, refActor, allowCancel, localeArgs)
    if allowCancel == nil then
        allowCancel = true
    end
    local refPosition = nil
    if refActor ~= nil then
        local gameMap = self:getGameMap()
        refPosition = gameMap:worldToUIScreenPosition(refActor:getPosition())
    end
    local restoreMove = suspendPlayerMovement(self)
    ---@type Source.Scenes.SceneMap.DialogueSelectionLocaleSource
    local dialogueSource = {
        kind = "selection",
        context = createDialogueLocaleContext(self, name, localeArgs, Scene.showSelection),
        content = copy(options)
    }
    self._dialogueLocaleSource = dialogueSource
    local formattedName, formattedOptions = Scene.FormatDialogueSelectionSource(dialogueSource)
    self._messageWindow:setSelection(refPosition, formattedName, formattedOptions, allowCancel, restoreMove)
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
    local mapPath = inst._cachedMap or GameSystem.GetStartMap()
    local position = self.player:getMapPosition()
    self._cachedMapFile = nil
    self._currentRegion = nil
    self:gotoMapAndPos(mapPath, position)
end

function Scene:_rebindPlayerToUI()
    self._windowItem:setPlayer(self.player)
    self._windowEquipSlot:setPlayer(self.player)
    self._windowEquipSelect:setPlayer(self.player)
    self._windowEquipStatus:setPlayer(self.player)
    self._windowMenu:setPlayer(self.player)
    self._windowShop:setPlayer(self.player)
    self._windowAttrShop:setPlayer(self.player)
    self._windowEnemyBook:setPlayer(self.player)
    self._playerHUD:setPlayer(self.player)
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
    if (not self:_canOpenMenu() and not self:_canOpenItemOverlay()) or not self.player:hasItem(FLOOR_TELEPORTER_ITEM_ID) then
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

---@param entry Source.UI.WindowEnemyBook.Entry
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
    local targetPosition = sf.Vector2i.new(telepoint.x, telepoint.y)
    ---@cast targetPosition sf.Vector2i
    self._windowFloorTeleporter:close()
    self:gotoMapAndPos(targetMap, targetPosition)
    self.player:setMoveEnabled(self._floorTeleporterMoveEnabledBeforeOpen)
    self:_blockMapInput(2)
end

function Scene:_recordCurrentFloorTelepoint()
    if self._gameMap == nil or self._cachedMapFile == nil then
        return
    end
    local telepoint = self:_findNearestFloorTelepoint()
    if telepoint == nil then
        return
    end
    local savedTelepoint = sf.Vector2u.new(telepoint.x, telepoint.y)
    ---@cast savedTelepoint sf.Vector2u
    self.inst:recordTelepoint(self._cachedMapFile, savedTelepoint)
end

---@return sf.Vector2i | nil
function Scene:_findNearestFloorTelepoint()
    local gameMap = self:getGameMap()
    local player = gameMap:getPlayer()
    if player == nil then
        return nil
    end
    local Teleporter = require("Source.Teleporter")

    local nearest = Teleporter.FindNearestTeleporter(gameMap:getAllActors(), player:getMapPosition())
    return nearest ~= nil and nearest:getTeleportPosition() or nil
end

---@return sf.IntRect, sf.IntRect
function Scene.GetShopRects()
    local commandRect, itemRect = WindowShop.GetDefaultRects()
    return commandRect, itemRect
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
    if self._pendingFloorTransfer ~= nil or self._pendingWorldTransfer ~= nil then
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
    local transferData = {
        targetMap = self._pendingFloorTransfer.targetMap,
        anchorPos = self._pendingFloorTransfer.anchorPos,
        moveEnabled = self._pendingFloorTransfer.moveEnabled
    }
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

    local targetTeleporter = Teleporter.FindNearestTeleporter(
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
        local savedTelepoint = sf.Vector2u.new(targetPos.x, targetPos.y)
        ---@cast savedTelepoint sf.Vector2u
        self.inst:recordTelepoint(self._cachedMapFile, savedTelepoint)
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

---@param targetMap       string
---@param targetPosition  sf.Vector2i | nil
---@param blockTransition boolean
function Scene:_applyMapDestination(targetMap, targetPosition, blockTransition)
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
        GlobalSystem.requestTransition(MAP_TRANSITION_NAME, MAP_TRANSITION_TIME)
    end
end

---@param targetMap      string
---@param targetPosition sf.Vector2i
function Scene:_queueWorldTransfer(targetMap, targetPosition)
    assert(self._pendingWorldTransfer == nil and self._pendingFloorTransfer == nil, "A map transfer is already pending")
    self._pendingWorldTransfer = { targetMap = targetMap, targetPosition = targetPosition }
    self._mapTransferInProgress = true
    GlobalSystem.freezeTransitionBackground()
end

function Scene:_processPendingWorldTransfer()
    if self._pendingWorldTransfer == nil or not GlobalSystem.isTransitionBackgroundFrozen() then
        return
    end
    local transferData = self._pendingWorldTransfer
    self._pendingWorldTransfer = nil
    self:_applyMapDestination(transferData.targetMap, transferData.targetPosition, false)
    self._mapTransferInProgress = false
end

---@return Source.GameInstance.GameInstance
function Scene:_getSaveSource()
    return self.inst
end

---@param reason string
function Scene:_onSaveLoadClose(reason)
    if reason == "cancel" then
        self._windowMenu:onSaveLoadClose()
        return
    end
    self._windowMenu:close()
end

function Scene:_onConfigClose()
    self._windowMenu:onConfigClose()
end

function Scene:gotoMapAndPos(mapPath, pos, blockTransition)
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
        elseif targetPosition == nil and (bool(isChildEntry) or os.path.basename(targetMap) == "_world.json") then
            targetMap, targetPosition = self._mapBuilder:resolveMapDestination(
                mapPath, self:_getCurrentRegionMap(), self.player:getMapPosition()
            )
        end
    end
    ---@cast targetMap string
    local isWorldTarget = bool(targetMap) and os.path.basename(targetMap) == "_world.json"
    if isWorldTarget and not blockTransition and not GlobalSystem.isTransitionBackgroundFrozen() then
        assert(targetPosition ~= nil, "World map transfer requires a resolved target position: " .. targetMap)
        self:_queueWorldTransfer(targetMap, targetPosition)
        return
    end
    self:_applyMapDestination(targetMap, targetPosition, blockTransition == true)
end

function Scene:tryCenterSymmetricTeleport()
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

function Scene:tryAdjacentFloorSamePos(step)
    ---@type table<string, string[]>
    local RegionDict = require("Source.Configs.RegionDict")
    local Teleporter = require("Source.Teleporter")

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
    GlobalSystem.requestTransition(MAP_TRANSITION_NAME, MAP_TRANSITION_TIME)
    return true
end

---@param mapPath  string
---@param actor    Engine.Actor
---@param position sf.Vector2i
---@return boolean
function Scene:_isMapPositionPassable(mapPath, actor, position)
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
    local gameMap = self:getGameMap()
    if gameMap:isWorldMap() then
        local worldMap = gameMap
        ---@cast worldMap Global.WorldGameMap.WorldGameMap
        worldMap:recordWorldActorPosition(actor, position)
    end
end

function Scene:recordDestroyedActor(actor)
    assert(self._cachedMapFile ~= nil, "Scene map path is not loaded")
    self.inst:recordDestroyedActor(self._cachedMapFile, actor)
end

function Scene:recordDestroyedActorTag(actorTag)
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
    worldMap:suppressActorTag(actorTag)
end

function Scene:recordTerrainDestructions(layerName, positions)
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
