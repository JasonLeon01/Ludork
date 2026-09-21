local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local DamageTextParticle = require("Global.CustomParticles.DamageTextParticle")
local GameMapActors = require("Global.GameMap.Actors")
local GameMapTerrain = require("Global.GameMap.Terrain")
local GameMapLighting = require("Global.GameMap.Lighting")
local GameMapPresentation = require("Global.GameMap.Presentation")
local GameMapRendering = require("Global.GameMap.Rendering")

local Display = GlobalCore.Display
local Graphics = GlobalCore.Graphics
local ShaderManager = GlobalCore.ShaderManager
local WeatherController = GlobalCore.WeatherController
local FogController = GlobalCore.FogController
local PanoramaController = GlobalCore.PanoramaController

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
GameMap.HideDisconnectedRegions = false

---@param gameMap GameMap
---@return sf.IntRect
local function validateMapViewRect(gameMap)
    local rect = gameMap.MapViewRect:copy()
    ---@cast rect sf.IntRect
    local gameSize = Display.getGameSize()
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
            ShaderManager.loadFull(
                "/Game/Assets/Shaders/Global/ActorPixelShatter.vert",
                "/Game/Assets/Shaders/Global/ActorPixelShatter.frag"
            ), "Actor pixel shatter shader must not be nil"
        )
    end
    GameMapBase.init(self)
    self:setHideDisconnectedRegions(self.HideDisconnectedRegions and sparseWorldConfig == nil)
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
        if gameMap ~= nil then
            gameMap:drawLayerEffects(layerName)
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
function GameMap:getGameplayCellRect()
    local size = self:getSize()
    return { x = 0, y = 0, width = size.x, height = size.y }
end

function GameMap:getMapViewRect()
    return self._mapViewRect
end

function GameMap:drawLayerEffects(layerName)
    if self._renderEffectTarget ~= nil then
        self:_drawActorPixelShatterEffects(self._renderEffectTarget, layerName)
    end
end

function GameMap:getScene()
    return self._scene
end

function GameMap:setScene(scene)
    if self._scene ~= nil and self._scene ~= scene then
        self._scene:setEmitterMap(nil)
    end
    self._scene = scene
    if scene ~= nil then
        scene:setEmitterMap(self)
    end
end

function GameMap:addCommonTip(text)
    if self._scene ~= nil then
        self._scene:addCommonTip(text)
    end
end

function GameMap:addDamageText(text, position, sourceActor)
    assert(self._damageTextSpeedCurve ~= nil, "DamageText speed curve is not configured")
    assert(self._damageTextConfig ~= nil, "DamageText config is not configured")
    if not self:isActorVisibleOnMap(sourceActor) then
        return
    end
    local drawPosition = self:worldToMapViewPosition(position)
    DamageTextParticle.new(
        self._particleSystem,
        text,
        drawPosition,
        self._damageTextConfig,
        self._damageTextSpeedCurve,
        function ()
            return self:isActorVisibleOnMap(sourceActor)
        end
    )
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
    Graphics.setWindowMapView(self._mapViewRect)
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
    if self._camera ~= nil then
        PanoramaController.drawUnderlay(self._camera, self._ambientLight)
    end
    ---@diagnostic disable-next-line: param-type-mismatch
    Graphics.draw(self._camera, self:_getMaterialShader())
    FogController.drawOverlay()
    ---@cast self._particleSystem Engine.ParticleSystem
    Graphics.draw(self._particleSystem)
    Graphics.setWindowDefaultView()
end

function GameMap:_syncActorsForMapCache()
    return GameMapActors.SyncActorsForMapCache(self)
end

function GameMap:_syncActorsForPathfinding()
    return GameMapActors.SyncActorsForPathfinding(self)
end

function GameMap:_checkDir4Between(fromPosition, toPosition, direction)
    return GameMapActors.CheckDir4Between(self, fromPosition, toPosition, direction)
end

function GameMap:getAllActors()
    return GameMapActors.GetAllActors(self)
end

function GameMap:getActorLayer(actor)
    return GameMapActors.GetActorLayer(self, actor)
