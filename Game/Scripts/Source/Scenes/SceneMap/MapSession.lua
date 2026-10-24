local GlobalCore = require("GlobalCore")
local StairDirection = require("Enums.StairDirection")
local ConditionalActor = require("Source.MapActors.ConditionalActor")
local GameSystem = require("Source.System")
local Teleporter = require("Source.MapActors.Teleporter")
local RegionDict = require("Source.Configs.RegionDict")
local MapConstants = require("Source.Configs.MapConstants")
local MapPath = require("Global.Utils.MapPath")
local MapBuilder = require("Source.SceneComponents.MapBuilder")

local AudioManager = GlobalCore.AudioManager
local Transition = GlobalCore.Transition

local MAP_TRANSITION_NAME = ""
local MAP_TRANSITION_TIME = 0.5

---@class (partial) Source.Scenes.SceneMap.MapSession
local MapSession = {}

function MapSession:init(scene, environment)
    self._scene = scene
    self._environment = environment
    self._mapBuilder = MapBuilder.new()
    self._gameMap = nil
    self._mapFile = nil
    self._currentRegion = nil
    self._pendingTeleporterTransfer = nil
    self._pendingWorldTransfer = nil
    self._mapTransferInProgress = false
end

function MapSession:getGameMap()
    return assert(self._gameMap, "Scene map is not loaded")
end

function MapSession:peekGameMap()
    return self._gameMap
end

function MapSession:getMapFile()
    return self._mapFile
end

function MapSession:getCurrentRegionMap()
    return self._mapFile or self._scene:getGameInstance():getCurrentMapPath() or GameSystem.GetStartMap()
end

function MapSession:replaceMap(mapPath, initialPosition)
    local instance = self._scene:getGameInstance()
    local mapFile, mapData = self._mapBuilder:loadMapData(mapPath, self:getCurrentRegionMap())
    ---@type GameMap
    local gameMap
    if mapData.type == "worldMap" then
        ---@cast mapData Global.WorldGameMap.WorldMapData
        gameMap = self._mapBuilder:generateWorldGameMap(mapFile, mapData, instance, initialPosition)
    else
        ---@cast mapData Global.GameMap.MapData
        gameMap = self._mapBuilder:generateGameMap(mapData, nil, false)
    end
    if self._gameMap ~= nil then
        ConditionalActor.ReleaseMapMonitors(self._gameMap)
        self._gameMap:disposeStreaming()
    end
    self._gameMap = gameMap
    self._mapFile = mapFile
    gameMap:setScene(self._scene)
    instance:applyMapInfo(mapFile, initialPosition)
    if not gameMap:isWorldMap() then
        self._mapBuilder:applyPersistentState(gameMap, instance, mapFile)
    end
    local player = self._scene:getPlayer()
    gameMap:setPlayer(player)
    gameMap:spawnActor(player, "default")
    return mapFile, mapData
end

function MapSession:updateRegion()
    local region = MapSession.FindRegionForMap(assert(self._mapFile, "Scene map path is not loaded"))
    self._scene:getGameInstance():setCurrentRegion(region or "")
    if region == self._currentRegion then
        return nil
    end
    self._currentRegion = region
    return region
end

function MapSession:invalidateLoadedMap()
    self._mapFile = nil
    self._currentRegion = nil
end

function MapSession:cancelActorOperations()
    if self._gameMap ~= nil then
        for _, actor in ipairs(self._gameMap:getAllActors()) do
            actor:cancelAsyncOperations()
        end
    end
end

function MapSession:isTransferInProgress()
    return self._mapTransferInProgress
end

function MapSession:cancelPendingTransfers()
    if self._pendingTeleporterTransfer ~= nil then
        self:_cancelTeleporterTransfer(
            self._pendingTeleporterTransfer.player, self._pendingTeleporterTransfer.moveEnabled
        )
    elseif self._pendingWorldTransfer ~= nil then
        self:_cancelTeleporterTransfer(self._pendingWorldTransfer.player, self._pendingWorldTransfer.moveEnabled)
    end
    self._mapTransferInProgress = false
end

function MapSession:processPendingTransfers()
    if not Transition.isTransitionBackgroundFrozen() then
        return
    end
    if self._pendingTeleporterTransfer ~= nil then
        self:_processTeleporterTransfer(self._pendingTeleporterTransfer)
    end
    if self._pendingWorldTransfer ~= nil and Transition.isTransitionBackgroundFrozen() then
        self:_processWorldTransfer(self._pendingWorldTransfer)
    end
end

