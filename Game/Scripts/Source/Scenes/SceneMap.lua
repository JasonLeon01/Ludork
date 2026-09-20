local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Logging = require("Global.Utils.Logging")
local GameSystem = require("Source.System")
local EventKeys = require("Source.Configs.EventKeys")
local RegionDict = require("Source.Configs.RegionDict")
local GameplayScene = require("Source.Gameplay.GameplayScene")
local ConditionalActor = require("Source.ConditionalActor")
local Teleporter = require("Source.Teleporter")
local MapPath = require("Source.MapPath")
local LiveDebug = require("Source.LiveDebug")
local SceneMapInteractions = require("Source.Scenes.SceneMap.Interactions")
local SceneMapAudioController = require("Source.SceneComponents.MapAudio")
local SceneMapBuilder = require("Source.SceneComponents.MapBuilder")
local RegionTitleController = require("Source.Scenes.SceneMap.RegionTitle.Controller")
local PlayerAttrHUD = require("Source.Windows.HUDPlayerAttr")
local SceneMapWindows = require("Source.Scenes.SceneMap.Windows")

local Input = Engine.Input
local GlobalSystem = GlobalCore.System
local ManagerFunctions = GlobalFunctions.Manager

local WORLD_AMBIENT_TRANSITION_TIME = 0.5

---@param from  sf.Color
---@param to    sf.Color
---@param alpha number
---@return sf.Color
local function interpolateColour(from, to, alpha)
    return sf.Color.new(
        math.round(math.lerp(from.r, to.r, alpha)), math.round(math.lerp(from.g, to.g, alpha)),
        math.round(math.lerp(from.b, to.b, alpha)), math.round(math.lerp(from.a, to.a, alpha))
    )
end

---@class (partial) Source.Scenes.SceneMap.SceneMap: Source.Gameplay.GameplayScene
local Scene = {}

---@diagnostic disable-next-line: unused
function Scene:onEnter()
    GlobalSystem.setTransition()
end

function Scene:setInst(inst)
    self._gameOverRequest = nil
    self.inst = inst
end

function Scene:onCreate()
    self._gameplayRequestsActive = true
    self._gameOverRequest = nil
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
    self._dialogueLocaleSource = nil
    SceneMapWindows.Create(self)
    self._regionTitleUI = RegionTitleController.new(GlobalSystem.getGameSize())
    self._regionTitleUI:prepare()
    self._regionTitleText = self._regionTitleUI:getText()
    self._playerHUD:mount(uiManager)
    self._localeChangedToken = Engine.subscribe(EventKeys.LocaleChanged, function ()
        local scene = sceneRef[1]
        if scene ~= nil then
            scene:refreshLocale()
        end
    end)

    self._gameMap = nil
    self._cachedMapFile = nil
    self._currentRegion = nil
    self._mapClickMoveBlockedUntilLateTick = false
    self._mapInputBlockFrames = 0
    self._pendingMenuOpen = false
    self._pendingSaveLoadOpen = nil
    self._pendingQuickSave = false
    self._pendingTeleporterTransfer = nil
    self._pendingWorldTransfer = nil
    self._mapTransferInProgress = false
    self._worldEnvironmentKey = nil
    self._worldAmbientStartColour = nil
    self._worldAmbientTargetColour = nil
    self._worldAmbientTransitionElapsed = 0
    local startMap = self.inst:getCurrentMapPath() or GameSystem.GetStartMap()
    self:gotoMapAndPos(startMap, nil, true)
    LiveDebug.BindScene(self)
end

function Scene:onQuit()
    ManagerFunctions.stopVoice()
    self._mapAudio:stopMapAudio()
    GlobalSystem.clearWeather()
    GlobalSystem.clearFog()
    GlobalSystem.clearPanorama()
end

function Scene:onDestroy()
    LiveDebug.UnbindScene(self)
    self._gameplayRequestsActive = false
    self._gameOverRequest = nil
    if self._gameMap ~= nil then
        ConditionalActor.ReleaseMapMonitors(self._gameMap)
        self._gameMap:disposeStreaming()
    end
    ManagerFunctions.stopVoice()
    if self._localeChangedToken ~= nil then
        Engine.unsubscribe(self._localeChangedToken)
        self._localeChangedToken = nil
    end
    self._dialogueLocaleSource = nil
    self._mapAudio:stopMapAudio()
    SceneMapWindows.Dispose(self)
    self._playerHUD:dispose()
    self._regionTitleUI:dispose()