end

function GameMap:getActorsByPosition(position)
    return GameMapActors.GetActorsByPosition(self, position)
end

function GameMap:getActorByLayerAndPosition(layer, position)
    return GameMapActors.GetActorByLayerAndPosition(self, layer, position)
end

function GameMap:getActorsByRange(position, radius)
    return GameMapActors.GetActorsByRange(self, position, radius)
end

function GameMap:getActorByTag(tag)
    return GameMapActors.GetActorByTag(self, tag)
end

function GameMap:getAllActorsByTag(tag)
    return GameMapActors.GetAllActorsByTag(self, tag)
end

function GameMap:removeActorsByTags(tags)
    return GameMapActors.RemoveActorsByTags(self, tags)
end

function GameMap:applyActorPositions(actorPositions)
    return GameMapActors.ApplyActorPositions(self, actorPositions)
end

function GameMap:spawnActor(actor, layer, emitCreateEvent)
    return GameMapActors.SpawnActor(self, actor, layer, emitCreateEvent)
end

function GameMap:createActor(actorClass, layer, kwargs, emitCreateEvent)
    return GameMapActors.CreateActor(self, actorClass, layer, kwargs, emitCreateEvent)
end

function GameMap:initialiseActorsAndComponents()
    return GameMapActors.InitialiseActorsAndComponents(self)
end

function GameMap:_addActorTreeToLayer(actor, layer)
    return GameMapActors.AddActorTreeToLayer(self, actor, layer)
end

function GameMap:_addActorToLayer(actor, layer)
    return GameMapActors.AddActorToLayer(self, actor, layer)
end

function GameMap:destroyActor(actor)
    return GameMapActors.DestroyActor(self, actor)
end

function GameMap:playActorPixelShatterEffect(actor)
    return GameMapActors.PlayActorPixelShatterEffect(self, actor)
end

function GameMap:findPathResult(start, goal, actor, excludedAnchors)
    return GameMapActors.FindPathResult(self, start, goal, actor, excludedAnchors)
end

function GameMap:_clearActorsPathfindingBlocks()
    return GameMapActors.ClearActorsPathfindingBlocks(self)
end

function GameMap:findPath(start, goal, actor, excludedAnchors)
    return GameMapActors.FindPath(self, start, goal, actor, excludedAnchors)
end

function GameMap:isPathfindingPassable(actor, targetPosition)
    return GameMapActors.IsPathfindingPassable(self, actor, targetPosition)
end

function GameMap:hasPathBlockingOverlapActor(actor, targetPosition)
    return GameMapActors.HasPathBlockingOverlapActor(self, actor, targetPosition)
end

function GameMap:_getDescendantActorIDs(actor)
    return GameMapActors.GetDescendantActorIDs(self, actor)
end

function GameMap:updateActorList()
    return GameMapActors.UpdateActorList(self)
end

function GameMap:_updateActorPixelShatterEffects(deltaTime)
    return GameMapActors.UpdateActorPixelShatterEffects(self, deltaTime)
end

function GameMap:getTerrainTile(layerName, position)
    return GameMapTerrain.GetTerrainTile(self, layerName, position)
end

function GameMap:getTerrainTilePositions(layerName, tileID)
    return GameMapTerrain.GetTerrainTilePositions(self, layerName, tileID)
end

function GameMap:setTerrainTile(layerName, position, tileID)
    return GameMapTerrain.SetTerrainTile(self, layerName, position, tileID)
end

function GameMap:setTerrainTiles(layerName, positions, tileID)
    return GameMapTerrain.SetTerrainTiles(self, layerName, positions, tileID)
end

function GameMap:applyTerrainDestructions(terrainDestructions)
    return GameMapTerrain.ApplyTerrainDestructions(self, terrainDestructions)
end

function GameMap:markPassabilityDirty()
    return GameMapTerrain.MarkPassabilityDirty(self)
end

function GameMap:updateActorOccupancy(actor)
    return GameMapTerrain.UpdateActorOccupancy(self, actor)
end

