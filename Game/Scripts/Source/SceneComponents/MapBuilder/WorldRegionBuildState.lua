local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local AutoTileRuntime = require("Global.GameMap.AutoTileRuntime")
local RegionTerrain = require("Global.GameMap.RegionTerrain")
local WorldGeometry = require("Global.WorldGeometry")
local WorldMapConstants = require("Global.WorldMapConstants")
local Data = require("Global.Data")
local MapDataParser = require("Source.SceneComponents.MapBuilder.MapDataParser")
local MapConstants = require("Source.Configs.MapConstants")

local TILE_CHUNK_INDEX_YIELD_INTERVAL = 32

---@enum Source.SceneComponents.MapBuilder.WorldRegionBuildState.ActorDescriptorKind
local ActorDescriptorKind = {
    AUTHORED = "authored",
    ADDED = "added",
    MOVED = "moved"
}

---@enum Source.SceneComponents.MapBuilder.WorldRegionBuildState.BuildStage
local BuildStage = {
    IDLE = WorldMapConstants.IDLE_BUILD_STAGE,
    INITIALISE = "initialise",
    TERRAIN_OVERRIDES = "terrainOverrides",
    SCAN_AUTOTILES = "scanAutotiles",
    INDEX_TERRAIN_AUTOTILES = "indexTerrainAutotiles",
    CREATE_TILE_LAYER_DATA = "createTileLayerData",
    LOAD_AUTOTILE_TEXTURE = "loadAutotileTexture",
    LOAD_TILESET_TEXTURE = "loadTilesetTexture",
    CONSTRUCT_TILE_LAYER = "constructTileLayer",
    CREATE_TILE_SCRATCH = "createTileScratch",
    INDEX_TILE_CHUNKS = "indexTileChunks",
    WRITE_TILE_BLOCK = "writeTileBlock",
    BUILD_PRIORITY_TILE_CHUNK = "buildPriorityTileChunk",
    CREATE_TILE_LAYER = "createTileLayer",
    CREATE_TERRAIN = "createTerrain",
    INDEX_DESTROYED_TAG = "indexDestroyedTag",
    INDEX_MOVED_ACTOR = "indexMovedActor",
    PREPARE_AUTHORED_ACTOR = "prepareAuthoredActor",
    VALIDATE_MOVED_ACTOR = "validateMovedActor",
    ACTOR_ROOT = "actorRoot",
    BUILD_ACTOR = "buildActor",
    READY_PAYLOAD = "readyPayload",
    ACTOR_PHASE_COMPLETE = "actorPhaseComplete",
    WRITE_BACKGROUND_TILE_BLOCK = "writeBackgroundTileBlock",
    BUILD_BACKGROUND_TILE_CHUNK = "buildBackgroundTileChunk",
    COMPLETE = "complete"
}

local TextureManager = GlobalCore.TextureManager
local WORLD_REGION_FIXED_CACHE_BYTES = WorldMapConstants.REGION_FIXED_CACHE_BYTES
local WORLD_REGION_LAYER_CELL_CACHE_BYTES = WorldMapConstants.REGION_LAYER_CELL_CACHE_BYTES
local WORLD_REGION_ACTOR_CACHE_BYTES = 16 * 1024
local WORLD_REGION_BUILD_CHUNK_SIZE = WorldMapConstants.REGION_BUILD_CHUNK_SIZE
local WORLD_TILE_GRAPHICS_CHUNK_SIZE = WorldMapConstants.SPATIAL_CHUNK_SIZE
---@type table<Global.WorldGameMap.WorldMapData, Source.SceneComponents.WorldLayerValidationState>
local worldLayerValidationStates = setmetatable({}, { __mode = "k" })

local WorldRegionBuildState = {}