end

function Scene:refreshLocale()
    local messageWindow = self._messageWindow:peek()
    if self._dialogueLocaleSource ~= nil and messageWindow ~= nil and messageWindow:isInDialogue() then
        if self._dialogueLocaleSource.kind == "selection" then
            ---@cast self._dialogueLocaleSource Source.Scenes.SceneMap.DialogueSelectionLocaleSource
            local name, options = Scene.FormatDialogueSelectionSource(self._dialogueLocaleSource)
            messageWindow:refreshSelection(name, options)
        else
            ---@cast self._dialogueLocaleSource Source.Scenes.SceneMap.DialogueMessageLocaleSource
            local name, message = Scene.FormatDialogueMessageSource(self._dialogueLocaleSource)
            messageWindow:refreshMessage(name, message)
        end
    end
    local menu = self._windowMenu:peek()
    if menu ~= nil then
        menu:refreshRows()
    end
    local windows = {
        self._windowItem, self._windowEquip, self._windowShop, self._windowAttrShop, self._windowEnemyBook,
        self._windowEnemyEncyclopedia, self._windowFloorTeleporter, self._windowPlayerName
    }
    for _, lazyWindow in ipairs(windows) do
        local window = lazyWindow:peek()
        if window ~= nil then
            window:refreshLocale()
        end
    end
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
        if Class.isInstance(self, hotKeyConfig.Scene) and hotKeyConfig.Filter ~= nil
            and table.contains(hotKeyConfig.Filter, "casual") then
            local functionWhenPressed = hotKeyConfig.FunctionWhenPressed
            if functionWhenPressed ~= nil and Input.getKeyPressed(key, false) then
                if functionWhenPressed(self) then
                    Input.getKeyPressed(key, true)
                end
            end
            local functionWhenReleased = hotKeyConfig.FunctionWhenReleased
            if functionWhenReleased ~= nil and Input.getKeyReleased(key, false) then
                if functionWhenReleased(self) then
                    Input.getKeyReleased(key, true)
                end
            end
        end
    end
end

