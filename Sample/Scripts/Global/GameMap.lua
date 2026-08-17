local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local ActorPixelShatterEffect = require("Global.CustomEffects.ActorPixelShatterEffect")
local DamageTextParticle = require("Global.CustomParticles.DamageTextParticle")
local Pool = require("Global.Pool")
local Render = require("Global.Utils.Render")

local ComponentsFunctions = GlobalFunctions.Components
local ManagerFunctions = GlobalFunctions.Manager
local ShaderManager = GlobalCore.ShaderManager
local System = GlobalCore.System
local WeatherController = GlobalCore.WeatherController
local FogController = GlobalCore.FogController

local MAX_SHADER_LIGHTS = 16
local DYNAMIC_TRANSMISSION_PADDING = 2
local UNOBSTRUCTED_LIGHT_INTENSITY_UNIFORMS = {}
for index = 0, MAX_SHADER_LIGHTS - 1 do
    UNOBSTRUCTED_LIGHT_INTENSITY_UNIFORMS[index + 1] = "lightIntensity[" .. index .. "]"
end
local Actor = Engine.Actor
local ActorUpdateBatch = Engine.ActorUpdateBatch
local Camera = GlobalCore.Camera
local Character = Engine.Character
local GameMapBase = GlobalCore.GameMapBase
local Light = GlobalCore.Light

local LISTENER_DIRECTION_UP = sf.Vector3f.new(0.0, -1.0, 0.0)
local LISTENER_DIRECTION_DOWN = sf.Vector3f.new(0.0, 1.0, 0.0)
local LISTENER_DIRECTION_LEFT = sf.Vector3f.new(-1.0, 0.0, 0.0)
local LISTENER_DIRECTION_RIGHT = sf.Vector3f.new(1.0, 0.0, 0.0)
local DEFAULT_LISTENER_DIRECTION = sf.Vector3f.new(0.0, 0.0, -1.0)
local CHARACTER_LISTENER_UP_VECTOR = sf.Vector3f.new(0.0, 0.0, -1.0)
local DEFAULT_LISTENER_UP_VECTOR = sf.Vector3f.new(0.0, 1.0, 0.0)

local GameMap = {}

GameMap.MapViewOffset = sf.Vector2f.new(80.0, 0.0)

