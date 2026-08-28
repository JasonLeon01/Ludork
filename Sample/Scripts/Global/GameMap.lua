local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local DamageTextParticle = require("Global.CustomParticles.DamageTextParticle")
local Pool = require("Global.Pool")
local GameMapActors = require("Global.GameMap.Actors")
local GameMapTerrain = require("Global.GameMap.Terrain")
local GameMapLighting = require("Global.GameMap.Lighting")
local GameMapRendering = require("Global.GameMap.Rendering")

local ManagerFunctions = GlobalFunctions.Manager
local ShaderManager = GlobalCore.ShaderManager
local System = GlobalCore.System
local WeatherController = GlobalCore.WeatherController
local FogController = GlobalCore.FogController

local Actor = Engine.Actor
local Camera = GlobalCore.Camera
local Character = Engine.Character
local GameMapBase = GlobalCore.GameMapBase

local LISTENER_DIRECTION_UP = sf.Vector3f.new(0.0, -1.0, 0.0)
local LISTENER_DIRECTION_DOWN = sf.Vector3f.new(0.0, 1.0, 0.0)
local LISTENER_DIRECTION_LEFT = sf.Vector3f.new(-1.0, 0.0, 0.0)
local LISTENER_DIRECTION_RIGHT = sf.Vector3f.new(1.0, 0.0, 0.0)
local DEFAULT_LISTENER_DIRECTION = sf.Vector3f.new(0.0, 0.0, -1.0)
local CHARACTER_LISTENER_UP_VECTOR = sf.Vector3f.new(0.0, 0.0, -1.0)
local DEFAULT_LISTENER_UP_VECTOR = sf.Vector3f.new(0.0, 1.0, 0.0)

---@class (partial) GameMap
local GameMap = {}

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
            local materialShaderPath = sparseWorldConfig == nil and "Global/Material.frag"
                or "Global/WorldMaterial.frag"
            materialShader = ManagerFunctions.loadShader(materialShaderPath, sf.Shader.Type.Fragment)
            lightPassShader = ManagerFunctions.loadShader("Global/LightPass.frag", sf.Shader.Type.Fragment)
            unobstructedLightPassShader = ManagerFunctions.loadShader(
                "Global/UnoccludedLightPass.frag", sf.Shader.Type.Fragment
            )
            actorPixelShatterShader = assert(
                ShaderManager.loadFull("Global/ActorPixelShatter.vert", "Global/ActorPixelShatter.frag"),
                "Actor pixel shatter shader must not be nil"
            )
        end
        actorHueShader = ManagerFunctions.loadShader("Global/Hue.frag", sf.Shader.Type.Fragment)
    end
    GameMapBase.init(self)
    if sparseWorldConfig ~= nil then
        self:configureSparseWorld(
            sparseWorldConfig.size, sparseWorldConfig.layerOrder, sparseWorldConfig.tilePassabilityQuery,
            sparseWorldConfig.directionPassabilityQuery, sparseWorldConfig.topMaterialQuery
        )
    end
    self._tilemapLightMaskShader = tilemapLightMaskShader
    self._lightMaskShader = lightMaskShader
    self._lightPassShader = lightPassShader
    self._unobstructedLightPassShader = unobstructedLightPassShader
    self._materialShader = materialShader
    self._actorHueShader = actorHueShader
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
    self._lightBlockSize = sf.Vector2f.new(Engine.CellSize, Engine.CellSize)
    local tilemapSize = sparseWorldConfig ~= nil and sparseWorldConfig.size or self._tilemap:getSize()
    self._shaderMapSize = sf.Vector2f.new(tilemapSize.x, tilemapSize.y)
    self._playerCoverColour = sf.Color.new(255, 255, 255, GameMap.DefaultCoverAlpha)
    self._staticTransmission = nil
    self._staticOccupancy = nil
    self._staticOccupancyPrefix = nil
    self._surfaceMask = nil
    self._dynamicTransmission = nil
    self._directLight = nil
    self._directLightCleared = false
    self._staticDirectLight = nil
    self._useStaticDirectLight = false
    self._lightPassQuad = nil
    self._unobstructedLightVertices = nil
    self._unobstructedLightVertex = nil
    self._unobstructedLightCache = nil
    self._cachedActiveLights = nil
    self._cachedLightMaterialRevision = -1
    self._cachedLightTransmissionSignature = nil
    self._staticTransmissionRevision = -1
    self._staticTransmissionSignature = nil
    self._staticHasTransmissionLoss = false
    self._staticTextureOrigin = sf.Vector2f.new(0.0, 0.0)
    self._staticTextureSize = sf.Vector2f.new(1.0, 1.0)
    self._staticOccupancyOrigin = sf.Vector2f.new(0.0, 0.0)
    self._staticOccupancySize = sf.Vector2f.new(1.0, 1.0)
    self._dynamicTransmissionPixelSize = 0
    self._zeroShaderOffset = sf.Vector2f.new(0.0, 0.0)
    self._shaderColour = sf.Vector3f.new(0.0, 0.0, 0.0)
    self._actorShaderBuffer = nil
    self._actorHueBuffer = nil
    self._actorHueSourceSprite = nil
    self._materialDirty = true
    self._materialRevision = 0
    self._layerMaskTextureCache = {}
    self._shaderTime = 0.0
    self._transparentTiles = {}
    self._coverLayerStates = nil
    self._coverPlayerX = nil
    self._coverPlayerY = nil
    self._coverPlayerLayerIndex = nil
    self._coverAlpha = nil
    self._coverMaterialRevision = nil
    self._tilePassableGrid = nil
    self._player = nil
    self._components = {}
    self._autoTileResolver = nil
    self._damageTextSpeedCurve = nil
    self._damageTextConfig = nil
    self._surfaceTileRenderStates = nil
    self._surfaceActorRenderStates = nil
    self._transmissionTileRenderStates = nil
    self._transmissionActorRenderStates = nil
    self._lightPassRenderStates = nil
    self._unobstructedLightPassRenderStates = nil
    if not self._previewOnly then
        ---@cast self._camera GlobalCore.Camera
        local maskPixelSize = sparseWorldConfig == nil and self._tilemap:getSize() * Engine.CellSize
            or assert(self._camera:getRenderTexture()):getSize()
        self._staticTransmission = sf.RenderTexture.new(maskPixelSize)
        self._staticTransmission:setSmooth(false)
        self._surfaceMask = sf.RenderTexture.new(maskPixelSize)
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
    if self._camera ~= nil then
        configureCamera(self, self._camera)
    end
    ---@type GameMap[]
    local selfRef = setmetatable({ self }, {
        __mode = "v"
    })
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

