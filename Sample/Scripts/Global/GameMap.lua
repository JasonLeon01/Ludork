local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local DamageTextParticle = require("Global.CustomParticles.DamageTextParticle")
local GameMapActors = require("Global.GameMap.Actors")
local GameMapTerrain = require("Global.GameMap.Terrain")
local GameMapLighting = require("Global.GameMap.Lighting")
local GameMapPresentation = require("Global.GameMap.Presentation")
local GameMapRendering = require("Global.GameMap.Rendering")

local ShaderManager = GlobalCore.ShaderManager
local System = GlobalCore.System
local WeatherController = GlobalCore.WeatherController
local FogController = GlobalCore.FogController

local Actor = Engine.Actor
local Camera = GlobalCore.Camera
local GameMapBase = GlobalCore.GameMapBase
local GameMapRenderer = GlobalCore.GameMapRenderer

---@class (partial) GameMap
local GameMap = {}

---@alias GameMapImplState GameMap

local defaultMapViewRect = sf.IntRect.new(192, 32, 416, 416)
---@cast defaultMapViewRect sf.IntRect
GameMap.MapViewRect = defaultMapViewRect

---@param gameMap GameMap
---@return sf.IntRect
local function validateMapViewRect(gameMap)
    local rect = copy(gameMap.MapViewRect)
    ---@cast rect sf.IntRect
    local gameSize = System.getGameSize()
    assert(rect.position.x >= 0 and rect.position.y >= 0, "MapViewRect position must not be negative")
    assert(rect.size.x > 0 and rect.size.y > 0, "MapViewRect size must be positive")
    assert(
        rect.position.x + rect.size.x <= gameSize.x and rect.position.y + rect.size.y <= gameSize.y,
        "MapViewRect must fit inside the logical game size"
    )
    return rect
end

---@param gameMap GameMap
---@param camera  GlobalCore.Camera
local function configureCamera(gameMap, camera)
    local rect = gameMap:getMapViewRect()
    camera:setMap(gameMap)
    camera:setViewSize(sf.Vector2f.new(rect.size.x, rect.size.y))
    camera:fixViewPosition()
end