---@param worldData  Global.WorldGameMap.WorldMapData
---@param validation Source.SceneComponents.WorldLayerValidationState
---@return string[]
local function mergeValidatedLayerOrders(worldData, validation)
    local outgoing = {}
    local incoming = {}
    local priority = {}
    local nextPriority = 1
    for _, region in ipairs(worldData.regions) do
        local layerOrder = assert(validation.layerOrders[region.index])
        for _, layerName in ipairs(layerOrder) do
            if outgoing[layerName] == nil then
                outgoing[layerName] = {}
                incoming[layerName] = 0
                priority[layerName] = nextPriority
                nextPriority = nextPriority + 1
            end
        end
        for earlier = 1, #layerOrder do
            for later = earlier + 1, #layerOrder do
                local from = layerOrder[earlier]
                local target = layerOrder[later]
                if not outgoing[from][target] then
                    outgoing[from][target] = true
                    incoming[target] = incoming[target] + 1
                end
            end
        end
    end
    local result = {}
    local emitted = {}
    while #result < nextPriority - 1 do
        local nextLayer = nil
        local selectedPriority = math.huge
        for layerName, count in pairs(incoming) do
            if count == 0 and not emitted[layerName] and priority[layerName] < selectedPriority then
                nextLayer = layerName
                selectedPriority = priority[layerName]
            end
        end
        assert(nextLayer ~= nil, "Placed child maps contain conflicting layer orders")
        emitted[nextLayer] = true
        result[#result + 1] = nextLayer
        for target in pairs(outgoing[nextLayer]) do
            incoming[target] = incoming[target] - 1
        end
    end
    return result
end

---@param worldData Global.WorldGameMap.WorldMapData
---@param region    Global.WorldGameMap.WorldRegionData
---@param data      Global.GameMap.SerializedMapData
local function validateWorldRegionData(worldData, region, data)
    assert(
        data.type ~= MapConstants.WORLD_MAP_TYPE, "World child map cannot be another world manifest: " .. region.path
    )
    assert(
        data.width == region.width and data.height == region.height,
        "World child map size does not match placement rect: " .. region.path
    )
    ---@type table<string, integer>
    local worldLayerIndices = {}
    for index, layerName in ipairs(worldData.layerOrder) do
        worldLayerIndices[layerName] = index
    end
    local previousIndex = 0
    for _, layerName in ipairs(data.layerOrder) do
        local worldIndex = worldLayerIndices[layerName]
        assert(worldIndex ~= nil, "World child layer is missing from world layerOrder: " .. layerName)
        assert(worldIndex > previousIndex, "World child layer order conflicts with world layerOrder: " .. region.path)
        previousIndex = worldIndex
    end
    local validation = worldLayerValidationStates[worldData]
    if validation == nil then
        validation = { layerOrders = {}, loadedRegionCount = 0 }
        worldLayerValidationStates[worldData] = validation
    end
    if validation.layerOrders[region.index] == nil then
        validation.loadedRegionCount = validation.loadedRegionCount + 1
    end
    validation.layerOrders[region.index] = copy(data.layerOrder)
    if validation.loadedRegionCount == #worldData.regions then
        local mergedLayerOrder = mergeValidatedLayerOrders(worldData, validation)
        assert(
            #worldData.layerOrder == #mergedLayerOrder,
            "World layerOrder must equal the stable topological merge of placed child maps"
        )
        for index, layerName in ipairs(worldData.layerOrder) do
            assert(
                layerName == mergedLayerOrder[index],
                "World layerOrder must equal the stable topological merge of placed child maps"
            )
        end
    end
end

---@param builder      Source.SceneComponents.SceneMapBuilder
---@param worldData    Global.WorldGameMap.WorldMapData
---@param region       Global.WorldGameMap.WorldRegionData
---@param data         Global.GameMap.SerializedMapData
---@param inst         Source.GameInstance.GameInstance
---@param worldPath    string
---@param addedActors  Source.GameInstance.AddedActorRecord[]
---@param movedActors  Source.GameInstance.WorldMovedActorRecord[]
---@param priorityRect Global.WorldGeometry.CellRect | nil
---@return thread
local function createWorldRegionBuildCoroutine(
    builder, worldData, region, data, inst, worldPath, addedActors, movedActors, priorityRect
)
    return coroutine.create(function ()
        MapDataParser.ValidateMapData(data)
        validateWorldRegionData(worldData, region, data)
        local ambientLight = builder.BuildAmbientLight(data.ambientLight)
        local serializedActors = data.actors or {}
        coroutine.yield(BuildStage.INITIALISE)

        local rowMajorChunks, priorityChunks, priorityDataChunkCount = builder:createWorldRegionChunks(
            region, data.width, data.height, priorityRect
        )
        local nativeChunks, priorityNativeChunkCount = builder:createWorldTileGraphicsChunks(
            region, data.width, data.height, priorityRect
        )
        ---@cast nativeChunks Global.WorldGeometry.CellRect[]
        ---@async
        local function yieldTerrainOverrideStep()
            coroutine.yield(BuildStage.TERRAIN_OVERRIDES)
        end
        local terrainOverrides = builder:createWorldTerrainOverrides(
            data, inst:getTerrainDestructions(region.path), yieldTerrainOverrideStep
        )
        local tilemap = Engine.Tilemap.new({})
        ---@type Source.SceneComponents.WorldLayerBuildState[]
        local layerBuildStates = {}
        for _, layerKey in ipairs(data.layerOrder) do
            local layerData = data.layers[layerKey]
            local layerTileset = Data.GetTileset(layerData.layerTileset)
            local rawAutoTiles = layerData.autoTiles
            local layerTerrainOverrides = terrainOverrides[layerKey] or {}
            local autoTilePool = {}
            local autoTileIndexByKey = {}
            if rawAutoTiles ~= nil then
                for _, chunk in ipairs(rowMajorChunks) do
                    for y = chunk.y + 1, chunk.y + chunk.height do
                        local rawRow = rawAutoTiles[y]
                        for x = chunk.x + 1, chunk.x + chunk.width do
                            local cell = rawRow ~= nil and rawRow[x] or nil
                            if Class.isInstance(cell, "string") then
                                ---@cast cell string
                                if bool(cell) and Data.HasAutoTile(cell) and autoTileIndexByKey[cell] == nil then
                                    autoTilePool[#autoTilePool + 1] = Data.GetAutoTile(cell)
                                    autoTileIndexByKey[cell] = #autoTilePool - 1
                                end
                            end
                        end
                    end
                    coroutine.yield(BuildStage.SCAN_AUTOTILES)
                end
            end
            for _, row in pairs(layerTerrainOverrides) do
                for _, terrainOverride in pairs(row) do
                    local tileID = terrainOverride.tileID
                    if Class.isInstance(tileID, "string") then
                        ---@cast tileID string
                        if autoTileIndexByKey[tileID] == nil then
                            autoTilePool[#autoTilePool + 1] = Data.GetAutoTile(tileID)
                            autoTileIndexByKey[tileID] = #autoTilePool - 1
                        end
                    end
                end
                coroutine.yield(BuildStage.INDEX_TERRAIN_AUTOTILES)
            end
            local autoTileKeys = {}
            for key, index in pairs(autoTileIndexByKey) do
                autoTileKeys[index + 1] = key
            end
            local tileLayerData = Engine.TileLayerData.new(
                layerData.layerName, layerTileset, data.width, data.height, autoTilePool, autoTileKeys,
                tostring(layerData.material or "")
            )
            tileLayerData.layerTilesetKey = layerData.layerTileset
            coroutine.yield(BuildStage.CREATE_TILE_LAYER_DATA)
            local autoTileTextures = {}
            local autoTileFrameCounts = {}
            for index, entry in ipairs(autoTilePool) do
                local texture = TextureManager.load(entry.fileName)
                autoTileTextures[index] = texture
                autoTileFrameCounts[index] = AutoTileRuntime.GetFrameCount(texture)
                coroutine.yield(BuildStage.LOAD_AUTOTILE_TEXTURE)
            end
            local tilesetTexture = TextureManager.load(tileLayerData.layerTileset.fileName)
            coroutine.yield(BuildStage.LOAD_TILESET_TEXTURE)
            local tileLayer = Engine.TileLayer.new(
                tileLayerData, tilesetTexture, autoTileTextures, autoTileFrameCounts, true, true
            )
            coroutine.yield(BuildStage.CONSTRUCT_TILE_LAYER)
            local tileBlock = { n = WORLD_REGION_BUILD_CHUNK_SIZE }
            local autoTileBlock = { n = WORLD_REGION_BUILD_CHUNK_SIZE }
            for blockY = 1, WORLD_REGION_BUILD_CHUNK_SIZE do
                tileBlock[blockY] = { n = WORLD_REGION_BUILD_CHUNK_SIZE }
                autoTileBlock[blockY] = { n = WORLD_REGION_BUILD_CHUNK_SIZE }
            end
            coroutine.yield(BuildStage.CREATE_TILE_SCRATCH)
            local dataChunksByKey = {}
            for index, chunk in ipairs(rowMajorChunks) do
                dataChunksByKey[WorldGeometry.GridKey(chunk.x, chunk.y)] = chunk
                if index % TILE_CHUNK_INDEX_YIELD_INTERVAL == 0 then
                    coroutine.yield(BuildStage.INDEX_TILE_CHUNKS)
                end
            end
            ---@type Source.SceneComponents.WorldLayerBuildState
            local layerBuildState = {
                tileLayer = tileLayer,
                layerData = layerData,
                layerTileset = layerTileset,
                rawAutoTiles = rawAutoTiles,
                layerTerrainOverrides = layerTerrainOverrides,
                autoTileIndexByKey = autoTileIndexByKey,
                autoTilePoolSize = #autoTilePool,
                tileBlock = tileBlock,
                autoTileBlock = autoTileBlock,
                dataChunksByKey = dataChunksByKey,
                writtenDataChunks = {},
                width = data.width,
                height = data.height
            }
            for chunkIndex = 1, priorityDataChunkCount do
                builder:writeWorldLayerDataChunk(layerBuildState, assert(priorityChunks[chunkIndex]))
                coroutine.yield(BuildStage.WRITE_TILE_BLOCK)
            end
            for chunkIndex = 1, priorityNativeChunkCount do
                local chunk = assert(nativeChunks[chunkIndex])
                ---@cast chunk Global.WorldGeometry.CellRect
                tileLayer:buildChunk(
                    chunk.x // WORLD_TILE_GRAPHICS_CHUNK_SIZE, chunk.y // WORLD_TILE_GRAPHICS_CHUNK_SIZE
                )
                coroutine.yield(BuildStage.BUILD_PRIORITY_TILE_CHUNK)
            end
            tilemap:addLayer(tileLayer)
            layerBuildStates[#layerBuildStates + 1] = layerBuildState
            coroutine.yield(BuildStage.CREATE_TILE_LAYER)
        end

        local terrain = RegionTerrain.new(tilemap, Data.GetAutoTile)
        coroutine.yield(BuildStage.CREATE_TERRAIN)

        local classVarChanges = data.BPClassVarChanged
        local destroyedActors = {}
        for _, tag in ipairs(inst:getDestroyedActors(worldPath)) do
            destroyedActors[tag] = true
            coroutine.yield(BuildStage.INDEX_DESTROYED_TAG)
        end
        local actorPositions = inst:getActorPositions(worldPath)
        ---@type table<string, Source.GameInstance.WorldMovedActorRecord>
        local movedActorsByTag = {}
        local definitionTags = {}
        local definitionRegions = {}
        for _, record in ipairs(movedActors) do
            movedActorsByTag[record.tag] = record
            coroutine.yield(BuildStage.INDEX_MOVED_ACTOR)
        end
        ---@type table<string, Engine.Actor[]>
        local actors = {}
        ---@type Source.SceneComponents.WorldActorDescriptor[]
        local priorityActorDescriptors = {}
        ---@type Source.SceneComponents.WorldActorDescriptor[]
        local backgroundActorDescriptors = {}
        local actorsByTag = {}
        ---@param position sf.Vector2i
        ---@return boolean
        local function isPriorityPosition(position)
            return priorityRect == nil or WorldGeometry.RectContainsPosition(priorityRect, position)
        end
        ---@param descriptor Source.SceneComponents.WorldActorDescriptor
        local function appendActorDescriptor(descriptor)
            if isPriorityPosition(descriptor.position) then
                priorityActorDescriptors[#priorityActorDescriptors + 1] = descriptor
            else
                backgroundActorDescriptors[#backgroundActorDescriptors + 1] = descriptor
            end
        end
        for _, layerName in ipairs(data.layerOrder) do
            actors[layerName] = {}
            for _, actorData in ipairs(serializedActors[layerName] or {}) do
                local normalised = MapDataParser.NormaliseActorData(actorData)
                local rootTag = tostring(normalised.tag or "")
                definitionTags[rootTag] = true
                local movedRecord = movedActorsByTag[rootTag]
                if movedRecord ~= nil then
                    assert(
                        movedRecord.definitionRegion == region.path,
                        "Moved world Actor definitionRegion does not match authored placement: " .. rootTag
                    )
                else
                    local worldPosition = sf.Vector2i.new(
                        normalised.position.x + region.x, normalised.position.y + region.y
                    )
                    ---@cast worldPosition sf.Vector2i
                    appendActorDescriptor({
                        kind = ActorDescriptorKind.AUTHORED,
                        layer = layerName,
                        normalised = normalised,
                        position = actorPositions[rootTag] or worldPosition
                    })
                end
                coroutine.yield(BuildStage.PREPARE_AUTHORED_ACTOR)
            end
        end
        for _, record in ipairs(movedActors) do
            if record.definitionRegion == region.path then
                assert(
                    definitionTags[record.tag],
                    "Moved world Actor definition tag is not an authored root in " .. region.path .. ": " .. record.tag
                )
            end
            coroutine.yield(BuildStage.VALIDATE_MOVED_ACTOR)
        end
        for _, actorRecord in ipairs(addedActors) do
            assert(not definitionTags[actorRecord.tag], "Duplicate restored world MapTag: " .. actorRecord.tag)
            appendActorDescriptor({
                kind = ActorDescriptorKind.ADDED,
                layer = actorRecord.layer,
                record = actorRecord,
                position = actorRecord.position
            })
        end
        for _, actorRecord in ipairs(builder:selectWorldMovedActors(movedActors, region)) do
            appendActorDescriptor({
                kind = ActorDescriptorKind.MOVED,
                layer = actorRecord.layer,
                record = actorRecord,
                position = actorRecord.position
            })
        end
        ---@param descriptor Source.SceneComponents.WorldActorDescriptor
        ---@param background boolean
        ---@async
        local function buildActorDescriptor(descriptor, background)
            local actor = nil
            if descriptor.kind == ActorDescriptorKind.AUTHORED then
                ---@cast descriptor Source.SceneComponents.WorldAuthoredActorDescriptor
                local normalised = descriptor.normalised
                local actorChanges = classVarChanges ~= nil and classVarChanges[tostring(normalised.tag or "")] or nil
                actor = Data.GenActorFromData(normalised, descriptor.layer, actorChanges)
                if actor ~= nil and builder:pruneDestroyedActorTree(actor, destroyedActors) then
                    local worldPosition = sf.Vector2i.new(
                        normalised.position.x + region.x, normalised.position.y + region.y
                    )
                    ---@cast worldPosition sf.Vector2i
                    actor:setMapPosition(worldPosition)
                    for _, listed in ipairs(actor:collectTree()) do
                        local savedPosition = actorPositions[listed:getMapTag()]
                        if savedPosition ~= nil then
                            listed:setMapPosition(savedPosition)
                        end
                    end
                    definitionRegions[actor] = region.path
                else
                    actor = nil
                end
            elseif descriptor.kind == ActorDescriptorKind.ADDED then
                ---@cast descriptor Source.SceneComponents.WorldAddedActorDescriptor
                assert(
                    actorsByTag[descriptor.record.tag] == nil,
                    "Duplicate restored world MapTag: " .. descriptor.record.tag
                )
                actor = builder:generatePersistedActor(descriptor.record, actorPositions, destroyedActors, false)
            else
                ---@cast descriptor Source.SceneComponents.WorldMovedActorDescriptor
                assert(
                    actorsByTag[descriptor.record.tag] == nil,
                    "Duplicate restored world MapTag: " .. descriptor.record.tag
                )
                actor = assert(
                    builder:generatePersistedActor(descriptor.record, actorPositions, destroyedActors, true),
                    "Failed to restore moved world Actor: " .. descriptor.record.tag
                )
                definitionRegions[actor] = descriptor.record.definitionRegion
            end
            if actor ~= nil then
                local roots = actors[descriptor.layer] or {}
                actors[descriptor.layer] = roots
                roots[#roots + 1] = actor
                builder:indexActorTreeByTag(actorsByTag, actor)
                if background then
                    coroutine.yield(BuildStage.ACTOR_ROOT, actor, descriptor.layer)
                end
            end
            coroutine.yield(BuildStage.BUILD_ACTOR)
        end
        for _, descriptor in ipairs(priorityActorDescriptors) do
            buildActorDescriptor(descriptor, false)
        end
        ---@type Global.WorldGameMap.WorldRegionEnvironmentData
        local environmentData = {
            ambientLight = ambientLight,
            bgm = data.bgm,
            bgs = data.bgs,
            bgmFilter = data.bgmFilter,
            bgsFilter = data.bgsFilter,
            fog = MapDataParser.BuildFog(data.fog)
        }
        ---@type Global.WorldGameMap.RegionPayload
        local payload = {
            tilemap = tilemap,
            terrain = terrain,
            actors = actors,
            mapData = environmentData,
            actorSet = {},
            definitionRegions = definitionRegions,
            actorRoots = {},
            activeRoots = {},
            estimatedRuntimeBytes = WORLD_REGION_FIXED_CACHE_BYTES + data.width * data.height
                    * #data.layerOrder * WORLD_REGION_LAYER_CELL_CACHE_BYTES
                + (#priorityActorDescriptors + #backgroundActorDescriptors) * WORLD_REGION_ACTOR_CACHE_BYTES
        }
        coroutine.yield(BuildStage.READY_PAYLOAD, payload, layerBuildStates)
        for _, descriptor in ipairs(backgroundActorDescriptors) do
            buildActorDescriptor(descriptor, true)
        end
        coroutine.yield(BuildStage.ACTOR_PHASE_COMPLETE)
        for _, layerBuildState in ipairs(layerBuildStates) do
            for chunkIndex = priorityDataChunkCount + 1, #priorityChunks do
                builder:writeWorldLayerDataChunk(layerBuildState, priorityChunks[chunkIndex])
                coroutine.yield(BuildStage.WRITE_BACKGROUND_TILE_BLOCK)
            end
        end
        for _, layerBuildState in ipairs(layerBuildStates) do
            while not layerBuildState.tileLayer:isBuildComplete() do
                layerBuildState.tileLayer:buildNextChunk()
                coroutine.yield(BuildStage.BUILD_BACKGROUND_TILE_CHUNK)
            end
        end
        return BuildStage.COMPLETE, payload
    end)
end

---@diagnostic disable-next-line: unused
---@param worldData    Global.WorldGameMap.WorldMapData
---@param region       Global.WorldGameMap.WorldRegionData
---@param data         Global.GameMap.SerializedMapData
---@param inst         Source.GameInstance.GameInstance
---@param worldPath    string
---@param addedActors  Source.GameInstance.AddedActorRecord[]
---@param movedActors  Source.GameInstance.WorldMovedActorRecord[]
---@param priorityRect Global.WorldGeometry.CellRect | nil
---@return Source.SceneComponents.MapBuilder.WorldRegionBuildState.State
function WorldRegionBuildState.Create(
    builder, worldData, region, data, inst, worldPath, addedActors, movedActors, priorityRect
)
    local buildThread = createWorldRegionBuildCoroutine(
        builder, worldData, region, data, inst, worldPath, addedActors, movedActors, priorityRect
    )
    ---@type Source.SceneComponents.MapBuilder.WorldRegionBuildState.State
    local state
    local function advance(deadline, stopAfterActorPhase)
        state.lastStepMaximumStage = BuildStage.IDLE
        state.lastStepMaximumMilliseconds = 0.0
        state.lastStepResumeCount = 0
        if state.completed or perfCounter() >= deadline then
            return state.payload
        end
        repeat
            local started = perfCounter()
            local succeeded, stage, eventValue, eventMetadata = coroutine.resume(buildThread)
            local elapsedMilliseconds = (perfCounter() - started) * 1000.0
            if not succeeded then
                error(stage, 0)
            end
            ---@cast stage string
            state.currentStage = stage
            state.currentStepMilliseconds = elapsedMilliseconds
            state.lastStepResumeCount = state.lastStepResumeCount + 1
            if elapsedMilliseconds > state.lastStepMaximumMilliseconds then
                state.lastStepMaximumStage = stage
                state.lastStepMaximumMilliseconds = elapsedMilliseconds
            end
            state.maximumStepMilliseconds = math.max(state.maximumStepMilliseconds, elapsedMilliseconds)
            state.stageMaximumMilliseconds[stage] = math.max(
                state.stageMaximumMilliseconds[stage] or 0.0, elapsedMilliseconds
            )
            if coroutine.status(buildThread) == "dead" then
                assert(stage == BuildStage.COMPLETE, "World region builder ended without a complete event")
                ---@cast eventValue Global.WorldGameMap.RegionPayload
                state.payload = eventValue
                state.completed = true
                return eventValue
            end
            if stage == BuildStage.READY_PAYLOAD then
                ---@cast eventValue Global.WorldGameMap.RegionPayload
                ---@cast eventMetadata Source.SceneComponents.WorldLayerBuildState[]
                state.payload = eventValue
                state.layerBuildStates = eventMetadata
                state.ready = true
                return state.payload
            end
            if stage == BuildStage.ACTOR_ROOT then
                ---@cast eventValue Engine.Actor
                ---@cast eventMetadata string
                state.readyActorRoots[#state.readyActorRoots + 1] = {
                    actorRoot = eventValue,
                    actorLayer = eventMetadata
                }
                return nil
            end
            if stage == BuildStage.ACTOR_PHASE_COMPLETE then
                state.actorPhaseComplete = true
                if stopAfterActorPhase then
                    return nil
                end
            end
            if stage == BuildStage.BUILD_PRIORITY_TILE_CHUNK or stage == BuildStage.BUILD_BACKGROUND_TILE_CHUNK then
                state.geometryRevision = state.geometryRevision + 1
            end
        until perfCounter() >= deadline
        return nil
    end
    state = {
        completed = false,
        ready = false,
        geometryRevision = 0,
        actorPhaseComplete = false,
        readyActorRoots = {},
        maximumStepMilliseconds = 0.0,
        stageMaximumMilliseconds = {},
        currentStage = BuildStage.IDLE,
        currentStepMilliseconds = 0.0,
        lastStepMaximumStage = BuildStage.IDLE,
        lastStepMaximumMilliseconds = 0.0,
        lastStepResumeCount = 0,
        areActorsReady = function ()
            return state.actorPhaseComplete and not bool(state.readyActorRoots) and state.actorPublishQueue == nil
        end,
        prepareActors = function (deadline)
            if state.actorPhaseComplete then
                return true
            end
            advance(deadline, true)
            return state.actorPhaseComplete
        end,
        isCellReady = function (position)
            if state.layerBuildStates == nil or not state.actorPhaseComplete
                or bool(state.readyActorRoots) or state.actorPublishQueue ~= nil then
                return false
            end
            local localX = position.x - region.x
            local localY = position.y - region.y
            if localX < 0 or localY < 0 or localX >= data.width or localY >= data.height then
                return false
            end
            local localPosition = sf.Vector2i.new(localX, localY)
            ---@cast localPosition sf.Vector2i
            for _, layerBuildState in ipairs(state.layerBuildStates) do
                if not layerBuildState.tileLayer:isCellBuilt(localPosition) then
                    return false
                end
            end
            return true
        end,
        isRectReady = function (rect)
            if state.layerBuildStates == nil or not state.actorPhaseComplete
                or bool(state.readyActorRoots) or state.actorPublishQueue ~= nil then
                return false
            end
            local chunks, priorityChunkCount = builder:createWorldTileGraphicsChunks(
                region, data.width, data.height, rect
            )
            for _, layerBuildState in ipairs(state.layerBuildStates) do
                for chunkIndex = 1, priorityChunkCount do
                    local chunk = assert(chunks[chunkIndex])
                    if not layerBuildState.tileLayer:isChunkBuilt(
                        chunk.x // WORLD_TILE_GRAPHICS_CHUNK_SIZE, chunk.y // WORLD_TILE_GRAPHICS_CHUNK_SIZE
                    ) then
                        return false
                    end
                end
            end
            return true
        end,
        prepareRect = function (rect, deadline)
            if state.layerBuildStates == nil then
                return false
            end
            if not state.actorPhaseComplete or bool(state.readyActorRoots) or state.actorPublishQueue ~= nil then
                advance(deadline, true)
                if not state.actorPhaseComplete or bool(state.readyActorRoots) or state.actorPublishQueue ~= nil then
                    return false
                end
            end
            local chunks, priorityChunkCount = builder:createWorldTileGraphicsChunks(
                region, data.width, data.height, rect
            )
            for _, layerBuildState in ipairs(state.layerBuildStates) do
                for chunkIndex = 1, priorityChunkCount do
                    local chunk = assert(chunks[chunkIndex])
                    local chunkX = chunk.x // WORLD_TILE_GRAPHICS_CHUNK_SIZE
                    local chunkY = chunk.y // WORLD_TILE_GRAPHICS_CHUNK_SIZE
                    if not layerBuildState.tileLayer:isChunkBuilt(chunkX, chunkY) then
                        if not builder:prepareWorldLayerNativeChunk(layerBuildState, chunk, deadline) then
                            return false
                        end
                        if perfCounter() >= deadline then
                            return false
                        end
                        layerBuildState.tileLayer:buildChunk(chunkX, chunkY)
                        state.geometryRevision = state.geometryRevision + 1
                    end
                end
            end
            return true
        end,
        step = function (deadline)
            return advance(deadline, false)
        end
    }
    return state
end

return WorldRegionBuildState