function GameMap:_replaceTerrainLayer(_layerName, layer, layerData, autoTileTextures, autoTileFrameCounts)
    return GameMapTerrain.ReplaceTerrainLayer(self, _layerName, layer, layerData, autoTileTextures, autoTileFrameCounts)
end

function GameMap:_getMaterialPropertyTexture(functionName, invalidValue, smooth)
    return GameMapTerrain.GetMaterialPropertyTexture(self, functionName, invalidValue, smooth)
end

function GameMap:_rebuildPassabilityCache()
    return GameMapTerrain.RebuildPassabilityCache(self)
end

function GameMap:getLights()
    return GameMapLighting.GetLights(self)
end

function GameMap:setLights(lights)
    return GameMapLighting.SetLights(self, lights)
end

function GameMap:addLight(light)
    return GameMapLighting.AddLight(self, light)
end

function GameMap:removeLight(light)
    return GameMapLighting.RemoveLight(self, light)
end

function GameMap:_requireLight(light)
    return GameMapLighting.RequireLight(self, light)
end

function GameMap:setLightPosition(light, position)
    return GameMapLighting.SetLightPosition(self, light, position)
end

function GameMap:setLightColour(light, colour)
    return GameMapLighting.SetLightColour(self, light, colour)
end

function GameMap:setLightRadius(light, radius)
    return GameMapLighting.SetLightRadius(self, light, radius)
end

function GameMap:setLightIntensity(light, intensity)
    return GameMapLighting.SetLightIntensity(self, light, intensity)
end

function GameMap:getAmbientLight()
    return GameMapLighting.GetAmbientLight(self)
end

function GameMap:setAmbientLight(ambientLight)
    return GameMapLighting.SetAmbientLight(self, ambientLight)
end

function GameMap:getMaterialPropertyMap(functionName, invalidValue)
    return GameMapLighting.GetMaterialPropertyMap(self, functionName, invalidValue)
end

function GameMap:getActorLayerLightBlockMap(layerName, size)
    return GameMapLighting.GetActorLayerLightBlockMap(self, layerName, size)
end

function GameMap:_lightingShadersAvailable()
    return GameMapLighting.LightingShadersAvailable(self)
end

function GameMap:_getActiveLights()
    return GameMapLighting.GetActiveLights(self)
end

function GameMap:_renderLighting(mapLights)
    return GameMapLighting.RenderLighting(self, mapLights)
end

function GameMap:refreshShader()
    return GameMapLighting.RefreshShader(self)
end

function GameMap:_getMaterialShader()
    return GameMapLighting.GetMaterialShader(self)
end

function GameMap:getPlayer()
    return GameMapPresentation.GetPlayer(self)
end

function GameMap:setPlayer(player)
    return GameMapPresentation.SetPlayer(self, player)
end

function GameMap:worldToMapViewPosition(position)
    return GameMapPresentation.WorldToMapViewPosition(self, position)
end

function GameMap:worldToUIScreenPosition(position)
    return GameMapPresentation.WorldToUIScreenPosition(self, position)
end

function GameMap:worldToCanvasPosition(position)
    return GameMapPresentation.WorldToCanvasPosition(self, position)
end

function GameMap:_updateAudioListener()
    return GameMapPresentation.UpdateAudioListener(self)
end

function GameMap:_getAudioListenerDirection(direction)
    return GameMapPresentation.GetAudioListenerDirection(self, direction)
end

function GameMap:_resetTransparentTiles()
    return GameMapRendering.ResetTransparentTiles(self)
end

function GameMap:_drawActor(target, states, actor, actorAlpha)
    return GameMapRendering.DrawActor(self, target, states, actor, actorAlpha)
end

function GameMap:_setActorEffectHidden(actor, hidden)
    return GameMapRendering.SetActorEffectHidden(self, actor, hidden)
end

function GameMap:_prepareActorPixelShatterEffects()
    return GameMapRendering.PrepareActorPixelShatterEffects(self)
end

function GameMap:_drawActorPixelShatterEffects(target, layerName)
    return GameMapRendering.DrawActorPixelShatterEffects(self, target, layerName)
end

return class(GameMap, GameMapBase)