function MapSession:dispose()
    self:cancelPendingTransfers()
    if self._gameMap ~= nil then
        ConditionalActor.ReleaseMapMonitors(self._gameMap)
        self._gameMap:disposeStreaming()
        self._gameMap = nil
    end
end

function MapSession:resolveRegionMapPath(mapKey)
    return self._mapBuilder:resolveRegionMapPath(mapKey, self:getCurrentRegionMap())
end

function MapSession:buildFloorMapPreview(mapKey, telepoint, previewSize, previewScale, showTelepointMarker)
    return self._mapBuilder:buildFloorMapPreview(
        self._scene:getGameInstance(), self:getCurrentRegionMap(), mapKey, telepoint, previewSize, previewScale,
        showTelepointMarker, self:getGameMap()
    )
end

function MapSession:getFloorMapPreviewState(mapKey)
    local mapPath = self._mapBuilder:resolveMapPath(mapKey, self:getCurrentRegionMap())
    local currentPath = self._mapBuilder:resolveMapPath(self:getCurrentRegionMap(), nil)
    local revision = mapPath == currentPath and self:getGameMap():getVisibilityRevision() or 0
    return mapPath, revision, self._mapBuilder:getFloorMapPreviewConditionRevision(self._scene:getGameInstance())
end

function MapSession:clearFloorMapPreviewCache()
    self._mapBuilder:clearFloorMapPreviewCache()
end

function MapSession.FindRegionForMap(mapFile)
    local currentName = MapSession.NormaliseRegionMapName(mapFile)
    local currentBaseName = os.path.basename(currentName)
    for region, regionMaps in pairs(RegionDict) do
        for _, regionMap in ipairs(regionMaps) do
            local regionMapName = MapSession.NormaliseRegionMapName(regionMap)
            if regionMapName == currentName
                or (not regionMapName:find("/", 1, true) and regionMapName == currentBaseName) then
                return region
            end
        end
    end
    return nil
end

function MapSession.NormaliseRegionMapName(mapPath)
    return (os.path.splitext(MapPath.Normalise(mapPath)))
end

function MapSession:_canRequestTeleporterTransfer(teleporter)
    if not self._scene:isGameplayRequestActive() or self._mapTransferInProgress
        or self._pendingTeleporterTransfer ~= nil or self._pendingWorldTransfer ~= nil
        or self._gameMap == nil or teleporter:isDestroyed()
        or not teleporter:isVisibleInHierarchy() or teleporter:getMap() ~= self._gameMap then
        return false
    end
    return self._gameMap:getPlayer() ~= nil and bool(self._mapFile)
end

function MapSession:requestFloorStep(teleporter, step)
    assert(step == 1 or step == -1, "Floor transfer step must be 1 or -1")
    if not self:_canRequestTeleporterTransfer(teleporter) then
        return false
    end
    ---@cast self._mapFile string
    local region = MapSession.FindRegionForMap(self._mapFile)
    local regionMaps = region ~= nil and RegionDict[region] or {}
    local currentIndex = Teleporter.FindCurrentMapIndex(regionMaps, self._mapFile)
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
    local targetStairDirection = step == 1 and StairDirection.Down or StairDirection.Up
    return self:_startTeleporterTransfer(teleporter, targetMap, anchorPosition, true, true, targetStairDirection)
end

function MapSession:requestMapTransfer(teleporter, mapPath, position, record)
    if not self:_canRequestTeleporterTransfer(teleporter) or not bool(mapPath) then
        return false
    end
    local targetMap, targetPosition = self._mapBuilder:resolveMapDestination(
        mapPath, self:getCurrentRegionMap(), position
    )
    assert(targetPosition ~= nil, "Teleporter transfer requires a target position")
    return self:_startTeleporterTransfer(teleporter, targetMap, targetPosition, false, record ~= false)
end

function MapSession:_startTeleporterTransfer(
    teleporter, targetMap, targetPosition, findNearest, record, targetStairDirection
)
    local player = self:getGameMap():getPlayer()
    assert(player ~= nil, "Teleporter transfer requires a player")
    local moveEnabled = player:getMoveEnabled()
    player:setMoveEnabled(false)
    if not self:requestTeleporterTransfer(
        targetMap, targetPosition, moveEnabled, findNearest, record, targetStairDirection
    ) then
        player:setMoveEnabled(moveEnabled)
        return false
    end
    if record then
        local sourcePosition = teleporter:getTeleportPosition()
        local sourceTelepoint = sf.Vector2u.new(sourcePosition.x, sourcePosition.y)
        ---@cast sourceTelepoint sf.Vector2u
        self._scene:getGameInstance():recordTelepoint(assert(self._mapFile), sourceTelepoint, teleporter:getMapTag())
    end
    AudioManager.playSound(teleporter.stairSE)
    return true