function Scene:onTick(deltaTime)
    self._mapAudio:onTick(deltaTime)
    if self._dialogueLocaleSource ~= nil and not self:_isInDialogue() then
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
    if self._mapClickMoveBlockedUntilLateTick or self:_isMapClickMoveBlocked() then
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
        gameMap = self._mapBuilder:generateGameMap(mapData, nil, false)
    end
    if self._gameMap ~= nil then
        ConditionalActor.ReleaseMapMonitors(self._gameMap)
        self._gameMap:disposeStreaming()
    end
    self._gameOverRequest = nil
    self._gameMap = gameMap
    self._cachedMapFile = mapFile
    gameMap:setScene(self)
    self.inst:applyMapInfo(mapFile, initialPosition)
    if not gameMap:isWorldMap() then
        gameMap:applyTerrainDestructions(self.inst:getTerrainDestructions(mapFile))
        self._mapBuilder:applyAddedActors(gameMap, self.inst:getAddedActors(mapFile), false)
        gameMap:applyActorPositions(self.inst:getActorPositions(mapFile))
        gameMap:removeActorsByTags(self.inst:getDestroyedActors(mapFile))
    end
    gameMap:setPlayer(self.player)
    gameMap:spawnActor(self.player, "default")
    self._worldEnvironmentKey = nil
    self._worldAmbientStartColour = nil
    self._worldAmbientTargetColour = nil
    self._worldAmbientTransitionElapsed = 0
    if not gameMap:isWorldMap() then
        ---@cast mapData Source.SceneComponents.MapData
        self._mapAudio:playMapAudio(mapData)
        GlobalSystem.clearFog()
        GlobalSystem.applyFogFromMapData(GlobalCore.MapFogSettings.new({
                fog = mapData.fog,
                fogPower = mapData.fogPower,
                fogOx = mapData.fogOx,
                fogOy = mapData.fogOy,
                fogDistort = mapData.fogDistort
            }))
        GlobalSystem.clearPanorama()
        GlobalSystem.applyPanoramaFromMapData(GlobalCore.MapPanoramaSettings.new({
                panorama = mapData.panorama
            }))
    end
    self:_updateCurrentRegion(mapFile)
    Engine.publish(EventKeys.PlayerChanged, {
        owner = self.player,
        kind = EventKeys.PlayerChangeKind.Map
    })
    Logging.info("Loaded map %s in %.3fs", mapFile, perfCounter() - startTime)
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
            self._worldAmbientTargetColour = targetColour:copy()
            worldMap:setAmbientLight(self._worldAmbientTargetColour)
        elseif self._worldAmbientTargetColour ~= targetColour then
            self._worldAmbientStartColour = worldMap:getAmbientLight():copy()
            self._worldAmbientTargetColour = targetColour:copy()
            self._worldAmbientTransitionElapsed = 0
        end
    end
    if self._worldAmbientStartColour == nil then
        return
    end
    self._worldAmbientTransitionElapsed = math.clamp(
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
    self:_processPendingTeleporterTransfer()
    self:_processPendingWorldTransfer()
    if self._pendingMenuOpen then
        self._pendingMenuOpen = false
        Scene.CaptureScreenSnapshot()
        self._windowMenu:get():open()
    elseif self._pendingSaveLoadOpen ~= nil or self._pendingQuickSave then
        Scene.CaptureScreenSnapshot()
        self:_processPendingSaveLoadOpen()
        self:_processPendingQuickSave()
    end
end

function Scene.CaptureScreenSnapshot()
    local clock = sf.Clock.new()
    local canvas = GlobalSystem.getCanvas()
    local sourceTexture = canvas:getTexture()
    local sourceSize = sourceTexture:getSize()
    if sourceSize.x == 0 or sourceSize.y == 0 then
        GameSystem.SetSavedScreenImage(nil)
        return
    end
    local scale = math.min(224 / sourceSize.x, 168 / sourceSize.y, 1)
    local previewSize = sf.Vector2u.new(
        math.max(1, math.floor(sourceSize.x * scale)), math.max(1, math.floor(sourceSize.y * scale))
    )
    ---@cast previewSize sf.Vector2u
    local scaled = sf.RenderTexture.new(previewSize)
    scaled:clear(sf.Color.Black)
    local sprite = sf.Sprite.new(sourceTexture)
    sprite:setScale(sf.Vector2f.new(previewSize.x / sourceSize.x, previewSize.y / sourceSize.y))
    scaled:draw(sprite)
    scaled:display()
    GameSystem.SetSavedScreenImage(scaled:getTexture():copyToImage())
    Logging.info("Save screenshot capture: %.2f ms", clock:getElapsedTime():asMicroseconds() / 1000)
end

---@return boolean
function Scene.IsMenuOpenTriggered()
    return Input.isMouseButtonTriggered(sf.Mouse.Button.Right, true)
        or Input.isAnyJoystickButtonTriggered(Engine.JoystickButton.getMenu(), true)
end

---@return boolean
function Scene:_canOpenMenu()
    return not self._pendingMenuOpen and self._pendingSaveLoadOpen == nil and not self._pendingQuickSave
        and not self:_isMenuBlocking() and not self:_isInDialogue() and not self:_hasVisibleBlockingWindow()
end

---@return boolean
function Scene:_canOpenItemOverlay()
    local menu = self._windowMenu:peek()
    local item = self._windowItem:peek()
    return menu ~= nil and menu:getVisible() and item ~= nil and item:getVisible() and not self:_isInDialogue()
end

---@return boolean
function Scene:_isMapClickMoveBlocked()
    return self:_isInDialogue() or self:_isMenuBlocking()
        or self:_hasVisibleBlockingWindow() or self._mapInputBlockFrames > 0
end

---@return boolean
function Scene:_isInDialogue()
    local window = self._messageWindow:peek()
    return window ~= nil and window:isInDialogue()
end

---@return boolean
function Scene:_isMenuBlocking()
    local window = self._windowMenu:peek()
    return window ~= nil and window:isBlocking()
end

function Scene.ConsumeMapClickMoveInput()
    Input.isMouseButtonTriggered(sf.Mouse.Button.Left, true)
    Input.isTouchTap(true)
    Input.isTouchTriggered(true)
end

---@param mapKey              string
---@param telepoint           sf.Vector2u
---@param previewSize         integer
---@param previewScale        number
---@param showTelepointMarker boolean
---@return sf.Texture
function Scene:_buildFloorMapPreview(mapKey, telepoint, previewSize, previewScale, showTelepointMarker)
    return self._mapBuilder:buildFloorMapPreview(
        self.inst, self:_getCurrentRegionMap(), mapKey, telepoint, previewSize, previewScale, showTelepointMarker,
        self:getGameMap()
    )
end

function Scene:resolveRegionMapPath(mapKey)
    return self._mapBuilder:resolveRegionMapPath(mapKey, self:_getCurrentRegionMap())
end

---@return string
function Scene:_getCurrentRegionMap()
    return self._cachedMapFile or self.inst:getCurrentMapPath() or GameSystem.GetStartMap()
end

---@param mapFile string
function Scene:_updateCurrentRegion(mapFile)
    local region = Scene.FindRegionForMap(mapFile)
    self.inst:setCurrentRegion(region or "")
    if region == self._currentRegion then
        return
    end
    self._currentRegion = region
    if region ~= nil then
        Scene.ShowRegionTitle(region)
    end
end

---@param mapFile string
---@return string | nil
function Scene.FindRegionForMap(mapFile)
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
    RegionTitleController.Publish({
        region = region
    })
end

---@param deltaTime number
function Scene:_updateRegionTitle(deltaTime)
    self._regionTitleUI:update(deltaTime)
end

function Scene:getGameMap()
    return SceneMapInteractions.GetGameMap(self)
end

function Scene:getGameInstance()
    return self.inst
end

---@param teleporter Source.Teleporter.Teleporter
---@return boolean
function Scene:_canRequestTeleporterTransfer(teleporter)
    if not self._gameplayRequestsActive or GlobalSystem.getScene() ~= self or self._mapTransferInProgress
        or self._pendingTeleporterTransfer ~= nil or self._pendingWorldTransfer ~= nil or self._gameMap == nil
        or teleporter:isDestroyed() or not teleporter:isVisibleInHierarchy() or teleporter:getMap() ~= self._gameMap then
        return false
    end
    return self._gameMap:getPlayer() ~= nil and bool(self._cachedMapFile)
end

function Scene:requestFloorStep(teleporter, step)
    assert(step == 1 or step == -1, "Floor transfer step must be 1 or -1")
    if not self:_canRequestTeleporterTransfer(teleporter) then
        return false
    end
    ---@cast self._cachedMapFile string
    local region = Scene.FindRegionForMap(self._cachedMapFile)
    local regionMaps = region ~= nil and RegionDict[region] or {}
    local currentIndex = Teleporter.FindCurrentMapIndex(regionMaps, self._cachedMapFile)
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
    return self:_startTeleporterTransfer(teleporter, targetMap, anchorPosition, true, true)
end

function Scene:requestMapTransfer(teleporter, mapPath, position, record)
    if not self:_canRequestTeleporterTransfer(teleporter) or not bool(mapPath) then
        return false
    end
    local targetMap, targetPosition = self._mapBuilder:resolveMapDestination(
        mapPath, self:_getCurrentRegionMap(), position
    )
    assert(targetPosition ~= nil, "Teleporter transfer requires a target position")
    return self:_startTeleporterTransfer(teleporter, targetMap, targetPosition, false, record ~= false)
end

---@param teleporter     Source.Teleporter.Teleporter
---@param targetMap      string
---@param targetPosition sf.Vector2i
---@param findNearest    boolean
---@param record         boolean
---@return boolean
function Scene:_startTeleporterTransfer(teleporter, targetMap, targetPosition, findNearest, record)
    local player = self:getGameMap():getPlayer()
    assert(player ~= nil, "Teleporter transfer requires a player")
    local moveEnabled = player:getMoveEnabled()
    player:setMoveEnabled(false)
    if not self:requestTeleporterTransfer(targetMap, targetPosition, moveEnabled, findNearest, record) then
        player:setMoveEnabled(moveEnabled)
        return false
    end
    if record then
        local sourcePosition = teleporter:getTeleportPosition()
        local sourceTelepoint = sf.Vector2u.new(sourcePosition.x, sourcePosition.y)
        ---@cast sourceTelepoint sf.Vector2u
        self.inst:recordTelepoint(assert(self._cachedMapFile), sourceTelepoint, teleporter:getMapTag())
    end
    GlobalCore.AudioManager.playSound(teleporter.stairSE)
    return true
end

function Scene:requestGameOver(player, delay)
    assert(math.isFinite(delay) and delay >= 0, "Game over delay must be finite and non-negative")
    if not self._gameplayRequestsActive or GlobalSystem.getScene() ~= self
        or self._gameMap == nil or player ~= self.player
        or self._gameMap:getPlayer() ~= player or player:isDestroyed()
        or player:getMap() ~= self._gameMap or self._gameOverRequest ~= nil then
        return
    end
    local request = { player = player, gameMap = self._gameMap }
    self._gameOverRequest = request
    local function finishGameOver()
        if not self._gameplayRequestsActive or self._gameOverRequest ~= request
            or GlobalSystem.getScene() ~= self or self.player ~= request.player
            or self._gameMap ~= request.gameMap or request.gameMap:getPlayer() ~= request.player
            or request.player:isDestroyed() or request.player:getMap() ~= request.gameMap then
            return
        end
        local SceneGameOver = require("Source.Scenes.SceneGameOver")

        self._gameplayRequestsActive = false
        self._gameOverRequest = nil
        GlobalSystem.setScene(SceneGameOver.new())
    end
    if delay == 0 then
        finishGameOver()
    else
        self:addTimer(delay, finishGameOver)
    end
end

function Scene:showMessage(name, message, refActor, localeArgs)
    return SceneMapInteractions.ShowMessage(self, name, message, refActor, localeArgs)
end

function Scene:showSelection(name, options, refActor, allowCancel, localeArgs)
    return SceneMapInteractions.ShowSelection(self, name, options, refActor, allowCancel, localeArgs)
end

function Scene:applyLoadedGame(inst)
    return SceneMapInteractions.ApplyLoadedGame(self, inst)
end

function Scene:applyPrimaryPlayer()
    return SceneMapInteractions.ApplyPrimaryPlayer(self)
end

function Scene:_rebindPlayerToUI()
    return SceneMapInteractions.RebindPlayerToUI(self)
end

function Scene:showEnemyBook()
    return SceneMapInteractions.ShowEnemyBook(self)
end

function Scene:showFloorTeleporter()
    return SceneMapInteractions.ShowFloorTeleporter(self)
end

function Scene:openMenu()
    return SceneMapInteractions.OpenMenu(self)
end

function Scene:openSaveUI()
    return SceneMapInteractions.OpenSaveUI(self)
end

function Scene:openLoadUI()
    return SceneMapInteractions.OpenLoadUI(self)
end

function Scene:openItemUI()
    return SceneMapInteractions.OpenItemUI(self)
end

function Scene:openEquipUI()
    return SceneMapInteractions.OpenEquipUI(self)
end

function Scene:quickSave()
    return SceneMapInteractions.QuickSave(self)
end

function Scene:quickLoad()
    return SceneMapInteractions.QuickLoad(self)
end

function Scene:openShop(buyItemIDs, canSell)
    return SceneMapInteractions.OpenShop(self, buyItemIDs, canSell)
end

function Scene:openAttrShop(actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName)
    return SceneMapInteractions.OpenAttrShop(
        self, actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName
    )
end

function Scene:_onShopClose()
    return SceneMapInteractions.OnShopClose(self)
end

function Scene:_onPlayerNameClose()
    return SceneMapInteractions.OnPlayerNameClose(self)
end

function Scene:_onAttrShopClose()
    return SceneMapInteractions.OnAttrShopClose(self)
end

function Scene:_onEnemyBookClose()
    return SceneMapInteractions.OnEnemyBookClose(self)
end

function Scene:_onEnemyBookConfirm(entry)
    return SceneMapInteractions.OnEnemyBookConfirm(self, entry)
end

function Scene:_onEnemyEncyclopediaClose()
    return SceneMapInteractions.OnEnemyEncyclopediaClose(self)
end

function Scene:_onFloorTeleporterClose()
    return SceneMapInteractions.OnFloorTeleporterClose(self)
end

function Scene:_onFloorTeleporterConfirm(mapKey, telepoint)
    return SceneMapInteractions.OnFloorTeleporterConfirm(self, mapKey, telepoint)
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

function Scene:_canRestoreMoveAfterMenuClose()
    return SceneMapInteractions.CanRestoreMoveAfterMenuClose(self)
end

function Scene:openPlayerName()
    return SceneMapInteractions.OpenPlayerName(self)
end

function Scene:_hasVisibleBlockingWindow()
    return SceneMapInteractions.HasVisibleBlockingWindow(self)
end

function Scene:_blockMapInput(frames)
    return SceneMapInteractions.BlockMapInput(self, frames)
end

function Scene:requestTeleporterTransfer(targetMap, targetPosition, moveEnabled, findNearest, record)
    return SceneMapInteractions.RequestTeleporterTransfer(
        self, targetMap, targetPosition, moveEnabled, findNearest, record
    )
end

function Scene:_processPendingTeleporterTransfer()
    return SceneMapInteractions.ProcessPendingTeleporterTransfer(self)
end

function Scene:_cancelTeleporterTransfer(moveEnabled)
    return SceneMapInteractions.CancelTeleporterTransfer(self, moveEnabled)
end

function Scene:_applyMapDestination(targetMap, targetPosition, blockTransition)
    return SceneMapInteractions.ApplyMapDestination(self, targetMap, targetPosition, blockTransition)
end

function Scene:_queueWorldTransfer(targetMap, targetPosition)
    return SceneMapInteractions.QueueWorldTransfer(self, targetMap, targetPosition)
end

function Scene:_processPendingWorldTransfer()
    return SceneMapInteractions.ProcessPendingWorldTransfer(self)
end

function Scene:_getSaveSource()
    return SceneMapInteractions.GetSaveSource(self)
end

function Scene:_onSaveLoadClose(reason)
    return SceneMapInteractions.OnSaveLoadClose(self, reason)
end

function Scene:_onHotkeySubMenuClose()
    return SceneMapInteractions.OnHotkeySubMenuClose(self)
end

function Scene:_onHotkeyItemUsed()
    return SceneMapInteractions.OnHotkeyItemUsed(self)
end

function Scene:_processPendingSaveLoadOpen()
    return SceneMapInteractions.ProcessPendingSaveLoadOpen(self)
end

function Scene:_processPendingQuickSave()
    return SceneMapInteractions.ProcessPendingQuickSave(self)
end

function Scene:_onConfigClose()
    return SceneMapInteractions.OnConfigClose(self)
end

function Scene:gotoMapAndPos(mapPath, pos, blockTransition)
    return SceneMapInteractions.GotoMapAndPos(self, mapPath, pos, blockTransition)
end

function Scene:tryCenterSymmetricTeleport()
    return SceneMapInteractions.TryCenterSymmetricTeleport(self)
end

function Scene:tryAdjacentFloorSamePos(step)
    return SceneMapInteractions.TryAdjacentFloorSamePos(self, step)
end

function Scene:_isMapPositionPassable(mapPath, actor, position)
    return SceneMapInteractions.IsMapPositionPassable(self, mapPath, actor, position)
end

function Scene:recordAddedActor(actor)
    return SceneMapInteractions.RecordAddedActor(self, actor)
end

function Scene:recordActorPosition(actor, position)
    return SceneMapInteractions.RecordActorPosition(self, actor, position)
end

function Scene:recordDestroyedActor(actor)
    return SceneMapInteractions.RecordDestroyedActor(self, actor)
end

function Scene:recordDestroyedActorTag(actorTag)
    return SceneMapInteractions.RecordDestroyedActorTag(self, actorTag)
end

function Scene:recordTerrainDestructions(layerName, positions)
    return SceneMapInteractions.RecordTerrainDestructions(self, layerName, positions)
end

return class(Scene, GameplayScene)