function GameMap:init(mapName, tilemap, camera, previewOnly, sparseWorldConfig)
    previewOnly = bool(previewOnly)
    local actorPixelShatterShader = nil
    if sf.Shader.isAvailable() and not previewOnly then
        actorPixelShatterShader = assert(
            ShaderManager.loadFull("Global/ActorPixelShatter.vert", "Global/ActorPixelShatter.frag"),
            "Actor pixel shatter shader must not be nil"
        )
    end
    GameMapBase.init(self)
    if sparseWorldConfig ~= nil then
        self:configureSparseWorld(sparseWorldConfig.size, sparseWorldConfig.layerOrder, sparseWorldConfig.regionRects)
    end
    self._actorPixelShatterShader = actorPixelShatterShader
    self._previewOnly = previewOnly
    self.mapName = mapName
    self._scene = nil
    self._tilemap = tilemap
    self._mapViewRect = validateMapViewRect(self)
    self._layersTopFirst = {}
    local layerNames = sparseWorldConfig ~= nil and copy(sparseWorldConfig.layerOrder)
        or self._tilemap:getLayerNameList()
    self._layerNames = layerNames
    for index = #layerNames, 1, -1 do
        self._layersTopFirst[#self._layersTopFirst + 1] = self._tilemap:getLayer(layerNames[index])
    end
    self._actors = {}
    self._particleSystem = nil
    if not self._previewOnly then
        self._particleSystem = Engine.ParticleSystem.new()
    end
    self._actorsOnDestroy = {}
    self._actorPixelShatterEffects = {}
    self._actorPixelShatterByActor = setmetatable({}, {
        __mode = "k"
    })
    self._actorPixelShatterSeed = 0
    self._camera = nil
    if not self._previewOnly then
        self._camera = camera or Camera.new()
    end
    self._lights = {}
    self._ambientLight = sf.Color.new(255, 255, 255, 255)
    self._materialDirty = true
    self._materialRevision = 0
    self._shaderTime = 0.0
    self._tilePassableGrid = nil
    self._player = nil
    self._components = {}
    self._autoTileResolver = nil
    self._damageTextSpeedCurve = nil
    self._damageTextConfig = nil
    self._renderer = nil
    self:setTilemap(self._tilemap)
    if self._camera ~= nil then
        configureCamera(self, self._camera)
    end
    if sparseWorldConfig ~= nil then
        ---@diagnostic disable-next-line: undefined-field
        self:_initialiseWorldRendering()
    else
        self._renderer = GameMapRenderer.new(
            self, self._tilemap, self._camera, self._layerNames, self.DefaultCoverAlpha, self._previewOnly
        )
    end
    ---@type GameMap[]
    local selfRef = setmetatable({ self }, {
        __mode = "v"
    })
    self._renderEffectTarget = nil
    self._drawLayerEffects = function (layerName)
        local gameMap = selfRef[1]
        if gameMap ~= nil and gameMap._renderEffectTarget ~= nil then
            gameMap:_drawActorPixelShatterEffects(gameMap._renderEffectTarget, layerName)
        end
    end
    self:setActorListUpdater(function ()
        local gameMap = selfRef[1]
        if gameMap ~= nil then
            gameMap:updateActorList()
        end
    end)
    self:setActorDestroyer(function (actor)
        local gameMap = selfRef[1]
        if gameMap ~= nil then
            gameMap:destroyActor(actor)
        end
    end)
    self:updateActorList()
end

---@diagnostic disable-next-line: unused
function GameMap:isWorldMap()
    return false
end

function GameMap:updateAutoTileAnimation(deltaTime)
    self._tilemap:updateAutoTileAnimation(deltaTime)
end

---@diagnostic disable-next-line: unused
function GameMap:disposeStreaming()
end

---@diagnostic disable-next-line: unused
function GameMap:drawMapFogOverlay()
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

function GameMap:setDamageTextConfig(config)
    assert(config ~= nil, "DamageText config must not be nil")
    self._damageTextConfig = config
end

function GameMap:getCamera()
    return self._camera
end

function GameMap:setCamera(camera)
    self._camera = camera
    configureCamera(self, camera)
    if self._renderer ~= nil then
        self._renderer:setCamera(camera)
    end
end

function GameMap:getTilemap()
    return self._tilemap
end

function GameMap:getSize()
    return GameMapBase.getSize(self)
end

---@return Global.WorldGeometry.CellRect
function GameMap:_getGameplayCellRect()
    local size = self:getSize()
    return { x = 0, y = 0, width = size.x, height = size.y }
end

function GameMap:getMapViewRect()
    return self._mapViewRect
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

function GameMap:addDamageText(text, position)
    assert(self._damageTextSpeedCurve ~= nil, "DamageText speed curve is not configured")
    assert(self._damageTextConfig ~= nil, "DamageText config is not configured")
    local drawPosition = self:worldToMapViewPosition(position)
    DamageTextParticle.new(self._particleSystem, text, drawPosition, self._damageTextConfig, self._damageTextSpeedCurve)
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
        self:_withDeferredActorViewSync(function ()
            for _, actor in ipairs(self._actorsOnDestroy) do
                for _, actorList in pairs(self._actors) do
                    local index = table.index(actorList, actor)
                    if index ~= nil then
                        table.remove(actorList, index)
                        self._actorPixelShatterByActor[actor] = nil
                        self:_setActorEffectHidden(actor, false)
                        Actor.BlueprintEvent(actor, Actor, "onDestroy")
                    end
                end
            end
        end)
        self._actorsOnDestroy = {}
    end
    self:_updateActorPixelShatterEffects(deltaTime)
    self:_updateActors(deltaTime)
    self:_updateAudioListener()
    ---@cast self._particleSystem Engine.ParticleSystem
    self._particleSystem:onTick(deltaTime)
end

function GameMap:onLateTick(deltaTime)
    if self._camera ~= nil then
        self._camera:onLateTick(deltaTime)
    end
    for _, component in ipairs(self._components) do
        component:onLateTick(deltaTime)
    end
    self:_lateUpdateActors(deltaTime)
    ---@cast self._particleSystem Engine.ParticleSystem
    self._particleSystem:onLateTick(deltaTime)
end

function GameMap:onFixedTick(fixedDelta)
    for _, component in ipairs(self._components) do
        component:onFixedTick(fixedDelta)
    end
    self:_fixedUpdateActors(fixedDelta)
    if self._camera ~= nil then
        self._camera:onFixedTick(fixedDelta)
    end
end

function GameMap:drawMapContent(target, states, applyPlayerCover)
    self:_prepareActorPixelShatterEffects()
    assert(self._renderer ~= nil, "GameMap renderer is unavailable")
    self._renderEffectTarget = target
    self._renderer:drawContent(
        target, states or sf.RenderStates.new(), bool(applyPlayerCover), self._shaderTime, self._materialRevision,
        self._drawLayerEffects
    )
    self._renderEffectTarget = nil
end

---@return boolean
function GameMap:_prepareCameraFrame()
    if self._camera ~= nil then
        self._camera:syncFollowTarget()
    end
    return true
end

function GameMap:show()
    local renderCameraFrame = self:_prepareCameraFrame()
    System.setWindowMapView(self._mapViewRect)
    if renderCameraFrame and self._camera ~= nil then
        self._camera:clear()
    end
    if renderCameraFrame and self._camera ~= nil then
        self:drawMapContent(assert(self._camera:getRenderTexture()), self._camera:getRenderStates(), true)
    elseif renderCameraFrame then
        self:_resetTransparentTiles()
    end
    ---@cast self._camera GlobalCore.Camera
    if renderCameraFrame then
        for _, component in ipairs(self._components) do
            component:onRender(self._camera)
        end
        if self:_lightingShadersAvailable() then
            self:_renderLighting(self:_getActiveLights())
        end
        self:drawMapFogOverlay()
        if self._camera ~= nil then
            self._camera:display()
        end
        self:refreshShader()
        if self._camera ~= nil then
            WeatherController.drawShaderOverlay(self._camera)
        end
    end
    ---@diagnostic disable-next-line: param-type-mismatch
    System.draw(self._camera, self:_getMaterialShader())
    FogController.drawOverlay()
    ---@cast self._particleSystem Engine.ParticleSystem
    System.draw(self._particleSystem)
    System.setWindowDefaultView()
end

function GameMap:_syncActorsForMapCache()
    return GameMapActors._syncActorsForMapCache(self)
end

function GameMap:_syncActorsForPathfinding()
    return GameMapActors._syncActorsForPathfinding(self)
end

function GameMap:_checkDir4Between(fromPosition, toPosition, direction)
    return GameMapActors._checkDir4Between(self, fromPosition, toPosition, direction)
end

function GameMap:getAllActors()
    return GameMapActors.getAllActors(self)
end

function GameMap:getActorLayer(actor)
    return GameMapActors.getActorLayer(self, actor)
end

function GameMap:getActorsByPosition(position)
    return GameMapActors.getActorsByPosition(self, position)
end

function GameMap:getActorByLayerAndPosition(layer, position)
    return GameMapActors.getActorByLayerAndPosition(self, layer, position)
end

function GameMap:getActorsByRange(position, radius)
    return GameMapActors.getActorsByRange(self, position, radius)
end

function GameMap:getActorByTag(tag)
    return GameMapActors.getActorByTag(self, tag)
end

function GameMap:getAllActorsByTag(tag)
    return GameMapActors.getAllActorsByTag(self, tag)
end

function GameMap:removeActorsByTags(tags)
    return GameMapActors.removeActorsByTags(self, tags)
end

function GameMap:applyActorPositions(actorPositions)
    return GameMapActors.applyActorPositions(self, actorPositions)
end

function GameMap:spawnActor(actor, layer, emitCreateEvent)
    return GameMapActors.spawnActor(self, actor, layer, emitCreateEvent)
end

function GameMap:createActor(actorClass, layer, kwargs, emitCreateEvent)
    return GameMapActors.createActor(self, actorClass, layer, kwargs, emitCreateEvent)
end

function GameMap:initialiseActorsAndComponents()
    return GameMapActors.initialiseActorsAndComponents(self)
end

function GameMap:_addActorTreeToLayer(actor, layer)
    return GameMapActors._addActorTreeToLayer(self, actor, layer)
end

function GameMap:_addActorToLayer(actor, layer)
    return GameMapActors._addActorToLayer(self, actor, layer)
end

function GameMap:destroyActor(actor)
    return GameMapActors.destroyActor(self, actor)
end

function GameMap:playActorPixelShatterEffect(actor)
    return GameMapActors.playActorPixelShatterEffect(self, actor)
end

function GameMap:findPathResult(start, goal, actor, excludedAnchors)
    return GameMapActors.findPathResult(self, start, goal, actor, excludedAnchors)
end

function GameMap:_clearActorsPathfindingBlocks()
    return GameMapActors._clearActorsPathfindingBlocks(self)
end

function GameMap:findPath(start, goal, actor, excludedAnchors)
    return GameMapActors.findPath(self, start, goal, actor, excludedAnchors)
end

function GameMap:isPathfindingPassable(actor, targetPosition)
    return GameMapActors.isPathfindingPassable(self, actor, targetPosition)
end

function GameMap:hasPathBlockingOverlapActor(actor, targetPosition)
    return GameMapActors.hasPathBlockingOverlapActor(self, actor, targetPosition)
end

function GameMap:_getDescendantActorIDs(actor)
    return GameMapActors._getDescendantActorIDs(self, actor)
end

function GameMap:updateActorList()
    return GameMapActors.updateActorList(self)
end

function GameMap:_updateActorPixelShatterEffects(deltaTime)
    return GameMapActors._updateActorPixelShatterEffects(self, deltaTime)
end

function GameMap:getTerrainTile(layerName, position)
    return GameMapTerrain.getTerrainTile(self, layerName, position)
end

function GameMap:getTerrainTilePositions(layerName, tileID)
    return GameMapTerrain.getTerrainTilePositions(self, layerName, tileID)
end

function GameMap:setTerrainTile(layerName, position, tileID)
    return GameMapTerrain.setTerrainTile(self, layerName, position, tileID)
end

function GameMap:setTerrainTiles(layerName, positions, tileID)
    return GameMapTerrain.setTerrainTiles(self, layerName, positions, tileID)
end

function GameMap:applyTerrainDestructions(terrainDestructions)
    return GameMapTerrain.applyTerrainDestructions(self, terrainDestructions)
end

function GameMap:markPassabilityDirty()
    return GameMapTerrain.markPassabilityDirty(self)
end

function GameMap:updateActorOccupancy(actor)
    return GameMapTerrain.updateActorOccupancy(self, actor)
end

function GameMap:_replaceTerrainLayer(_layerName, layer, layerData, autoTileTextures, autoTileFrameCounts)
    return GameMapTerrain._replaceTerrainLayer(
        self, _layerName, layer, layerData, autoTileTextures, autoTileFrameCounts
    )
end

function GameMap:_getMaterialPropertyTexture(functionName, invalidValue, smooth)
    return GameMapTerrain._getMaterialPropertyTexture(self, functionName, invalidValue, smooth)
end

function GameMap:_rebuildPassabilityCache()
    return GameMapTerrain._rebuildPassabilityCache(self)
end

function GameMap:getLights()
    return GameMapLighting.getLights(self)
end

function GameMap:setLights(lights)
    return GameMapLighting.setLights(self, lights)
end

function GameMap:addLight(light)
    return GameMapLighting.addLight(self, light)
end

function GameMap:removeLight(light)
    return GameMapLighting.removeLight(self, light)
end

function GameMap:_requireLight(light)
    return GameMapLighting._requireLight(self, light)
end

function GameMap:setLightPosition(light, position)
    return GameMapLighting.setLightPosition(self, light, position)
end

function GameMap:setLightColour(light, colour)
    return GameMapLighting.setLightColour(self, light, colour)
end

function GameMap:setLightRadius(light, radius)
    return GameMapLighting.setLightRadius(self, light, radius)
end

function GameMap:setLightIntensity(light, intensity)
    return GameMapLighting.setLightIntensity(self, light, intensity)
end

function GameMap:getAmbientLight()
    return GameMapLighting.getAmbientLight(self)
end

function GameMap:setAmbientLight(ambientLight)
    return GameMapLighting.setAmbientLight(self, ambientLight)
end

function GameMap:getMaterialPropertyMap(functionName, invalidValue)
    return GameMapLighting.getMaterialPropertyMap(self, functionName, invalidValue)
end

function GameMap:getActorLayerLightBlockMap(layerName, size)
    return GameMapLighting.getActorLayerLightBlockMap(self, layerName, size)
end

function GameMap:_lightingShadersAvailable()
    return GameMapLighting._lightingShadersAvailable(self)
end

function GameMap:_getActiveLights()
    return GameMapLighting._getActiveLights(self)
end

function GameMap:_renderLighting(mapLights)
    return GameMapLighting._renderLighting(self, mapLights)
end

function GameMap:refreshShader()
    return GameMapLighting.refreshShader(self)
end

function GameMap:_getMaterialShader()
    return GameMapLighting._getMaterialShader(self)
end

function GameMap:getPlayer()
    return GameMapPresentation.getPlayer(self)
end

function GameMap:setPlayer(player)
    return GameMapPresentation.setPlayer(self, player)
end

function GameMap:worldToMapViewPosition(position)
    return GameMapPresentation.worldToMapViewPosition(self, position)
end

function GameMap:worldToUIScreenPosition(position)
    return GameMapPresentation.worldToUIScreenPosition(self, position)
end

function GameMap:worldToCanvasPosition(position)
    return GameMapPresentation.worldToCanvasPosition(self, position)
end

function GameMap:_updateAudioListener()
    return GameMapPresentation._updateAudioListener(self)
end

function GameMap:_getAudioListenerDirection(direction)
    return GameMapPresentation._getAudioListenerDirection(self, direction)
end

function GameMap:_resetTransparentTiles()
    return GameMapRendering._resetTransparentTiles(self)
end

function GameMap:_drawActor(target, states, actor, actorAlpha)
    return GameMapRendering._drawActor(self, target, states, actor, actorAlpha)
end

function GameMap:_setActorEffectHidden(actor, hidden)
    return GameMapRendering._setActorEffectHidden(self, actor, hidden)
end

function GameMap:_prepareActorPixelShatterEffects()
    return GameMapRendering._prepareActorPixelShatterEffects(self)
end

function GameMap:_drawActorPixelShatterEffects(target, layerName)
    return GameMapRendering._drawActorPixelShatterEffects(self, target, layerName)
end

return class(GameMap, GameMapBase)