end

function MapSession:requestTeleporterTransfer(
    targetMap, targetPosition, moveEnabled, findNearest, record, targetStairDirection
)
    if self._pendingTeleporterTransfer ~= nil or self._pendingWorldTransfer ~= nil or self._mapTransferInProgress then
        return false
    end
    local savedPosition = targetPosition:copy()
    self._pendingTeleporterTransfer = {
        player = self._scene:getPlayer(),
        targetMap = targetMap,
        targetPosition = savedPosition,
        moveEnabled = moveEnabled,
        findNearest = findNearest,
        record = record,
        targetStairDirection = targetStairDirection
    }
    Transition.freezeTransitionBackground()
    return true
end

function MapSession:_processTeleporterTransfer(transferData)
    self._pendingTeleporterTransfer = nil
    self._mapTransferInProgress = true
    local targetMap = transferData.targetMap
    local targetPos = transferData.targetPosition
    local moveEnabled = bool(transferData.moveEnabled)
    self:gotoMapAndPos(targetMap, targetPos, true)
    local targetGameMap = self:getGameMap()
    local targetPlayer = targetGameMap:getPlayer()
    if targetPlayer == nil then
        self:_cancelTeleporterTransfer(transferData.player, moveEnabled)
        self._mapTransferInProgress = false
        return
    end

    local targetTag = ""
    if transferData.findNearest then
        local targetTeleporter = Teleporter.FindNearestTeleporter(
            targetGameMap:getAllActors(), targetPlayer:getMapPosition(), transferData.targetStairDirection
        )
        if targetTeleporter == nil then
            self:_cancelTeleporterTransfer(transferData.player, moveEnabled)
            self._mapTransferInProgress = false
            return
        end
        targetPos = targetTeleporter:getTeleportPosition()
        targetTag = targetTeleporter:getMapTag()
    end
    self:gotoMapAndPos(targetMap, targetPos)
    if transferData.record and self._mapFile ~= nil then
        local savedTelepoint = sf.Vector2u.new(targetPos.x, targetPos.y)
        ---@cast savedTelepoint sf.Vector2u
        self._scene:getGameInstance():recordTelepoint(self._mapFile, savedTelepoint, targetTag)
    end
    targetPlayer:setMoveEnabled(moveEnabled)
    self._mapTransferInProgress = false
end

function MapSession:_cancelTeleporterTransfer(player, moveEnabled)
    self._pendingTeleporterTransfer = nil
    self._pendingWorldTransfer = nil
    self._mapTransferInProgress = false
    if not player:isDestroyed() then
        player:setMoveEnabled(moveEnabled)
    end
    Transition.cancelTransitionBackgroundFreeze()
    Transition.cancelPendingTransition()
end

function MapSession:_applyMapDestination(targetMap, targetPosition, blockTransition)
    if bool(targetMap) and self._mapFile ~= targetMap then
        targetMap = self._scene:loadMap(targetMap, targetPosition or self._scene:getPlayer():getMapPosition())
        self._mapFile = targetMap
    end
    self._scene:getGameInstance():applyMapInfo(targetMap, targetPosition)
    if self._gameMap ~= nil and self._gameMap:isWorldMap() then
        assert(targetPosition ~= nil, "World map transfer requires a resolved target position: " .. targetMap)
        local worldMap = self:getGameMap()
        ---@cast worldMap Global.WorldGameMap.WorldGameMap
        worldMap:prepareViewportAt(targetPosition)
        self._environment:updateWorld(worldMap, self._scene:getPlayer(), 0, true)
    end
    if not blockTransition then
        Transition.requestTransition(MAP_TRANSITION_NAME, MAP_TRANSITION_TIME)
    end
end

function MapSession:_queueWorldTransfer(targetMap, targetPosition)
    assert(
        self._pendingWorldTransfer == nil and self._pendingTeleporterTransfer == nil and not self._mapTransferInProgress,
        "A map transfer is already pending"
    )
    self._pendingWorldTransfer = {
        targetMap = targetMap,
        targetPosition = targetPosition:copy(),
        player = self._scene:getPlayer(),
        moveEnabled = self._scene:getPlayer():getMoveEnabled()
    }
    self._mapTransferInProgress = true
    Transition.freezeTransitionBackground()
end

function MapSession:_processWorldTransfer(transferData)
    self._pendingWorldTransfer = nil
    self:_applyMapDestination(transferData.targetMap, transferData.targetPosition, false)
    self._mapTransferInProgress = false