function GameMap:getCamera()
    return self._camera
end

function GameMap:setCamera(camera)
    self._camera = camera
    configureCamera(self, camera)
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

function GameMap:worldToMapViewPosition(position)
    local camera = self:getCamera()
    if camera == nil then
        return copy(position)
    end
    local viewPosition = camera:getViewPosition()
    if viewPosition == nil then
        return copy(position)
    end
    return sf.Vector2f.new(position.x - viewPosition.x, position.y - viewPosition.y)
end

function GameMap:worldToUIScreenPosition(position)
    local mapPosition = self:worldToMapViewPosition(position)
    local mapViewPosition = self._mapViewRect.position
    return sf.Vector2f.new(mapPosition.x + mapViewPosition.x, mapPosition.y + mapViewPosition.y)
end

function GameMap:worldToCanvasPosition(position)
    local uiPosition = self:worldToUIScreenPosition(position)
    local scale = System.getScale()
    return sf.Vector2f.new(uiPosition.x * scale, uiPosition.y * scale)
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
    ---@cast listenerVector sf.Vector3f
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
---@diagnostic disable-next-line: unused
function GameMap:_getAudioListenerDirection(direction)
    if direction == Engine.Direction.UP then
        return LISTENER_DIRECTION_UP
    elseif direction == Engine.Direction.LEFT then
        return LISTENER_DIRECTION_LEFT
    elseif direction == Engine.Direction.RIGHT then
        return LISTENER_DIRECTION_RIGHT
    end
    return LISTENER_DIRECTION_DOWN
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
    states = states or sf.RenderStates.new()
    local applyCover = bool(applyPlayerCover)
    local playerLayerIndex = applyCover and self:_getPlayerLayerIndex(self._layerNames) or -1
    local refreshPlayerCover = false
    local playerPosition = nil
    if applyCover then
        refreshPlayerCover, playerPosition = self:_preparePlayerCover(self._layerNames, playerLayerIndex)
    end
    for index, layerName in ipairs(self._layerNames) do
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
            self:_drawLayerActors(target, states, layerName, index - 1, playerLayerIndex, applyCover)
        end
    end
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
    System.draw(self._camera, self._materialShader)
    FogController.drawOverlay()
    ---@cast self._particleSystem Engine.ParticleSystem
    WeatherController.registerParticleSystem(self._particleSystem)
    System.draw(self._particleSystem)
    System.setWindowDefaultView()
end

return class(GameMap, GameMapBase, GameMapActors, GameMapTerrain, GameMapLighting, GameMapRendering)