function GameMap:init(mapName, tilemap, camera, previewOnly)
    local materialShader = nil
    local tilemapLightMaskShader = nil
    local lightMaskShader = nil
    local lightPassShader = nil
    local unobstructedLightPassShader = nil
    local actorHueShader = nil
    local actorPixelShatterShader = nil
    if sf.Shader.isAvailable() then
        if not previewOnly then
            tilemapLightMaskShader = ManagerFunctions.loadShader(
                "Global/TilemapLightMask.frag", sf.Shader.Type.Fragment
            )
            lightMaskShader = ManagerFunctions.loadShader("Global/LightMask.frag", sf.Shader.Type.Fragment)
            materialShader = ManagerFunctions.loadShader("Global/Material.frag", sf.Shader.Type.Fragment)
            lightPassShader = ManagerFunctions.loadShader("Global/LightPass.frag", sf.Shader.Type.Fragment)
            unobstructedLightPassShader = ManagerFunctions.loadShader(
                "Global/UnoccludedLightPass.frag", sf.Shader.Type.Fragment
            )
            actorPixelShatterShader = assert(ShaderManager.loadFull(
                "Global/ActorPixelShatter.vert", "Global/ActorPixelShatter.frag"
            ), "Actor pixel shatter shader must not be nil")
        end
        actorHueShader = ManagerFunctions.loadShader("Global/Hue.frag", sf.Shader.Type.Fragment)
    end
    ---@type GlobalCore.GameMapBase
    local gameMapBase = self
    GameMapBase.init(gameMapBase)
    self._tilemapLightMaskShader = tilemapLightMaskShader
    self._lightMaskShader = lightMaskShader
    self._lightPassShader = lightPassShader
    self._unobstructedLightPassShader = unobstructedLightPassShader
    self._materialShader = materialShader
    self._actorHueShader = actorHueShader
    self._actorPixelShatterShader = actorPixelShatterShader
    self._previewOnly = previewOnly == true
    self.mapName = mapName
    self._persistentMapPath = mapName
    ---@type GlobalCore.SceneBase | nil
    self._scene = nil
    ---@type Engine.Tilemap
    self._tilemap = tilemap
    self._layersTopFirst = {}
    local layerNames = self._tilemap:getLayerNameList()
    self._layerNames = layerNames
    for index = #layerNames, 1, -1 do
        self._layersTopFirst[#self._layersTopFirst + 1] = self._tilemap:getLayer(layerNames[index])
    end
    self._actors = {}
    ---@type Engine.ParticleSystem | nil
    self._particleSystem = nil
    if not self._previewOnly then
        self._particleSystem = Engine.ParticleSystem.new()
    end
    self._actorsOnDestroy = {}
    ---@type table<string, Global.CustomEffects.ActorPixelShatterEffect[]>
    self._actorPixelShatterEffects = {}
    ---@type table<Engine.Actor, Global.CustomEffects.ActorPixelShatterEffect>
    self._actorPixelShatterByActor = setmetatable({}, {
        __mode = "k"
    })
    self._actorPixelShatterSeed = 0
    self._wholeActorList = {}
    self._actorUpdateList = {}
    self._actorUpdateBatch = ActorUpdateBatch.new()
    self._createInitialisedActorIDs = {}
    self._componentInitialisedActorIDs = {}
    ---@type integer
    self._actorBatchDepth = 0
    ---@type boolean
    self._initialisingActors = false
    ---@type GlobalCore.Camera | nil
    self._camera = nil
    if not self._previewOnly then
        self._camera = camera or Camera.new()
        self._camera:setMap(self)
    end
    self._lights = {}
    self._ambientLight = sf.Color.new(255, 255, 255, 255)
    self._lightBlockSize = sf.Vector2f.new(Engine.CellSize, Engine.CellSize)
    local tilemapSize = self._tilemap:getSize()
    self._shaderMapSize = sf.Vector2f.new(tilemapSize.x, tilemapSize.y)
    self._playerCoverColour = sf.Color.new(255, 255, 255, GameMap.DefaultCoverAlpha)
    ---@type sf.RenderTexture | nil
    self._staticTransmission = nil
    self._staticOccupancy = nil
    ---@type number[][] | nil
    self._staticOccupancyPrefix = nil
    ---@type sf.RenderTexture | nil
    self._surfaceMask = nil
    ---@type sf.RenderTexture | nil
    self._dynamicTransmission = nil
    ---@type sf.RenderTexture | nil
    self._directLight = nil
    ---@type sf.RenderTexture | nil
    self._staticDirectLight = nil
    self._useStaticDirectLight = false
    ---@type sf.RectangleShape | nil
    self._lightPassQuad = nil
    ---@type sf.VertexArray | nil
    self._unobstructedLightVertices = nil
    ---@type sf.Vertex | nil
    self._unobstructedLightVertex = nil
    self._unobstructedLightCache = nil
    ---@type table[] | nil
    self._cachedActiveLights = nil
    ---@type integer
    self._cachedLightMaterialRevision = -1
    ---@type tuple<any> | nil
    self._cachedLightTransmissionSignature = nil
    ---@type integer
    self._staticTransmissionRevision = -1
    ---@type tuple<any> | nil
    self._staticTransmissionSignature = nil
    ---@type boolean
    self._staticHasTransmissionLoss = false
    self._dynamicTransmissionPixelSize = 0
    self._zeroShaderOffset = sf.Vector2f.new(0.0, 0.0)
    self._shaderColour = sf.Vector3f.new(0.0, 0.0, 0.0)
    ---@type sf.RenderTexture | nil
    self._actorShaderBuffer = nil
    ---@type sf.RenderTexture | nil
    self._actorHueBuffer = nil
    self._actorHueSourceSprite = nil
    ---@type boolean
    self._materialDirty = true
    ---@type integer
    self._materialRevision = 0
    ---@type table<string, GameMapLayerMaskTextureCacheEntry>
    self._layerMaskTextureCache = {}
    self._shaderTime = 0.0
    self._transparentTiles = {}
    ---@type GameMapCoverLayerState[] | nil
    self._coverLayerStates = nil
    self._coverPlayerX = nil
    self._coverPlayerY = nil
    self._coverPlayerLayerIndex = nil
    self._coverAlpha = nil
    self._coverMaterialRevision = nil
    ---@type boolean[][] | nil
    self._tilePassableGrid = nil
    ---@type Engine.Actor | nil
    self._player = nil
    ---@type ComponentBase[]
    self._components = {}
    ---@type fun(autoTileName: string): Engine.AutoTile | nil
    self._autoTileResolver = nil
    self._damageTextSpeedCurve = nil
    ---@type sf.RenderStates | nil
    self._surfaceTileRenderStates = nil
    ---@type sf.RenderStates | nil
    self._surfaceActorRenderStates = nil
    ---@type sf.RenderStates | nil
    self._transmissionTileRenderStates = nil
    ---@type sf.RenderStates | nil
    self._transmissionActorRenderStates = nil
    ---@type sf.RenderStates | nil
    self._lightPassRenderStates = nil
    ---@type sf.RenderStates | nil
    self._unobstructedLightPassRenderStates = nil
    if not self._previewOnly then
        local mapPixelSize = self._tilemap:getSize() * Engine.CellSize
        self._staticTransmission = sf.RenderTexture.new(mapPixelSize)
        self._staticTransmission:setSmooth(false)
        self._surfaceMask = sf.RenderTexture.new(mapPixelSize)
        self._surfaceMask:setSmooth(false)
        self._surfaceTileRenderStates = sf.RenderStates.new(self._camera:getRenderStates().blendMode)
        self._surfaceTileRenderStates.shader = self._tilemapLightMaskShader
        self._surfaceActorRenderStates = sf.RenderStates.new(self._camera:getRenderStates().blendMode)
        self._surfaceActorRenderStates.shader = self._lightMaskShader
        self._transmissionTileRenderStates = sf.RenderStates.new(sf.BlendMultiply)
        self._transmissionTileRenderStates.shader = self._tilemapLightMaskShader
        self._transmissionActorRenderStates = sf.RenderStates.new(sf.BlendMultiply)
        self._transmissionActorRenderStates.shader = self._lightMaskShader
        self._lightPassRenderStates = sf.RenderStates.new(sf.BlendAdd)
        self._lightPassRenderStates.shader = self._lightPassShader
        self._lightPassQuad = sf.RectangleShape.new()
        self._lightPassQuad:setFillColor(sf.Color.White)
        self._unobstructedLightPassRenderStates = sf.RenderStates.new(sf.BlendAdd)
        self._unobstructedLightPassRenderStates.shader = self._unobstructedLightPassShader
        self._unobstructedLightVertices = sf.VertexArray.new(sf.PrimitiveType.Triangles)
        self._unobstructedLightVertex = sf.Vertex.new()
    end
    self:setTilemap(self._tilemap)
    ---@type GameMap[]
    local selfRef = setmetatable({ self }, {
        __mode = "v"
    })
    self:setActorListUpdater(function ()
        local gameMap = selfRef[1]
        if gameMap ~= nil and gameMap._actorBatchDepth == 0 and not gameMap._initialisingActors then
            gameMap:updateActorList()
        end
    end)
    self:setActorDestroyer(function (actor)
        local gameMap = selfRef[1]
        if gameMap ~= nil then
            gameMap:destroyActor(actor)
        end
    end)
    self:syncActorsRef(self._wholeActorList)
    self:syncMaterialActorsRef(self._actors)
end

function GameMap:addComponent(component)
    assert(component ~= nil, "GameMap component must not be nil")
    self._components[#self._components + 1] = component
end

function GameMap:setAutoTileResolver(resolver)
    self._autoTileResolver = resolver
end

function GameMap:setDamageTextSpeedCurve(curve)
    assert(curve ~= nil, "DamageText speed curve must not be nil")
    self._damageTextSpeedCurve = curve
end

function GameMap:_syncActorsForMapCache()
    for _, actor in ipairs(self:getAllActors()) do
        actor:syncMapCache()
        actor:refreshDescendantCache()
    end
end

function GameMap:_syncActorsForPathfinding()
    for _, actor in ipairs(self:getAllActors()) do
        actor:setPathfindingBlocks(Actor.HasBlueprintEvent(actor, "onOverlap") and not actor:getCollisionEnabled())
    end
end

---@param fromPosition sf.Vector2i
---@param toPosition   sf.Vector2i
---@param direction    integer
---@return boolean
function GameMap:_checkDir4Between(fromPosition, toPosition, direction)
    local oppositeDirection = Engine.OppositeDirection(direction)
    local fromBlocked = false
    local toBlocked = false
    for _, layer in ipairs(self._layersTopFirst) do
        if layer.visible then
            if not fromBlocked then
                local tileFrom = layer:get(fromPosition)
                if tileFrom ~= nil then
                    if not layer:isDirectionPassable(fromPosition, direction) then
                        return false
                    end
                    fromBlocked = true
                end
            end
            if not toBlocked then
                local tileTo = layer:get(toPosition)
                if tileTo ~= nil then
                    if not layer:isDirectionPassable(toPosition, oppositeDirection) then
                        return false
                    end
                    toBlocked = true
                end
            end
            if fromBlocked and toBlocked then
                break
            end
        end
    end
    return true
end

function GameMap:getPlayer()
    return self._player
end

function GameMap:setPlayer(player)
    if self._camera == nil then
        return
    end
    if self._player == nil then
        self._camera:setParent(player)
    end
    self._player = player
    self:setPlayerActor(player)
    self:_updateAudioListener()
end

function GameMap:getAllActors()
    local actors = {}
    for _, actorList in pairs(self._actors) do
        for _, actor in ipairs(actorList) do
            actors[#actors + 1] = actor
        end
    end
    return actors
end

function GameMap:getActorLayer(actor)
    for layerName, actorList in pairs(self._actors) do
        for _, listed in ipairs(actorList) do
            if listed == actor then
                return layerName
            end
        end
    end
    for layerName, actorList in pairs(self._wholeActorList) do
        for _, listed in ipairs(actorList) do
            if listed == actor then
                return layerName
            end
        end
    end
    return nil
end

function GameMap:getActorsByPosition(position)
    if self._tilePassableGrid == nil or self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    return self:getActorsAt(position.x, position.y)
end

function GameMap:getActorByLayerAndPosition(layer, position)
    for _, actor in ipairs(self._actors[layer] or {}) do
        if actor:getPosition() == position then
            return actor
        end
    end
    return nil
end

function GameMap:getActorsByRange(position, radius)
    if self._tilePassableGrid == nil or self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    return self:getActorsInRange(position.x, position.y, radius)
end

function GameMap:getActorByTag(tag)
    for _, actorList in pairs(self._actors) do
        for _, actor in ipairs(actorList) do
            if actor:getMapTag() == tag then
                return actor
            end
        end
    end
    return nil
end

function GameMap:getAllActorsByTag(tag)
    local actor = self:getActorByTag(tag)
    return actor == nil and {} or { actor }
end

function GameMap:removeActorsByTags(tags)
    if not bool(tags) then
        return
    end
    local tagSet = {}
    for _, tag in ipairs(tags) do
        tagSet[tag] = true
    end
    local actorsToRemove = {}
    for _, actorList in pairs(self._actors) do
        for _, actor in ipairs(actorList) do
            if tagSet[actor:getMapTag()] then
                actorsToRemove[actor] = true
                for descendant in pairs(self:_getDescendantActorIDs(actor)) do
                    actorsToRemove[descendant] = true
                end
            end
        end
    end
    if not bool(actorsToRemove) then
        return
    end
    local removed = false
    for layerName, actorList in pairs(self._actors) do
        local keptActors = {}
        for _, actor in ipairs(actorList) do
            if actorsToRemove[actor] then
                removed = true
            else
                keptActors[#keptActors + 1] = actor
            end
        end
        self._actors[layerName] = keptActors
    end
    if removed then
        self:updateActorList()
        self._materialDirty = true
    end
end

function GameMap:applyActorPositions(actorPositions)
    if actorPositions == nil then
        return
    end
    local movedAny = false
    for actorTag, position in pairs(actorPositions) do
        local actor = self:getActorByTag(actorTag)
        if actor ~= nil then
            actor:setMapPosition(position)
            movedAny = true
        end
    end
    if movedAny then
        self:updateActorList()
        self:markPassabilityDirty()
    end
end

function GameMap:isPassable(actor, targetPosition)
    if not actor:getCollisionEnabled() then
        return true
    end
    local size = self._tilemap:getSize()
    if self._tilePassableGrid == nil or self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    ---@type sf.Vector2i[]
    local occupied = actor:getOccupiedMapCellsAtMapPosition(targetPosition)
    for _, cell in ipairs(occupied) do
        if cell.x < 0 or cell.y < 0 or cell.x >= size.x or cell.y >= size.y then
            return false
        end
        local passableGrid = self._tilePassableGrid
        if passableGrid ~= nil then
            local passableRow = passableGrid[cell.y + 1]
            ---@cast passableRow boolean[]
            if not passableRow[cell.x + 1] then
                return false
            end
        end
    end
    local currentPosition = actor:getMapPosition()
    local delta = sf.Vector2i.new(targetPosition.x - currentPosition.x, targetPosition.y - currentPosition.y)
    local direction = nil
    if delta.x == 0 and delta.y == 1 then
        direction = Engine.Direction.DOWN
    elseif delta.x == 0 and delta.y == -1 then
        direction = Engine.Direction.UP
    elseif delta.x == 1 and delta.y == 0 then
        direction = Engine.Direction.RIGHT
    elseif delta.x == -1 and delta.y == 0 then
        direction = Engine.Direction.LEFT
    end
    if direction ~= nil then
        ---@type dict<tuple<any>, boolean>
        local currentCells = dict()
        for _, cell in ipairs(actor:getOccupiedMapCellsAtMapPosition(currentPosition)) do
            currentCells[tuple { cell.x, cell.y }] = true
        end
        for _, cell in ipairs(occupied) do
            if not currentCells:get(tuple { cell.x, cell.y }) then
                local previousX = cell.x - delta.x
                local previousY = cell.y - delta.y
                local previousPosition = sf.Vector2i.new(previousX, previousY)
                ---@cast previousPosition sf.Vector2i
                local occupiedPosition = sf.Vector2i.new(cell.x, cell.y)
                ---@cast occupiedPosition sf.Vector2i
                if not self:_checkDir4Between(previousPosition, occupiedPosition, direction) then
                    return false
                end
            end
        end
    end
    for _, cell in ipairs(occupied) do
        if bool(self:getCollisionAt(cell.x, cell.y, actor)) then
            return false
        end
    end
    return true
end

function GameMap:spawnActor(actor, layer, emitCreateEvent)
    if emitCreateEvent == nil then
        emitCreateEvent = true
    end
    self:_addActorTreeToLayer(actor, layer)
    if self._actorBatchDepth == 0 and not self._initialisingActors then
        self:updateActorList()
    end
    self._materialDirty = true
    if emitCreateEvent and self._actorBatchDepth == 0 and not self._initialisingActors then
        self:initialiseActorsAndComponents()
    end
end

function GameMap:beginActorBatch()
    self._actorBatchDepth = self._actorBatchDepth + 1
end

function GameMap:endActorBatch()
    assert(self._actorBatchDepth > 0, "Actor batch is not active")
    self._actorBatchDepth = self._actorBatchDepth - 1
    if self._actorBatchDepth > 0 then
        return
    end
    self:updateActorList()
end

function GameMap:createActor(actorClass, layer, kwargs, emitCreateEvent)
    if emitCreateEvent == nil then
        emitCreateEvent = true
    end
    local actor = Class.constructNamed(actorClass, kwargs or {})
    if actor.material ~= nil and not Class.isInstance(actor.material, Engine.Material) then
        local values = Engine.filterDataClassParams(actor.material, Engine.Material)
        actor.material = Engine.Material.new(
            values.lightBlock, values.mirror, values.reflectionStrength, values.opacity, values.speedRate,
            values.ignoreLighting
        )
    end
    self:spawnActor(actor, layer, emitCreateEvent)
    return actor
end

function GameMap:initialiseActorsAndComponents()
    if self._initialisingActors then
        return
    end
    self._initialisingActors = true
    while true do
        local createdAny = self:_initialisePendingActorCreateEvents()
        local componentAny = self:_initialisePendingActorComponents()
        if not createdAny and not componentAny then
            break
        end
    end
    self._initialisingActors = false
    self:updateActorList()
    self._materialDirty = true
end

---@return boolean
function GameMap:_initialisePendingActorCreateEvents()
    local createdAny = false
    while true do
        local pendingActors = {}
        for _, actor in ipairs(self:getAllActors()) do
            if not self._createInitialisedActorIDs[actor] and not actor:isDestroyed() then
                pendingActors[#pendingActors + 1] = actor
            end
        end
        if not bool(pendingActors) then
            return createdAny
        end
        for _, actor in ipairs(pendingActors) do
            if not self._createInitialisedActorIDs[actor] then
                self._createInitialisedActorIDs[actor] = true
                Actor.BlueprintEvent(actor, Actor, "onCreate")
                createdAny = true
            end
        end
    end
    return createdAny
end

---@return boolean
function GameMap:_initialisePendingActorComponents()
    local componentAny = false
    local pendingActors = {}
    for _, actor in ipairs(self:getAllActors()) do
        if not self._componentInitialisedActorIDs[actor] and not actor:isDestroyed() then
            pendingActors[#pendingActors + 1] = actor
        end
    end
    for _, actor in ipairs(pendingActors) do
        if not self._componentInitialisedActorIDs[actor] then
            self._componentInitialisedActorIDs[actor] = true
            ComponentsFunctions.attachInstanceComponents(actor)
            componentAny = true
        end
    end
    return componentAny
end

---@param actor Engine.Actor
---@param layer string
function GameMap:_addActorTreeToLayer(actor, layer)
    self:_addActorToLayer(actor, layer)
    for _, child in ipairs(actor:getChildren()) do
        self:_addActorTreeToLayer(child, layer)
    end
end

---@param actor Engine.Actor
---@param layer string
function GameMap:_addActorToLayer(actor, layer)
    if self._actors[layer] == nil then
        self._actors[layer] = {}
    end
    actor:setMap(self)
    actor:ensureMapTag()
    for _, listed in ipairs(self._actors[layer]) do
        if listed == actor then
            return
        end
    end
    self._actors[layer][#self._actors[layer] + 1] = actor
end

function GameMap:destroyActor(actor)
    self._actorsOnDestroy[#self._actorsOnDestroy + 1] = actor
    self._materialDirty = true
end

function GameMap:playActorPixelShatterEffect(actor)
    if self._previewOnly or self._actorPixelShatterShader == nil or actor:isDestroyed()
        or self._actorPixelShatterByActor[actor] ~= nil then
        return false
    end
    local layerName = self:getActorLayer(actor)
    if layerName == nil then
        return false
    end
    local layer = self._tilemap:getLayer(layerName)
    if layer == nil or not layer.visible then
        return false
    end
    self._actorPixelShatterSeed = self._actorPixelShatterSeed + 1
    local effect = ActorPixelShatterEffect.new(
        actor, self._actorPixelShatterShader, self._actorPixelShatterSeed
    )
    local layerEffects = self._actorPixelShatterEffects[layerName]
    if layerEffects == nil then
        layerEffects = {}
        self._actorPixelShatterEffects[layerName] = layerEffects
    end
    layerEffects[#layerEffects + 1] = effect
    self._actorPixelShatterByActor[actor] = effect
    return true
end

function GameMap:getCamera()
    return self._camera
end

function GameMap:setCamera(camera)
    self._camera = camera
end

function GameMap:getTilemap()
    return self._tilemap
end

function GameMap:getTerrainTile(layerName, position)
    local layer = self._tilemap:getLayer(layerName)
    if layer == nil then
        return nil
    end
    if not self:_isTerrainPositionInLayer(layer, position) then
        return nil
    end
    return self:_getTerrainTileID(layer, position)
end

function GameMap:getTerrainTilePositions(layerName, tileID)
    local layer = self._tilemap:getLayer(layerName)
    if layer == nil then
        return {}
    end
    local terrainTileID = self:_normaliseTerrainTileID(tileID)
    local positions = {}
    local size = layer:getGridSize()
    for y = 0, size.y - 1 do
        for x = 0, size.x - 1 do
            local terrainPosition = sf.Vector2i.new(x, y)
            ---@cast terrainPosition sf.Vector2i
            if self:_getTerrainTileID(layer, terrainPosition) == terrainTileID then
                positions[#positions + 1] = terrainPosition
            end
        end
    end
    return positions
end

function GameMap:setPersistentMapPath(mapPath)
    self._persistentMapPath = mapPath
end

function GameMap:setTerrainTile(layerName, position, tileID)
    return bool(self:_setTerrainTiles(layerName, { position }, tileID))
end

function GameMap:setTerrainTiles(layerName, positions, tileID)
    return #self:_setTerrainTiles(layerName, positions, tileID)
end

---@param layerName string
---@param positions sf.Vector2i[]
---@param tileID    integer | string | nil
---@return sf.Vector2i[]
function GameMap:_setTerrainTiles(layerName, positions, tileID)
    if not bool(positions) then
        return {}
    end
    local layer = self._tilemap:getLayer(layerName)
    if not bool(layer) then
        return {}
    end
    ---@cast layer Engine.TileLayer
    local terrainTileID = self:_normaliseTerrainTileID(tileID)
    local layerData = layer:getData()
    local autoTileTextures = layer:getAutoTileTextures()
    local autoTileFrameCounts = layer:getAutoTileFrameCounts()
    local changedPositions = {}
    for _, position in ipairs(positions) do
        if self:_isTerrainPositionInLayer(layer, position) then
            self:_writeTerrainTile(layer, layerData, autoTileTextures, autoTileFrameCounts, position, terrainTileID)
            changedPositions[#changedPositions + 1] = position
        end
    end
    if not bool(changedPositions) then
        return {}
    end
    self:_replaceTerrainLayer(layerName, layer, layerData, autoTileTextures, autoTileFrameCounts)
    self:markPassabilityDirty()
    return changedPositions
end

function GameMap:destroyTerrain(layerName, position, tileID)
    local terrainTileID = self:_normaliseTerrainTileID(tileID)
    local changedPositions = self:_setTerrainTiles(layerName, { position }, terrainTileID)
    if not bool(changedPositions) then
        return
    end
    local scene = self._scene
    ---@cast scene Source.Scenes.SceneMap.SceneMap | nil
    if scene ~= nil and scene.inst ~= nil then
        local changedPosition = changedPositions[1]
        ---@cast changedPosition sf.Vector2i
        scene.inst:recordTerrainDestruction(self._persistentMapPath, layerName, changedPosition, terrainTileID)
    end
end

function GameMap:destroyTerrainList(layerName, positions, tileID)
    local terrainTileID = self:_normaliseTerrainTileID(tileID)
    local changedPositions = self:_setTerrainTiles(layerName, positions, terrainTileID)
    if not bool(changedPositions) then
        return
    end
    local scene = self._scene
    ---@cast scene Source.Scenes.SceneMap.SceneMap | nil
    if scene ~= nil and scene.inst ~= nil then
        for _, terrainPosition in ipairs(changedPositions) do
            scene.inst:recordTerrainDestruction(self._persistentMapPath, layerName, terrainPosition, terrainTileID)
        end
    end
end

function GameMap:applyTerrainDestructions(terrainDestructions)
    for layerName, changes in pairs(terrainDestructions) do
        for _, change in pairs(changes) do
            self:setTerrainTile(layerName, change.position, change.tileID)
        end
    end
end

function GameMap:getLights()
    return self._lights
end

function GameMap:setLights(lights)
    self._lights = lights
end

function GameMap:addLight(light)
    self._lights[#self._lights + 1] = light
end

function GameMap:removeLight(light)
    for index, listed in ipairs(self._lights) do
        if listed == light then
            table.remove(self._lights, index)
            return
        end
    end
    error("Light not found in map", 2)
end

---@param light GlobalCore.Light
function GameMap:_requireLight(light)
    for _, listed in ipairs(self._lights) do
        if listed == light then
            return
        end
    end
    error("Light not found in map", 3)
end

function GameMap:setLightPosition(light, position)
    self:_requireLight(light)
    light.position = position
end

function GameMap:setLightColour(light, colour)
    self:_requireLight(light)
    light.colour = colour
end

function GameMap:setLightRadius(light, radius)
    self:_requireLight(light)
    light.radius = radius
end

function GameMap:setLightIntensity(light, intensity)
    self:_requireLight(light)
    light.intensity = intensity
end

function GameMap:getAmbientLight()
    return self._ambientLight
end

function GameMap:setAmbientLight(ambientLight)
    self._ambientLight = ambientLight
end

function GameMap:getSize()
    return self._tilemap:getSize()
end

function GameMap:getMapViewOffset()
    local offset = GameMap.MapViewOffset
    local gameSize = System.getGameSize()
    local mapSize = self._tilemap:getSize() * Engine.CellSize
    return sf.Vector2f.new(
        offset.x + (mapSize.x < gameSize.x and (gameSize.x - mapSize.x) / 2.0 or 0.0),
        offset.y + (mapSize.y < gameSize.y and (gameSize.y - mapSize.y) / 2.0 or 0.0)
    )
end

function GameMap:getTopMaterial(pos)
    local layerKeys = self._layerNames
    for index = #layerKeys, 1, -1 do
        local layerName = layerKeys[index]
        local layer = self._tilemap:getLayer(layerName)
        if layer ~= nil and layer.visible then
            for _, actor in ipairs(self._actors[layerName] or {}) do
                if actor ~= self._player and actor:getMapPosition() == pos then
                    return actor:getMaterial()
                end
            end
            local material = layer:getMaterial(pos)
            if material ~= nil then
                return material
            end
        end
    end
    return nil
end

function GameMap:findPathResult(start, goal, actor, excludedAnchors)
    self:_syncActorsForPathfinding()
    local result = self:findPathExt(start, goal, self._tilemap:getSize(), actor, excludedAnchors or {})
    self:_clearActorsPathfindingBlocks()
    return result
end

function GameMap:_clearActorsPathfindingBlocks()
    for _, actor in ipairs(self:getAllActors()) do
        actor:setPathfindingBlocks(false)
    end
end

function GameMap:findPath(start, goal, actor, excludedAnchors)
    return self:findPathResult(start, goal, actor, excludedAnchors).offsets
end

function GameMap:isPathfindingPassable(actor, targetPosition)
    if not self:isPassable(actor, targetPosition) then
        return false
    end
    return not self:hasPathBlockingOverlapActor(actor, targetPosition)
end

function GameMap:hasPathBlockingOverlapActor(actor, targetPosition)
    if self._tilePassableGrid == nil or self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    for _, cell in ipairs(actor:getOccupiedMapCellsAtMapPosition(targetPosition)) do
        for _, other in ipairs(self:getOverlapsAt(cell.x, cell.y, actor)) do
            if Actor.HasBlueprintEvent(other, "onOverlap") and not other:getCollisionEnabled() then
                return true
            end
        end
    end
    return false
end

function GameMap:getScene()
    return self._scene
end

function GameMap:setScene(scene)
    self._scene = scene
end

function GameMap:addCommonTip(text)
    if self._scene ~= nil then
        self._scene:addCommonTip(text)
    end
end

function GameMap:addDamageText(text, position, textConfigKey)
    assert(self._damageTextSpeedCurve ~= nil, "DamageText speed curve is not configured")
    local drawPosition = self:worldToMapViewPosition(position)
    local Data = require("Source.Data")

    local textConfig = Data.getPlainTextConfig(textConfigKey or "Global/DamageText")
    DamageTextParticle.new(self._particleSystem, text, drawPosition, textConfig, self._damageTextSpeedCurve)
end

function GameMap:worldToMapViewPosition(position)
    local camera = self:getCamera()
    if camera == nil then
        return sf.Vector2f.new(position.x, position.y)
    end
    local viewPosition = camera:getViewPosition()
    if viewPosition == nil then
        return sf.Vector2f.new(position.x, position.y)
    end
    return sf.Vector2f.new(position.x - viewPosition.x, position.y - viewPosition.y)
end

function GameMap:worldToUIScreenPosition(position)
    local mapPosition = self:worldToMapViewPosition(position)
    local mapOffset = self:getMapViewOffset()
    return sf.Vector2f.new(mapPosition.x + mapOffset.x, mapPosition.y + mapOffset.y)
end

function GameMap:worldToCanvasPosition(position)
    local uiPosition = self:worldToUIScreenPosition(position)
    local scale = System.getScale()
    return sf.Vector2f.new(uiPosition.x * scale, uiPosition.y * scale)
end

function GameMap:getCollision(actor, targetPosition)
    if not actor:getCollisionEnabled() then
        return {}
    end
    if self._tilePassableGrid == nil or self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    local collisions = {}
    local seen = {}
    for _, cell in ipairs(actor:getOccupiedMapCellsAtMapPosition(targetPosition)) do
        for _, other in ipairs(self:getCollisionAt(cell.x, cell.y, actor)) do
            if not seen[other] then
                seen[other] = true
                collisions[#collisions + 1] = other
            end
        end
    end
    return collisions
end

function GameMap:getOverlaps(actor)
    if self._tilePassableGrid == nil or self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    local overlaps = {}
    local seen = {}
    for _, cell in ipairs(actor:getOccupiedMapCells()) do
        for _, other in ipairs(self:getOverlapsAt(cell.x, cell.y, actor)) do
            if not seen[other] then
                seen[other] = true
                overlaps[#overlaps + 1] = other
            end
        end
    end
    return overlaps
end

---@param actor Engine.Actor
---@return table<Engine.Actor, boolean>
function GameMap._getDescendantActorIDs(_self, actor)
    local descendantActors = {}
    local stack = {}
    for _, child in ipairs(actor:getChildren()) do
        stack[#stack + 1] = child
    end
    while bool(stack) do
        local child = table.remove(stack)
        if not descendantActors[child] then
            descendantActors[child] = true
            for _, nested in ipairs(child:getChildren()) do
                stack[#stack + 1] = nested
            end
        end
    end
    return descendantActors
end

function GameMap:_updateAudioListener()
    if self._player == nil then
        return
    end
    local position = self._player:getPosition()
    local listenerVector = Pool.Get("sf.Vector3f", sf.Vector3f, {
        x = position.x,
        y = position.y,
        z = 0.0
    })
    sf.Listener.setPosition(listenerVector)
    Pool.Put("sf.Vector3f", listenerVector)
    if Class.isInstance(self._player, Character) then
        ---@cast self._player Engine.Character
        sf.Listener.setDirection(self:_getAudioListenerDirection(self._player.direction))
        sf.Listener.setUpVector(CHARACTER_LISTENER_UP_VECTOR)
    else
        sf.Listener.setDirection(DEFAULT_LISTENER_DIRECTION)
        sf.Listener.setUpVector(DEFAULT_LISTENER_UP_VECTOR)
    end
end

---@param direction integer
---@return sf.Vector3f
function GameMap._getAudioListenerDirection(_self, direction)
    if direction == Engine.Direction.UP then
        return LISTENER_DIRECTION_UP
    elseif direction == Engine.Direction.LEFT then
        return LISTENER_DIRECTION_LEFT
    elseif direction == Engine.Direction.RIGHT then
        return LISTENER_DIRECTION_RIGHT
    end
    return LISTENER_DIRECTION_DOWN
end

function GameMap:updateActorList()
    self._wholeActorList = {}
    self._actorUpdateList = {}
    for layerName, actorList in pairs(self._actors) do
        self._wholeActorList[layerName] = {}
        ---@type Engine.Actor[]
        local queue = {}
        for _, actor in ipairs(actorList) do
            queue[#queue + 1] = actor
            self._actorUpdateList[#self._actorUpdateList + 1] = actor
        end
        local index = 1
        while index <= #queue do
            local child = queue[index]
            ---@cast child Engine.Actor
            index = index + 1
            child:setMap(self)
            self._wholeActorList[layerName][#self._wholeActorList[layerName] + 1] = child
            for _, nested in ipairs(child:getChildren()) do
                queue[#queue + 1] = nested
            end
        end
    end
    self:syncActorsRef(self._wholeActorList)
    self:syncMaterialActorsRef(self._actors)
    self._actorUpdateBatch:syncActors(self._actorUpdateList)
end

function GameMap:getMaterialPropertyMap(functionName, invalidValue)
    local mapSize = self._tilemap:getSize()
    return self:getMaterialPropertyMapExt(mapSize.x, mapSize.y, functionName, invalidValue)
end

function GameMap:getActorLayerLightBlockMap(layerName, size)
    if self._actors[layerName] == nil then
        return nil
    end
    local result = {}
    for y = 1, size.y do
        result[y] = {}
        for x = 1, size.x do
            result[y][x] = 0.0
        end
    end
    for _, actor in ipairs(self._actors[layerName]) do
        local position = actor:getMapPosition()
        result[position.y + 1][position.x + 1] = actor:getLightBlock()
    end
    return result
end

function GameMap:onTick(deltaTime)
    self._shaderTime = self._shaderTime + deltaTime
    if self._camera ~= nil then
        self._camera:onTick(deltaTime)
    end
    for _, component in ipairs(self._components) do
        component:onTick(deltaTime)
    end
    if bool(self._actorsOnDestroy) then
        for _, actor in ipairs(self._actorsOnDestroy) do
            for _, actorList in pairs(self._actors) do
                for index, listed in ipairs(actorList) do
                    if listed == actor then
                        table.remove(actorList, index)
                        self._actorPixelShatterByActor[actor] = nil
                        Actor.BlueprintEvent(actor, Actor, "onDestroy")
                        break
                    end
                end
            end
        end
        self:updateActorList()
        self._actorsOnDestroy = {}
    end
    self:_updateActorPixelShatterEffects(deltaTime)
    self._actorUpdateBatch:update(deltaTime)
    self:_updateAudioListener()
    ---@cast self._particleSystem Engine.ParticleSystem
    self._particleSystem:onTick(deltaTime)
end

function GameMap:_updateActorPixelShatterEffects(deltaTime)
    for layerName, effects in pairs(self._actorPixelShatterEffects) do
        local activeEffects = {}
        for _, effect in ipairs(effects) do
            effect:onTick(deltaTime)
            if effect:isFinished() then
                for actor, actorEffect in pairs(self._actorPixelShatterByActor) do
                    if actorEffect == effect then
                        self._actorPixelShatterByActor[actor] = nil
                        break
                    end
                end
            else
                activeEffects[#activeEffects + 1] = effect
            end
        end
        if bool(activeEffects) then
            self._actorPixelShatterEffects[layerName] = activeEffects
        else
            self._actorPixelShatterEffects[layerName] = nil
        end
    end
end

function GameMap:onLateTick(deltaTime)
    if self._camera ~= nil then
        self._camera:onLateTick(deltaTime)
    end
    for _, component in ipairs(self._components) do
        component:onLateTick(deltaTime)
    end
    self._actorUpdateBatch:lateUpdate(deltaTime)
    ---@cast self._particleSystem Engine.ParticleSystem
    self._particleSystem:onLateTick(deltaTime)
end

function GameMap:onFixedTick(fixedDelta)
    for _, component in ipairs(self._components) do
        component:onFixedTick(fixedDelta)
    end
    self._actorUpdateBatch:fixedUpdate(fixedDelta)
    if self._camera ~= nil then
        self._camera:onFixedTick(fixedDelta)
    end
end

function GameMap:drawMapContent(target, states, applyPlayerCover)
    self:_prepareActorPixelShatterEffects()
    states = states or sf.RenderStates.new()
    local layerKeys = self._layerNames
    local playerLayerIndex = applyPlayerCover and self:_getPlayerLayerIndex(layerKeys) or -1
    local refreshPlayerCover = false
    local playerPosition = nil
    if applyPlayerCover then
        refreshPlayerCover, playerPosition = self:_preparePlayerCover(layerKeys, playerLayerIndex)
    end
    for index, layerName in ipairs(layerKeys) do
        local layer = self._tilemap:getLayer(layerName)
        ---@cast layer Engine.TileLayer
        if layer.visible then
            if refreshPlayerCover then
                ---@cast playerPosition sf.Vector2i
                self:_applyPlayerCover(layer, index - 1, playerLayerIndex, playerPosition)
            end
            local layerStates = states
            if layer.shader ~= nil then
                layerStates = sf.RenderStates.new(states.blendMode)
                layerStates.transform = states.transform
                layerStates.texture = states.texture
                layerStates.shader = layer.shader
            end
            target:draw(layer, layerStates)
            self:_drawLayerActors(target, states, layerName, index - 1, playerLayerIndex, applyPlayerCover == true)
        end
    end
end

function GameMap:show()
    if self._camera ~= nil then
        self._camera:syncFollowTarget()
    end
    local mapViewOffset = self:getMapViewOffset()
    System.setWindowMapView(mapViewOffset)
    if self._camera ~= nil then
        self._camera:clear()
    end
    if self._camera ~= nil then
        self:drawMapContent(self._camera:getRenderTexture(), self._camera:getRenderStates(), true)
    else
        self:_resetTransparentTiles()
    end
    do
        local componentCamera = self._camera
        ---@cast componentCamera GlobalCore.Camera
        for _, component in ipairs(self._components) do
            component:onRender(componentCamera)
        end
    end
    if self:_lightingShadersAvailable() then
        self:_renderLighting(self:_getActiveLights())
    end
    if self._camera ~= nil then
        self._camera:display()
    end
    self:refreshShader()
    if self._camera ~= nil then
        WeatherController.drawShaderOverlay(self._camera)
    end
    local renderCamera = self._camera
    ---@cast renderCamera GlobalCore.Camera
    System.draw(renderCamera, self._materialShader)
    FogController.drawOverlay()
    local particleSystem = self._particleSystem
    ---@cast particleSystem Engine.ParticleSystem
    WeatherController.registerParticleSystem(particleSystem)
    System.draw(particleSystem)
    System.setWindowDefaultView()
end

function GameMap:refreshShader()
    if self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    if self._camera == nil or self._materialShader == nil or self._surfaceMask == nil or self._directLight == nil then
        return
    end
    local screenSize = self._camera:getViewSize()
    ---@cast screenSize sf.Vector2f
    self._materialShader:setUniform("surfaceMask", self._surfaceMask:getTexture())
    self._materialShader:setUniform("directLight", self._directLight:getTexture())
    if self._staticDirectLight ~= nil then
        self._materialShader:setUniform("staticDirectLight", self._staticDirectLight:getTexture())
    end
    self._materialShader:setUniform("useStaticDirectLight", self._useStaticDirectLight and 1.0 or 0.0)
    self:_setViewShaderUniforms(self._materialShader, screenSize, self:getMapViewOffset(), false)
    self._materialShader:setUniform("ambientColor", self:_toShaderColour(self._ambientLight, true))
end

---@return boolean
function GameMap:_lightingShadersAvailable()
    return self._camera ~= nil and self._materialShader ~= nil and self._tilemapLightMaskShader ~= nil
        and self._lightMaskShader ~= nil and self._lightPassShader ~= nil and self._unobstructedLightPassShader ~= nil
        and self._staticTransmission ~= nil and self._surfaceMask ~= nil
end

---@param activeLights table[]
function GameMap:_renderLighting(activeLights)
    if self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    self:_rebuildStaticTransmission()
    local visibleActors = self:_renderSurfaceMask()
    self:_ensureDirectLight()
    ---@cast self._directLight sf.RenderTexture
    ---@cast self._camera GlobalCore.Camera
    self._useStaticDirectLight = bool(activeLights)
        and not self:_hasRelevantLightBlockingActors(activeLights, visibleActors)
    if self._useStaticDirectLight then
        self:_renderCachedLighting(activeLights)
        return
    end
    self._directLight:setView(self._camera:getView())
    self._directLight:clear(sf.Color.Black)
    if bool(activeLights) then
        self:_renderDynamicLighting(activeLights, visibleActors)
    end
    self._directLight:display()
end

---@param activeLights  table[]
---@param visibleActors table<Engine.Actor, boolean>
---@return boolean
function GameMap:_hasRelevantLightBlockingActors(activeLights, visibleActors)
    for actor in pairs(visibleActors) do
        if not actor:isDestroyed() and actor:getLightBlock() > 0.0 then
            for _, entry in ipairs(activeLights) do
                if actor ~= entry.owner and self:_actorIntersectsLight(actor, entry.light) then
                    return true
                end
            end
        end
    end
    return false
end

---@param activeLights  table[]
---@param visibleActors table<Engine.Actor, boolean>
function GameMap:_renderDynamicLighting(activeLights, visibleActors)
    ---@cast self._directLight sf.RenderTexture
    local unobstructedLights = {}
    local staticLights = {}
    self:_ensureDynamicTransmission(activeLights)
    local commonUniformsSet = false
    for _, entry in ipairs(activeLights) do
        local hasStaticTransmissionLoss = self:_lightHasStaticTransmissionLoss(entry.light)
        local dynamicOrigin, dynamicSize, hasDynamicTransmissionLoss = self:_renderDynamicTransmission(
            entry.light, entry.owner, visibleActors
        )
        if hasDynamicTransmissionLoss then
            if not commonUniformsSet then
                self:_setLightPassCommonUniforms()
                commonUniformsSet = true
            end
            self:_renderLight(entry, dynamicOrigin, dynamicSize, hasStaticTransmissionLoss, true, self._directLight)
        elseif hasStaticTransmissionLoss then
            staticLights[#staticLights + 1] = entry
        else
            unobstructedLights[#unobstructedLights + 1] = entry
        end
    end
    if bool(staticLights) then
        if not commonUniformsSet then
            self:_setLightPassCommonUniforms()
        end
        self:_renderStaticLights(staticLights, self._directLight)
    end
    self:_renderUnobstructedLights(unobstructedLights, self._directLight)
end

---@param activeLights table[]
function GameMap:_renderCachedLighting(activeLights)
    self:_ensureStaticDirectLight()
    ---@cast self._staticDirectLight sf.RenderTexture
    local cacheValid = self._cachedLightMaterialRevision == self._materialRevision and self._cachedLightTransmissionSignature
            == self._staticTransmissionSignature and self:_lightsMatchCache(activeLights, self._cachedActiveLights)
    if cacheValid then
        return
    end
    local unobstructedLights = {}
    local staticLights = {}
    for _, entry in ipairs(activeLights) do
        if self:_lightHasStaticTransmissionLoss(entry.light) then
            staticLights[#staticLights + 1] = entry
        else
            unobstructedLights[#unobstructedLights + 1] = entry
        end
    end
    self._staticDirectLight:setView(self._staticDirectLight:getDefaultView())
    self._staticDirectLight:clear(sf.Color.Black)
    if bool(staticLights) then
        self:_setLightPassWorldUniforms()
        self:_renderStaticLights(staticLights, self._staticDirectLight)
    end
    self:_renderUnobstructedLights(unobstructedLights, self._staticDirectLight)
    self._staticDirectLight:display()
    self._cachedActiveLights = self:_cacheLightList(activeLights)
    self._cachedLightMaterialRevision = self._materialRevision
    self._cachedLightTransmissionSignature = self._staticTransmissionSignature
end

function GameMap:_rebuildStaticTransmission()
    local transmissionSignature = self:_getStaticTransmissionSignature()
    if self._staticTransmissionRevision == self._materialRevision
        and self._staticTransmissionSignature == transmissionSignature then
        return
    end
    ---@cast self._staticTransmission sf.RenderTexture
    ---@cast self._transmissionTileRenderStates sf.RenderStates
    self._staticTransmission:clear(sf.Color.White)
    self._tilemapLightMaskShader:setUniform("transmissionMode", 1.0)
    local size = self._tilemap:getSize()
    ---@type number[][]
    local occupancy = {}
    for y = 1, size.y do
        local row = {}
        for x = 1, size.x do
            row[x] = 0.0
        end
        occupancy[y] = row
    end
    for _, layerName in ipairs(self._layerNames) do
        local layer = self._tilemap:getLayer(layerName)
        ---@cast layer Engine.TileLayer
        if layer.visible then
            local lightBlockMap = layer:getLightBlockMap()
            for y, row in ipairs(lightBlockMap) do
                local occupancyRow = occupancy[y]
                ---@cast occupancyRow number[]
                for x, lightBlock in ipairs(row) do
                    if lightBlock > 0.0 then
                        occupancyRow[x] = 1.0
                    end
                end
            end
            self:_setTileMaskUniforms(layerName, layer)
            local drawable = layer
            ---@cast drawable sf.Drawable
            self._staticTransmission:draw(drawable, self._transmissionTileRenderStates)
        end
    end
    self._staticTransmission:display()
    self:_rebuildStaticOccupancy(size, occupancy)
    self._staticTransmissionRevision = self._materialRevision
    self._staticTransmissionSignature = transmissionSignature
end

---@param size      sf.Vector2u
---@param occupancy number[][]
function GameMap:_rebuildStaticOccupancy(size, occupancy)
    ---@type number[][]
    local prefix = {}
    ---@type number[]
    local firstPrefixRow = {}
    prefix[1] = firstPrefixRow
    for x = 1, size.x + 1 do
        firstPrefixRow[x] = 0
    end
    local textureRows = {}
    for y = 1, size.y do
        ---@type number[]
        local prefixRow = { 0 }
        local previousPrefixRow = prefix[y]
        ---@cast previousPrefixRow number[]
        local occupancyRow = occupancy[y]
        ---@cast occupancyRow number[]
        for x = 1, size.x do
            local occupancyValue = occupancyRow[x]
            ---@cast occupancyValue number
            local previousPrefixValue = previousPrefixRow[x + 1]
            ---@cast previousPrefixValue number
            local prefixValue = prefixRow[x]
            ---@cast prefixValue number
            local previousRowPrefixValue = previousPrefixRow[x]
            ---@cast previousRowPrefixValue number
            prefixRow[x + 1] = occupancyValue + previousPrefixValue + prefixValue - previousRowPrefixValue
        end
        prefix[y + 1] = prefixRow
        textureRows[size.y - y + 1] = occupancyRow
    end
    self._staticOccupancyPrefix = prefix
    local lastPrefixRow = prefix[size.y + 1]
    ---@cast lastPrefixRow number[]
    local lastPrefixValue = lastPrefixRow[size.x + 1]
    ---@cast lastPrefixValue number
    self._staticHasTransmissionLoss = lastPrefixValue > 0
    self._staticOccupancy = self:generateDataFromMap(size, textureRows, false)
end

---@return tuple<any>
function GameMap:_getStaticTransmissionSignature()
    local states = {}
    for _, layerName in ipairs(self._layerNames) do
        local layer = self._tilemap:getLayer(layerName)
        ---@cast layer Engine.TileLayer
        states[#states + 1] = bool(layer.visible)
    end
    if self._coverPlayerX == nil or self._coverPlayerY == nil
        or self._coverPlayerLayerIndex == nil or self._coverAlpha == nil then
        states[#states + 1] = tuple { "none" }
    else
        states[#states + 1] = tuple {
            "cover", self._coverPlayerX, self._coverPlayerY, self._coverPlayerLayerIndex, self._coverAlpha
        }
    end
    return tuple(states)
end

---@return table<Engine.Actor, boolean>
function GameMap:_renderSurfaceMask()
    local visibleActors = {}
    ---@cast self._surfaceMask sf.RenderTexture
    ---@cast self._surfaceTileRenderStates sf.RenderStates
    ---@cast self._surfaceActorRenderStates sf.RenderStates
    self._surfaceMask:clear(sf.Color.Transparent)
    self._tilemapLightMaskShader:setUniform("transmissionMode", 0.0)
    self._lightMaskShader:setUniform("transmissionMode", 0.0)
    for _, layerName in ipairs(self._layerNames) do
        local layer = self._tilemap:getLayer(layerName)
        ---@cast layer Engine.TileLayer
        if layer.visible then
            self:_setTileMaskUniforms(layerName, layer)
            local drawable = layer
            ---@cast drawable sf.Drawable
            self._surfaceMask:draw(drawable, self._surfaceTileRenderStates)
            for _, actor in ipairs(self._actors[layerName] or {}) do
                if not actor:isDestroyed() then
                    visibleActors[actor] = true
                    self:_setActorMaskUniforms(actor)
                    self._surfaceMask:draw(actor, self._surfaceActorRenderStates)
                end
            end
        end
    end
    self._surfaceMask:display()
    return visibleActors
end

---@param layerName string
---@param layer     Engine.TileLayer
function GameMap:_setTileMaskUniforms(layerName, layer)
    local lightBlockImage = layer:getLightBlockImage()
    local reflectionStrengthImage = layer:getReflectionStrengthImage()
    local ignoreLightingImage = layer:getIgnoreLightingImage()
    ---@cast lightBlockImage sf.Image
    ---@cast reflectionStrengthImage sf.Image
    ---@cast ignoreLightingImage sf.Image
    local cached = self._layerMaskTextureCache[layerName]
    ---@type sf.Texture
    local lightBlockTexture
    ---@type sf.Texture
    local reflectionStrengthTexture
    ---@type sf.Texture
    local ignoreLightingTexture
    if cached == nil or cached[1] ~= lightBlockImage
        or cached[2] ~= reflectionStrengthImage or cached[3] ~= ignoreLightingImage then
        lightBlockTexture = sf.Texture.new(lightBlockImage)
        reflectionStrengthTexture = sf.Texture.new(reflectionStrengthImage)
        ignoreLightingTexture = sf.Texture.new(ignoreLightingImage)
        cached = {
            lightBlockImage, reflectionStrengthImage, ignoreLightingImage, lightBlockTexture, reflectionStrengthTexture,
            ignoreLightingTexture
        }
        ---@cast cached GameMapLayerMaskTextureCacheEntry
        self._layerMaskTextureCache[layerName] = cached
    else
        lightBlockTexture = cached[4]
        reflectionStrengthTexture = cached[5]
        ignoreLightingTexture = cached[6]
    end
    self._tilemapLightMaskShader:setUniform("lightBlockTex", lightBlockTexture)
    self._tilemapLightMaskShader:setUniform("reflectionStrengthTex", reflectionStrengthTexture)
    self._tilemapLightMaskShader:setUniform("ignoreLightingTex", ignoreLightingTexture)
    self._tilemapLightMaskShader:setUniform("lightBlockSize", self._lightBlockSize)
    self._tilemapLightMaskShader:setUniform("mapSize", self._shaderMapSize)
end

---@param actor Engine.Actor
function GameMap:_setActorMaskUniforms(actor)
    self._lightMaskShader:setUniform("lightBlock", actor:getLightBlock())
    self._lightMaskShader:setUniform("reflectionStrength", actor:getMirror() and actor:getReflectionStrength() or 0.0)
    self._lightMaskShader:setUniform("ignoreLighting", actor:getIgnoreLighting() and 1.0 or 0.0)
end

---@param activeLights table[]
function GameMap:_ensureDynamicTransmission(activeLights)
    local requiredSize = 1
    for _, entry in ipairs(activeLights) do
        local light = entry.light
        local width = math.ceil(light.position.x + light.radius) - math.floor(light.position.x - light.radius)
            + DYNAMIC_TRANSMISSION_PADDING * 2
        local height = math.ceil(light.position.y + light.radius) - math.floor(light.position.y - light.radius)
            + DYNAMIC_TRANSMISSION_PADDING * 2
        requiredSize = math.max(requiredSize, width, height)
    end
    if self._dynamicTransmission ~= nil and self._dynamicTransmissionPixelSize >= requiredSize then
        return
    end
    local size = sf.Vector2u.new(requiredSize, requiredSize)
    ---@cast size sf.Vector2u
    self._dynamicTransmission = sf.RenderTexture.new(size)
    self._dynamicTransmission:setSmooth(false)
    self._dynamicTransmissionPixelSize = requiredSize
end

function GameMap:_ensureDirectLight()
    ---@cast self._camera GlobalCore.Camera
    local renderTexture = self._camera:getRenderTexture()
    ---@cast renderTexture sf.RenderTexture
    local requiredSize = renderTexture:getSize()
    if self._directLight ~= nil and self._directLight:getSize() == requiredSize then
        return
    end
    self._directLight = sf.RenderTexture.new(requiredSize)
    self._directLight:setSmooth(false)
end

function GameMap:_ensureStaticDirectLight()
    if self._staticDirectLight ~= nil then
        return
    end
    local tilemapSize = self._tilemap:getSize()
    local requiredSize = sf.Vector2u.new(tilemapSize.x * Engine.CellSize, tilemapSize.y * Engine.CellSize)
    ---@cast requiredSize sf.Vector2u
    self._staticDirectLight = sf.RenderTexture.new(requiredSize)
    self._staticDirectLight:setSmooth(false)
    self._cachedLightMaterialRevision = -1
end

function GameMap:_setLightPassCommonUniforms()
    ---@cast self._camera GlobalCore.Camera
    local screenSize = self._camera:getViewSize()
    ---@cast screenSize sf.Vector2f
    self:_setLightPassTextureUniforms()
    self:_setViewShaderUniforms(self._lightPassShader, screenSize, self._zeroShaderOffset, true)
end

function GameMap:_setLightPassWorldUniforms()
    local tilemapSize = self._tilemap:getSize()
    local screenSize = sf.Vector2f.new(tilemapSize.x * Engine.CellSize, tilemapSize.y * Engine.CellSize)
    self:_setLightPassTextureUniforms()
    self._lightPassShader:setUniform("screenScale", 1.0)
    self._lightPassShader:setUniform("screenSize", screenSize)
    self._lightPassShader:setUniform("mapViewOffset", self._zeroShaderOffset)
    self._lightPassShader:setUniform("viewPos", self._zeroShaderOffset)
    self._lightPassShader:setUniform("viewRot", 0.0)
    self._lightPassShader:setUniform("gridSize", self._shaderMapSize)
    self._lightPassShader:setUniform("cellSize", Engine.CellSize)
end

function GameMap:_setLightPassTextureUniforms()
    ---@cast self._staticTransmission sf.RenderTexture
    self._lightPassShader:setUniform("staticTransmission", self._staticTransmission:getTexture())
    self._lightPassShader:setUniform("staticOccupancy", self._staticOccupancy)
end

---@param shader                  sf.Shader
---@param screenSize              sf.Vector2f
---@param mapViewOffset           sf.Vector2f
---@param usesFragmentCoordinates boolean
function GameMap:_setViewShaderUniforms(shader, screenSize, mapViewOffset, usesFragmentCoordinates)
    if usesFragmentCoordinates then
        shader:setUniform("screenScale", System.getScale())
        shader:setUniform("mapViewOffset", mapViewOffset)
    end
    shader:setUniform("screenSize", screenSize)
    ---@cast self._camera GlobalCore.Camera
    local viewPosition = self._camera:getViewPosition()
    ---@cast viewPosition sf.Vector2f
    shader:setUniform("viewPos", viewPosition)
    shader:setUniform("viewRot", self._camera:getViewRotation():asDegrees())
    shader:setUniform("gridSize", self._shaderMapSize)
    shader:setUniform("cellSize", Engine.CellSize)
end

---@param entry         table
---@param dynamicOrigin sf.Vector2f
---@param dynamicSize   sf.Vector2f
---@param traceStatic   boolean
---@param traceDynamic  boolean
---@param target        sf.RenderTexture
function GameMap:_renderLight(entry, dynamicOrigin, dynamicSize, traceStatic, traceDynamic, target)
    local light = entry.light
    if traceDynamic then
        ---@cast self._dynamicTransmission sf.RenderTexture
        self._lightPassShader:setUniform("dynamicTransmission", self._dynamicTransmission:getTexture())
    end
    self._lightPassShader:setUniform("dynamicMaskOrigin", dynamicOrigin)
    self._lightPassShader:setUniform("dynamicMaskSize", dynamicSize)
    self._lightPassShader:setUniform("traceStatic", traceStatic and 1.0 or 0.0)
    self._lightPassShader:setUniform("traceDynamic", traceDynamic and 1.0 or 0.0)
    self._lightPassShader:setUniform("lightPos", light.position)
    self._lightPassShader:setUniform("lightColor", self:_toShaderColour(light.colour, false))
    self._lightPassShader:setUniform("lightRadius", light.radius)
    self._lightPassShader:setUniform("lightIntensity", light.intensity)
    local diameter = light.radius * 2.0
    ---@cast self._lightPassQuad sf.RectangleShape
    ---@cast self._lightPassRenderStates sf.RenderStates
    self._lightPassQuad:setSize(sf.Vector2f.new(diameter, diameter))
    self._lightPassQuad:setPosition(sf.Vector2f.new(light.position.x - light.radius, light.position.y - light.radius))
    target:draw(self._lightPassQuad, self._lightPassRenderStates)
end

---@param light GlobalCore.Light
---@return boolean
function GameMap:_lightHasStaticTransmissionLoss(light)
    if not self._staticHasTransmissionLoss then
        return false
    end
    local size = self._tilemap:getSize()
    local cellSize = Engine.CellSize
    local minimumX = math.max(0, math.floor((light.position.x - light.radius) / cellSize) - 1)
    local minimumY = math.max(0, math.floor((light.position.y - light.radius) / cellSize) - 1)
    local maximumX = math.min(size.x - 1, math.floor((light.position.x + light.radius) / cellSize) + 1)
    local maximumY = math.min(size.y - 1, math.floor((light.position.y + light.radius) / cellSize) + 1)
    if minimumX > maximumX or minimumY > maximumY then
        return false
    end
    local prefix = self._staticOccupancyPrefix
    ---@cast prefix number[][]
    local maximumRow = prefix[maximumY + 2]
    ---@cast maximumRow number[]
    local minimumRow = prefix[minimumY + 1]
    ---@cast minimumRow number[]
    local maximumCorner = maximumRow[maximumX + 2]
    ---@cast maximumCorner number
    local maximumRowMinimum = maximumRow[minimumX + 1]
    ---@cast maximumRowMinimum number
    local minimumRowMaximum = minimumRow[maximumX + 2]
    ---@cast minimumRowMaximum number
    local minimumCorner = minimumRow[minimumX + 1]
    ---@cast minimumCorner number
    local total = maximumCorner - minimumRowMaximum - maximumRowMinimum + minimumCorner
    return total > 0
end

---@param entries table[]
---@param target  sf.RenderTexture
function GameMap:_renderStaticLights(entries, target)
    for _, entry in ipairs(entries) do
        self:_renderLight(entry, self._zeroShaderOffset, self._zeroShaderOffset, true, false, target)
    end
end

---@param entries table[]
---@param target  sf.RenderTexture
function GameMap:_renderUnobstructedLights(entries, target)
    if not bool(entries) then
        self._unobstructedLightCache = nil
        return
    end
    if not self:_lightsMatchCache(entries, self._unobstructedLightCache) then
        self:_cacheUnobstructedLights(entries)
    end
    ---@cast self._unobstructedLightVertices sf.VertexArray
    ---@cast self._unobstructedLightPassRenderStates sf.RenderStates
    target:draw(self._unobstructedLightVertices, self._unobstructedLightPassRenderStates)
end

---@param entries table[]
---@param cache   table[] | nil
---@return boolean
function GameMap._lightsMatchCache(_self, entries, cache)
    if cache == nil or #cache ~= #entries then
        return false
    end
    for index, entry in ipairs(entries) do
        local light = entry.light
        local cached = cache[index]
        ---@cast cached number[]
        if cached[1] ~= light.position.x or cached[2] ~= light.position.y or cached[3] ~= light.colour.r
            or cached[4] ~= light.colour.g or cached[5] ~= light.colour.b or cached[6] ~= light.radius
            or cached[7] ~= light.intensity then
            return false
        end
    end
    return true
end

---@param entries table[]
function GameMap:_cacheUnobstructedLights(entries)
    local cache = {}
    ---@cast self._unobstructedLightVertices sf.VertexArray
    ---@cast self._unobstructedLightVertex sf.Vertex
    self._unobstructedLightVertices:clear()
    for index, entry in ipairs(entries) do
        local light = entry.light
        cache[index] = self:_cacheLightValues(light)
        self._unobstructedLightPassShader:setUniform(UNOBSTRUCTED_LIGHT_INTENSITY_UNIFORMS[index], light.intensity)
        self:_appendLightBatch(self._unobstructedLightVertices, self._unobstructedLightVertex, light, index - 1)
    end
    self._unobstructedLightCache = cache
end

---@param light GlobalCore.Light
---@return table
function GameMap._cacheLightValues(_self, light)
    return {
        light.position.x, light.position.y, light.colour.r, light.colour.g, light.colour.b, light.radius,
        light.intensity
    }
end

---@param entries table[]
---@return table[]
function GameMap:_cacheLightList(entries)
    local cache = {}
    for index, entry in ipairs(entries) do
        cache[index] = self:_cacheLightValues(entry.light)
    end
    return cache
end

---@param vertices sf.VertexArray
---@param vertex   sf.Vertex
---@param light    GlobalCore.Light
---@param index    integer
function GameMap:_appendLightBatch(vertices, vertex, light, index)
    local radius = light.radius
    local left = light.position.x - radius
    local right = light.position.x + radius
    local top = light.position.y - radius
    local bottom = light.position.y + radius
    local colour = sf.Color.new(light.colour.r, light.colour.g, light.colour.b, index)
    self:_appendLightBatchVertex(vertices, vertex, left, top, -1.0, -1.0, colour)
    self:_appendLightBatchVertex(vertices, vertex, right, top, 1.0, -1.0, colour)
    self:_appendLightBatchVertex(vertices, vertex, right, bottom, 1.0, 1.0, colour)
    self:_appendLightBatchVertex(vertices, vertex, left, top, -1.0, -1.0, colour)
    self:_appendLightBatchVertex(vertices, vertex, right, bottom, 1.0, 1.0, colour)
    self:_appendLightBatchVertex(vertices, vertex, left, bottom, -1.0, 1.0, colour)
end

---@param vertices sf.VertexArray
---@param vertex   sf.Vertex
---@param x        number
---@param y        number
---@param textureX number
---@param textureY number
---@param colour   sf.Color
function GameMap._appendLightBatchVertex(_self, vertices, vertex, x, y, textureX, textureY, colour)
    vertex.position = sf.Vector2f.new(x, y)
    vertex.texCoords = sf.Vector2f.new(textureX, textureY)
    vertex.color = colour
    vertices:append(vertex)
end

---@param light         GlobalCore.Light
---@param owner         Engine.Actor | nil
---@param visibleActors table<Engine.Actor, boolean>
---@return sf.Vector2f, sf.Vector2f, boolean
function GameMap:_renderDynamicTransmission(light, owner, visibleActors)
    local cellRadius = math.ceil((light.radius + Engine.CellSize) / Engine.CellSize)
    local mapX = math.floor(light.position.x / Engine.CellSize)
    local mapY = math.floor(light.position.y / Engine.CellSize)
    local actors
    if owner == nil then
        actors = self:getActorsInRange(mapX, mapY, cellRadius)
    else
        actors = self:getActorsInRangeExcluding(mapX, mapY, cellRadius, owner)
    end
    ---@cast actors Engine.Actor[]
    local occluders = {}
    for _, actor in ipairs(actors) do
        if visibleActors[actor] == true and not actor:isDestroyed()
            and actor:getLightBlock() > 0.0 and self:_actorIntersectsLight(actor, light) then
            occluders[#occluders + 1] = actor
        end
    end
    if not bool(occluders) then
        return self._zeroShaderOffset, self._zeroShaderOffset, false
    end

    local minimumX = nil
    local minimumY = nil
    local maximumX = nil
    local maximumY = nil
    for _, actor in ipairs(occluders) do
        local bounds = actor:getGlobalBounds()
        if minimumX == nil or bounds.position.x < minimumX then
            minimumX = bounds.position.x
        end
        if minimumY == nil or bounds.position.y < minimumY then
            minimumY = bounds.position.y
        end
        local right = bounds.position.x + bounds.size.x
        if maximumX == nil or right > maximumX then
            maximumX = right
        end
        local bottom = bounds.position.y + bounds.size.y
        if maximumY == nil or bottom > maximumY then
            maximumY = bottom
        end
    end
    ---@cast minimumX number
    ---@cast minimumY number
    ---@cast maximumX number
    ---@cast maximumY number
    minimumX = math.max(
        math.floor(light.position.x - light.radius) - DYNAMIC_TRANSMISSION_PADDING,
        math.floor(minimumX) - DYNAMIC_TRANSMISSION_PADDING
    )
    minimumY = math.max(
        math.floor(light.position.y - light.radius) - DYNAMIC_TRANSMISSION_PADDING,
        math.floor(minimumY) - DYNAMIC_TRANSMISSION_PADDING
    )
    maximumX = math.min(
        math.ceil(light.position.x + light.radius) + DYNAMIC_TRANSMISSION_PADDING,
        math.ceil(maximumX) + DYNAMIC_TRANSMISSION_PADDING
    )
    maximumY = math.min(
        math.ceil(light.position.y + light.radius) + DYNAMIC_TRANSMISSION_PADDING,
        math.ceil(maximumY) + DYNAMIC_TRANSMISSION_PADDING
    )
    local origin = sf.Vector2f.new(minimumX, minimumY)
    local size = sf.Vector2f.new(math.max(1.0, maximumX - minimumX), math.max(1.0, maximumY - minimumY))
    local centre = sf.Vector2f.new(origin.x + size.x * 0.5, origin.y + size.y * 0.5)
    ---@cast self._dynamicTransmission sf.RenderTexture
    ---@cast self._transmissionActorRenderStates sf.RenderStates
    self._dynamicTransmission:setView(sf.View.new(centre, size))
    self._dynamicTransmission:clear(sf.Color.White)
    self._lightMaskShader:setUniform("transmissionMode", 1.0)
    for _, actor in ipairs(occluders) do
        self:_setActorMaskUniforms(actor)
        self._dynamicTransmission:draw(actor, self._transmissionActorRenderStates)
    end
    self._dynamicTransmission:display()
    return origin, size, true
end

---@param actor Engine.Actor
---@param light GlobalCore.Light
---@return boolean
function GameMap._actorIntersectsLight(_self, actor, light)
    local bounds = actor:getGlobalBounds()
    return bounds.position.x <= light.position.x + light.radius and bounds.position.x + bounds.size.x
            >= light.position.x - light.radius and bounds.position.y <= light.position.y + light.radius
        and bounds.position.y + bounds.size.y >= light.position.y - light.radius
end

---@return table[]
function GameMap:_getActiveLights()
    local lights = {}
    local viewport = self._camera ~= nil and self._camera:getViewport() or nil
    for _, light in ipairs(self._lights) do
        if light.radius > 0.0 and self:_isLightVisible(light.position, light.radius, viewport) then
            lights[#lights + 1] = { light = light }
            if #lights >= MAX_SHADER_LIGHTS then
                return lights
            end
        end
    end
    ---@type sf.Vector2f | nil
    local position = nil
    for _, actor in ipairs(self:getAllActors()) do
        local lightComp = actor.lightComp
        if lightComp ~= nil and not actor:isDestroyed() then
            local radius = tonumber(lightComp.lightRadius) or 0.0
            if radius > 0.0 then
                if position == nil then
                    position = Pool.Get("sf.Vector2f", sf.Vector2f, {
                        x = 0.0,
                        y = 0.0
                    })
                end
                ---@cast position sf.Vector2f
                self:_getActorLightPosition(actor, lightComp, position)
                if self:_isLightVisible(position, radius, viewport) then
                    lights[#lights + 1] = {
                        light = Light.new(position, lightComp.lightColour, radius, 1.0),
                        owner = actor
                    }
                    if #lights >= MAX_SHADER_LIGHTS then
                        break
                    end
                end
            end
        end
    end
    if position ~= nil then
        Pool.Put("sf.Vector2f", position)
    end
    return lights
end

---@param actor     Engine.Actor
---@param lightComp Engine.LightComponent
---@param result    sf.Vector2f | nil
---@return sf.Vector2f
function GameMap._getActorLightPosition(_self, actor, lightComp, result)
    local bounds = actor:getLocalBounds()
    local offset = lightComp.lightOffset
    local localPosition = sf.Vector2f.new(
        bounds.position.x + bounds.size.x * 0.5 + offset.x, bounds.position.y + bounds.size.y * 0.5 + offset.y
    )
    local worldPosition = actor:getTransform():transformPoint(localPosition)
    result = result or sf.Vector2f.new()
    result.x = worldPosition.x
    result.y = worldPosition.y
    return result
end

---@param position sf.Vector2f
---@param radius   number
---@param viewport sf.FloatRect | nil
---@return boolean
function GameMap:_isLightVisible(position, radius, viewport)
    if viewport == nil then
        return true
    end
    ---@cast self._camera GlobalCore.Camera
    local radians = self._camera:getViewRotation():asDegrees() * 0.017453292519943295
    local cosine = math.abs(math.cos(radians))
    local sine = math.abs(math.sin(radians))
    local halfWidth = viewport.size.x * 0.5
    local halfHeight = viewport.size.y * 0.5
    local extentX = cosine * halfWidth + sine * halfHeight
    local extentY = sine * halfWidth + cosine * halfHeight
    local centreX = viewport.position.x + halfWidth
    local centreY = viewport.position.y + halfHeight
    return position.x + radius >= centreX - extentX and position.x - radius <= centreX + extentX
        and position.y + radius >= centreY - extentY and position.y - radius <= centreY + extentY
end

---@param colour     sf.Color
---@param applyAlpha boolean
---@return sf.Vector3f
function GameMap:_toShaderColour(colour, applyAlpha)
    local alpha = applyAlpha and colour.a / 255.0 or 1.0
    self._shaderColour.x = colour.r / 255.0 * alpha
    self._shaderColour.y = colour.g / 255.0 * alpha
    self._shaderColour.z = colour.b / 255.0 * alpha
    return self._shaderColour
end

function GameMap:markPassabilityDirty()
    self._materialDirty = true
    self._materialRevision = self._materialRevision + 1
    self:invalidatePassabilityCache()
end

function GameMap:updateActorOccupancy(actor)
    if self._tilePassableGrid == nil or self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
        return
    end
    actor:syncMapCache()
    super(GameMap, self).updateActorOccupancy(actor)
end

---@param tileID integer | string | nil
---@return integer | string | nil
function GameMap._normaliseTerrainTileID(_self, tileID)
    if tileID == nil then
        return nil
    elseif type(tileID) == "string" then
        return bool(tileID) and tileID or nil
    end
    assert(
        type(tileID) == "number" and math.type(tileID) == "integer",
        "terrain tile ID must be an integer, string, or nil"
    )
    return tileID
end

---@param layer    Engine.TileLayer
---@param position sf.Vector2i
---@return boolean
function GameMap._isTerrainPositionInLayer(_self, layer, position)
    local size = layer:getGridSize()
    return position.x >= 0 and position.y >= 0 and position.x < size.x and position.y < size.y
end

---@param layer    Engine.TileLayer
---@param position sf.Vector2i
---@return integer | string | nil
function GameMap:_getTerrainTileID(layer, position)
    return self:_getTerrainAutoTileID(layer, position) or layer:get(position)
end

---@param layer               Engine.TileLayer
---@param layerData           Engine.TileLayerData
---@param autoTileTextures    sf.Texture[]
---@param autoTileFrameCounts integer[]
---@param position            sf.Vector2i
---@param tileID              integer | string | nil
function GameMap:_writeTerrainTile(layer, layerData, autoTileTextures, autoTileFrameCounts, position, tileID)
    local x = position.x + 1
    local y = position.y + 1
    self:_ensureTerrainAutoTileGrid(layerData, layer:getGridSize())
    local tiles = layerData.tiles
    local autoTiles = layerData.autoTiles
    local tilesRow = tiles[y]
    ---@cast tilesRow(integer | nil)[]
    local autoTilesRow = autoTiles[y]
    ---@cast autoTilesRow(integer | string | nil)[]
    if tileID == nil then
        tilesRow[x] = nil
        autoTilesRow[x] = nil
    elseif type(tileID) == "string" then
        tilesRow[x] = nil
        autoTilesRow[x] = self:_resolveAutoTileIndex(layerData, autoTileTextures, autoTileFrameCounts, tileID)
    else
        if tileID < 0 or tileID >= #layerData.layerTileset.materials then
            error("Tile ID " .. tileID .. " is out of range for layer '" .. layer:getName() .. "'", 2)
        end
        tilesRow[x] = tileID
        autoTilesRow[x] = nil
    end
    layerData.tiles = tiles
    layerData.autoTiles = autoTiles
end

---@param layer    Engine.TileLayer
---@param position sf.Vector2i
---@return string | nil
function GameMap._getTerrainAutoTileID(_self, layer, position)
    local autoTiles = layer:getAutoTiles()
    local row = autoTiles and autoTiles[position.y + 1] or nil
    if row == nil then
        return nil
    end
    local autoTileIndex = row[position.x + 1]
    if autoTileIndex == nil then
        return nil
    elseif type(autoTileIndex) == "string" then
        return bool(autoTileIndex) and autoTileIndex or nil
    end
    local autoTileKey = layer:getAutoTileKey(autoTileIndex)
    if autoTileKey ~= nil then
        return autoTileKey
    end
    local autoTilePool = layer:getAutoTilePool()
    if autoTileIndex >= 0 and autoTileIndex < #autoTilePool then
        local autoTile = autoTilePool[autoTileIndex + 1]
        ---@cast autoTile Engine.AutoTile
        return autoTile.name
    end
    return nil
end

---@param layerData Engine.TileLayerData
---@param size      sf.Vector2u
function GameMap._ensureTerrainAutoTileGrid(_self, layerData, size)
    local autoTiles = layerData.autoTiles
    if not bool(autoTiles) then
        autoTiles = {}
    end
    while #autoTiles < size.y do
        autoTiles[#autoTiles + 1] = {}
    end
    for _, row in ipairs(autoTiles) do
        ---@cast row(integer | string | nil)[] & { n: integer | nil }
        local rowLength = row.n
        if rowLength == nil then
            row.n = size.x
        else
            row.n = math.max(rowLength, size.x)
        end
    end
    layerData.autoTiles = autoTiles
end

---@param layerData           Engine.TileLayerData
---@param autoTileTextures    sf.Texture[]
---@param autoTileFrameCounts integer[]
---@param autoTileName        string
---@return integer
function GameMap:_resolveAutoTileIndex(layerData, autoTileTextures, autoTileFrameCounts, autoTileName)
    local autoTileKeys = layerData.autoTileKeys or {}
    local autoTilePool = layerData.autoTilePool
    if not bool(autoTileKeys) then
        for _, entry in ipairs(autoTilePool) do
            autoTileKeys[#autoTileKeys + 1] = entry.name
        end
        layerData.autoTileKeys = autoTileKeys
    end
    for index, name in ipairs(autoTileKeys) do
        if name == autoTileName then
            self:_ensureAutoTileRuntimeData(layerData, autoTileTextures, autoTileFrameCounts)
            return index - 1
        end
    end
    if self._autoTileResolver == nil then
        error("AutoTile resolver is not configured", 2)
    end
    local autoTile = self._autoTileResolver(autoTileName)
    if autoTile == nil then
        error("Autotile '" .. autoTileName .. "' not found", 2)
    end
    autoTilePool[#autoTilePool + 1] = autoTile
    autoTileKeys[#autoTileKeys + 1] = autoTileName
    layerData.autoTilePool = autoTilePool
    layerData.autoTileKeys = autoTileKeys
    self:_ensureAutoTileRuntimeData(layerData, autoTileTextures, autoTileFrameCounts)
    return #autoTilePool - 1
end

---@param layerData           Engine.TileLayerData
---@param autoTileTextures    sf.Texture[]
---@param autoTileFrameCounts integer[]
function GameMap:_ensureAutoTileRuntimeData(layerData, autoTileTextures, autoTileFrameCounts)
    while #autoTileTextures < #layerData.autoTilePool do
        local autoTile = layerData.autoTilePool[#autoTileTextures + 1]
        ---@cast autoTile Engine.AutoTile
        autoTileTextures[#autoTileTextures + 1] = ManagerFunctions.loadAutotile(autoTile.fileName)
    end
    while #autoTileFrameCounts < #autoTileTextures do
        local texture = autoTileTextures[#autoTileFrameCounts + 1]
        autoTileFrameCounts[#autoTileFrameCounts + 1] = self:_getAutoTileFrameCount(texture)
    end
end

---@param texture sf.Texture | nil
---@return integer
function GameMap._getAutoTileFrameCount(_self, texture)
    if texture == nil then
        return 1
    end
    local size = texture:getSize()
    local frames = Engine.CellSize > 0 and math.floor(size.x / (3 * Engine.CellSize)) or 1
    return math.max(frames, 1)
end

---@param _layerName          string
---@param layer               Engine.TileLayer
---@param layerData           Engine.TileLayerData
---@param autoTileTextures    sf.Texture[]
---@param autoTileFrameCounts integer[]
function GameMap:_replaceTerrainLayer(_layerName, layer, layerData, autoTileTextures, autoTileFrameCounts)
    self:_resetTransparentTiles()
    local newLayer = layer:rebuild(layerData, autoTileTextures, autoTileFrameCounts)
    self._tilemap:addLayer(newLayer)
    self._layersTopFirst = {}
    local layerKeys = self._layerNames
    for index = #layerKeys, 1, -1 do
        self._layersTopFirst[#self._layersTopFirst + 1] = self._tilemap:getLayer(layerKeys[index])
    end
end

---@param layerKeys        string[]
---@param playerLayerIndex integer
---@return boolean, sf.Vector2i | nil
function GameMap:_preparePlayerCover(layerKeys, playerLayerIndex)
    if self._player == nil or playerLayerIndex == -1 then
        self:_resetTransparentTiles()
        return false, nil
    end
    local playerPosition = self._player:getMapPosition()
    local coverLayerStates = self._coverLayerStates
    local reusable = self._coverPlayerX == playerPosition.x and self._coverPlayerY == playerPosition.y
        and self._coverPlayerLayerIndex == playerLayerIndex and self._coverAlpha == GameMap.DefaultCoverAlpha
        and self._coverMaterialRevision == self._materialRevision and coverLayerStates ~= nil
        and #coverLayerStates == #layerKeys
    if reusable then
        ---@cast coverLayerStates GameMapCoverLayerState[]
        for index, layerName in ipairs(layerKeys) do
            local layer = self._tilemap:getLayer(layerName)
            ---@cast layer Engine.TileLayer
            local state = coverLayerStates[index]
            ---@cast state GameMapCoverLayerState
            if state.layer ~= layer or state.visible ~= layer.visible then
                reusable = false
                break
            end
        end
    end
    if reusable then
        return false, playerPosition
    end
    self:_resetTransparentTiles()
    self._coverPlayerX = playerPosition.x
    self._coverPlayerY = playerPosition.y
    self._coverPlayerLayerIndex = playerLayerIndex
    self._coverAlpha = GameMap.DefaultCoverAlpha
    self._coverMaterialRevision = self._materialRevision
    self._coverLayerStates = {}
    for index, layerName in ipairs(layerKeys) do
        local layer = self._tilemap:getLayer(layerName)
        ---@cast layer Engine.TileLayer
        self._coverLayerStates[index] = { layer = layer, visible = layer.visible }
    end
    return true, playerPosition
end

function GameMap:_resetTransparentTiles()
    for _, item in ipairs(self._transparentTiles) do
        if item[1].resetTileColor ~= nil then
            item[1]:resetTileColor(item[2], item[3])
        end
    end
    self._transparentTiles = {}
    self._coverLayerStates = nil
    self._coverPlayerX = nil
    self._coverPlayerY = nil
    self._coverPlayerLayerIndex = nil
    self._coverAlpha = nil
    self._coverMaterialRevision = nil
end

---@param layerKeys string[]
---@return integer
function GameMap:_getPlayerLayerIndex(layerKeys)
    if self._player == nil then
        return -1
    end
    for index, name in ipairs(layerKeys) do
        for _, actor in ipairs(self._actors[name] or {}) do
            if actor == self._player then
                return index - 1
            end
        end
    end
    return -1
end

---@param layer            Engine.TileLayer
---@param layerIndex       integer
---@param playerLayerIndex integer
---@param playerPosition   sf.Vector2i
function GameMap:_applyPlayerCover(layer, layerIndex, playerLayerIndex, playerPosition)
    if self._player == nil or layerIndex <= playerLayerIndex or playerLayerIndex == -1 then
        return
    end
    if layer:get(playerPosition) == nil then
        return
    end
    if layer.floodFillTransparent ~= nil then
        for _, position in ipairs(
            layer:floodFillTransparent(playerPosition.x, playerPosition.y, self._playerCoverColour)
        ) do
            self._transparentTiles[#self._transparentTiles + 1] = { layer, position.x, position.y }
        end
    elseif layer.setTileColor ~= nil then
        layer:setTileColor(playerPosition.x, playerPosition.y, self._playerCoverColour)
        self._transparentTiles[#self._transparentTiles + 1] = { layer, playerPosition.x, playerPosition.y }
    end
end

---@param target           sf.RenderTarget
---@param states           Engine.RenderStates
---@param layerName        string
---@param layerIndex       integer
---@param playerLayerIndex integer
---@param applyPlayerCover boolean
function GameMap:_drawLayerActors(target, states, layerName, layerIndex, playerLayerIndex, applyPlayerCover)
    for _, actor in ipairs(self._actors[layerName] or {}) do
        if self._actorPixelShatterByActor[actor] == nil then
            local actorAlpha = 255
            if applyPlayerCover and self._player ~= nil and layerIndex > playerLayerIndex and playerLayerIndex ~= -1
                and actor ~= self._player and actor:intersects(self._player) then
                actorAlpha = GameMap.DefaultCoverAlpha
            end
            ---@cast actorAlpha integer
            self:_drawActor(target, states, actor, actorAlpha)
        end
    end
    self:_drawActorPixelShatterEffects(target, layerName)
end

function GameMap:_prepareActorPixelShatterEffects()
    local function drawActor(snapshotTarget, actor)
        self:_drawActor(snapshotTarget, sf.RenderStates.new(), actor, 255)
    end
    for _, effects in pairs(self._actorPixelShatterEffects) do
        for _, effect in ipairs(effects) do
            if not effect:isPrepared() then
                effect:prepare(drawActor)
            end
        end
    end
end

---@param target    sf.RenderTarget
---@param layerName string
function GameMap:_drawActorPixelShatterEffects(target, layerName)
    local effects = self._actorPixelShatterEffects[layerName]
    if not bool(effects) then
        return
    end
    ---@cast effects Global.CustomEffects.ActorPixelShatterEffect[]
    for _, effect in ipairs(effects) do
        if not effect:isFinished() then
            effect:draw(target)
        end
    end
end

---@param target     sf.RenderTarget
---@param states     Engine.RenderStates
---@param actor      Engine.Actor
---@param actorAlpha integer
function GameMap:_drawActor(target, states, actor, actorAlpha)
    local hue = Render.NormaliseActorHue(actor.hue or 0.0)
    local hasHue = self._actorHueShader ~= nil and not Render.IsNeutralActorHue(hue)
    local hasShaderError = actor:hasShaderError()
    local actorColour = Pool.Get("sf.Color", sf.Color, {
        r = 255,
        g = hasShaderError and 0 or 255,
        b = 255,
        a = actorAlpha
    })
    if hasShaderError then
        actor:setColor(actorColour)
        Pool.Put("sf.Color", actorColour)
        target:draw(actor, states)
        return
    end
    actor:setColor(actorColour)
    Pool.Put("sf.Color", actorColour)
    local actorShader = actor:getShader()
    if actorShader ~= nil then
        local texture = actor:getTexture()
        ---@cast texture sf.Texture
        Render.BindActorShader(actorShader, texture, actor:getTextureRect(), self._shaderTime)
        if hasHue and self:_drawActorShaderWithHue(target, actor, actorShader, hue, actorAlpha) then
            return
        end
        local renderStates = sf.RenderStates.new()
        renderStates.shader = actorShader
        target:draw(actor, renderStates)
        return
    end
    if hasHue then
        self:_applyActorHueUniform(hue)
        local renderStates = sf.RenderStates.new(states.blendMode)
        renderStates.transform = states.transform
        renderStates.texture = states.texture
        renderStates.shader = self._actorHueShader
        target:draw(actor, renderStates)
        return
    end
    target:draw(actor, states)
end

---@param target      sf.RenderTarget
---@param actor       Engine.Actor
---@param actorShader sf.Shader
---@param hue         number
---@param actorAlpha  integer
---@return boolean
function GameMap:_drawActorShaderWithHue(target, actor, actorShader, hue, actorAlpha)
    if self._actorHueShader == nil then
        return false
    end
    local texture = actor:getTexture()
    ---@cast texture sf.Texture
    local rect = actor:getTextureRect()
    local size = Pool.Get("sf.Vector2u", sf.Vector2u, {
        x = math.max(1, math.floor(rect.size.x)),
        y = math.max(1, math.floor(rect.size.y))
    })
    local shaderBuffer = self:_ensureActorShaderBuffer(size)
    local hueBuffer = self:_ensureActorHueBuffer(size)
    Pool.Put("sf.Vector2u", size)
    local localSprite = sf.Sprite.new(texture, rect)
    local actorColour = Pool.Get("sf.Color", sf.Color, {
        r = 255,
        g = 255,
        b = 255,
        a = actorAlpha
    })
    localSprite:setColor(actorColour)
    Pool.Put("sf.Color", actorColour)
    local shaderStates = sf.RenderStates.new()
    shaderStates.shader = actorShader
    shaderBuffer:clear(sf.Color.Transparent)
    shaderBuffer:draw(localSprite, shaderStates)
    shaderBuffer:display()
    self:_applyActorHueUniform(hue)
    local hueStates = sf.RenderStates.new()
    hueStates.shader = self._actorHueShader
    local sourceSprite = self:_ensureActorHueSourceSprite(shaderBuffer:getTexture())
    sourceSprite:setTexture(shaderBuffer:getTexture(), true)
    sourceSprite:setColor(sf.Color.White)
    hueBuffer:clear(sf.Color.Transparent)
    hueBuffer:draw(sourceSprite, hueStates)
    hueBuffer:display()
    local resultSprite = sf.Sprite.new(hueBuffer:getTexture())
    local renderStates = sf.RenderStates.new()
    renderStates.transform = renderStates.transform:combine(actor:getTransform())
    target:draw(resultSprite, renderStates)
    return true
end

---@param size sf.Vector2u
---@return sf.RenderTexture
function GameMap:_ensureActorShaderBuffer(size)
    if self._actorShaderBuffer == nil or self._actorShaderBuffer:getSize() ~= size then
        self._actorShaderBuffer = sf.RenderTexture.new(size)
    end
    return self._actorShaderBuffer
end

---@param size sf.Vector2u
---@return sf.RenderTexture
function GameMap:_ensureActorHueBuffer(size)
    if self._actorHueBuffer == nil or self._actorHueBuffer:getSize() ~= size then
        self._actorHueBuffer = sf.RenderTexture.new(size)
    end
    return self._actorHueBuffer
end

---@param texture sf.Texture
---@return sf.Sprite
function GameMap:_ensureActorHueSourceSprite(texture)
    if self._actorHueSourceSprite == nil then
        self._actorHueSourceSprite = sf.Sprite.new(texture)
    end
    return self._actorHueSourceSprite
end

---@param hue number
function GameMap:_applyActorHueUniform(hue)
    if self._actorHueShader ~= nil then
        self._actorHueShader:setUniform("screenTex", sf.Shader.CurrentTexture)
        self._actorHueShader:setUniform("hue", hue)
    end
end

---@param functionName string
---@param invalidValue number | boolean
---@param smooth       boolean
---@return sf.Texture
function GameMap:_getMaterialPropertyTexture(functionName, invalidValue, smooth)
    return self:generateDataFromMap(
        self._tilemap:getSize(), self:getMaterialPropertyMap(functionName, invalidValue), smooth == true
    )
end

function GameMap:_rebuildPassabilityCache()
    local size = self._tilemap:getSize()
    self:syncActorsRef(self._wholeActorList)
    self:_syncActorsForMapCache()
    self._tilePassableGrid = self:rebuildPassabilityCache(size)
end

return class(GameMap, GameMapBase)