end

function MapSession:gotoMapAndPos(mapPath, pos, blockTransition)
    local targetMap = mapPath
    local targetPosition = pos
    if bool(mapPath) then
        local isChildEntry
        targetMap, targetPosition, isChildEntry = self._mapBuilder:resolveMapDestination(
            mapPath, self:getCurrentRegionMap(), pos
        )
        if targetPosition == nil and bool(isChildEntry) and self._gameMap ~= nil and self._gameMap:isWorldMap()
            and self._mapFile == targetMap then
            targetPosition = self._scene:getPlayer():getMapPosition()
            local worldSize = self._gameMap:getSize()
            assert(targetPosition.x >= 0 and targetPosition.y >= 0
                    and targetPosition.x < worldSize.x and targetPosition.y < worldSize.y,
                "Current world position is outside the destination world")
        elseif targetPosition == nil
            and (bool(isChildEntry) or os.path.basename(targetMap) == MapConstants.WORLD_MANIFEST_FILE) then
            targetMap, targetPosition = self._mapBuilder:resolveMapDestination(
                mapPath, self:getCurrentRegionMap(), self._scene:getPlayer():getMapPosition()
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

function MapSession:tryCenterSymmetricTeleport()
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

function MapSession:tryAdjacentFloorSamePos(step)
    local gameMap = self:getGameMap()
    local player = gameMap:getPlayer()
    if player == nil then
        return false
    end
    if not bool(self._mapFile) then
        return false
    end
    local sourceMap = assert(self._mapFile)
    ---@type string[]
    local regionMaps = RegionDict[self._scene:getGameInstance():getCurrentRegion()] or {}
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

function MapSession:_isMapPositionPassable(mapPath, actor, position)
    local mapFile, mapData = self._mapBuilder:loadMapData(mapPath, self:getCurrentRegionMap())
    assert(mapData.type ~= "worldMap", "Floor passability preview does not support world manifests: " .. mapFile)
    ---@cast mapData Global.GameMap.MapData
    local gameMap = self._mapBuilder:generateGameMap(mapData, nil, false, true)
    self._mapBuilder:applyPersistentState(gameMap, self._scene:getGameInstance(), mapFile)
    return gameMap:isPassable(actor, position)
end

function MapSession:recordAddedActor(actor)
    local layerName = self:getGameMap():getActorLayer(actor)
    if layerName ~= nil then
        assert(self._mapFile ~= nil, "Scene map path is not loaded")
        self._scene:getGameInstance():recordAddedActor(self._mapFile, actor, layerName)
    end
end

function MapSession:recordActorPosition(actor, position)
    assert(self._mapFile ~= nil, "Scene map path is not loaded")
    self._scene:getGameInstance():recordActorPosition(self._mapFile, actor, position)
    local gameMap = self:getGameMap()
    if gameMap:isWorldMap() then
        local worldMap = gameMap
        ---@cast worldMap Global.WorldGameMap.WorldGameMap
        worldMap:recordWorldActorPosition(actor, position)
    end
end

function MapSession:recordDestroyedActor(actor)
    assert(self._mapFile ~= nil, "Scene map path is not loaded")
    self._scene:getGameInstance():recordDestroyedActor(self._mapFile, actor)
end

function MapSession:recordDestroyedActorTag(actorTag)
    assert(self._mapFile ~= nil, "Scene map path is not loaded")
    self._scene:getGameInstance():recordDestroyedActorTag(self._mapFile, actorTag)
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
                ---@cast listed Source.MapActors.ConditionalActor
                listed:releaseConditionMonitor()
            end
        end
    end
    worldMap:suppressActorTag(actorTag)
end

function MapSession:recordTerrainDestructions(layerName, positions)
    if not bool(positions) then
        return
    end
    assert(self._mapFile ~= nil, "Scene map path is not loaded")
    local gameMap = self:getGameMap()
    for _, position in ipairs(positions) do
        if gameMap:isWorldMap() then
            local worldMap = gameMap
            ---@cast worldMap Global.WorldGameMap.WorldGameMap
            local region, localPosition = worldMap:getRegionPosition(position)
            if region ~= nil then
                ---@cast localPosition sf.Vector2i
                self
                    ._scene
                    :getGameInstance()
                    :recordTerrainDestruction(region.path, layerName, localPosition, worldMap:getTerrainTile(
                        layerName, position
                    ))
            end
        else
            self
                ._scene
                :getGameInstance()
                :recordTerrainDestruction(self._mapFile, layerName, position, gameMap:getTerrainTile(
                    layerName, position
                ))
        end
    end
end

return class(MapSession)
