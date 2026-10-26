local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local PlayerChangeKind = require("Enums.PlayerChangeKind")
local EventKey = require("Enums.EventKey")
local Logging = require("Global.Utils.Logging")
local GameSystem = require("Source.System")
local GameplayScene = require("Source.Gameplay.GameplayScene")
local LiveDebug = require("Internal.LiveDebug")
local MapSession = require("Source.Scenes.SceneMap.MapSession")
local Dialogue = require("Source.Scenes.SceneMap.Dialogue")
local Windows = require("Source.Scenes.SceneMap.Windows")
local Environment = require("Source.Scenes.SceneMap.Environment")
local RegionTitleController = require("Source.Scenes.SceneMap.RegionTitle.Controller")
local PlayerAttrHUD = require("Source.Windows.HUDPlayerAttr")
local TutorialController = require("Source.SceneComponents.Tutorial")

local Display = GlobalCore.Display
local Graphics = GlobalCore.Graphics
local SceneManager = GlobalCore.SceneManager
local Transition = GlobalCore.Transition
local Input = Engine.Input
local ManagerFunctions = GlobalFunctions.Manager

---@class Source.Scenes.SceneMap.SceneMap: Source.Gameplay.GameplayScene
local Scene = {}

function Scene:setInst(inst)
    self._gameOverRequest = nil
    self.inst = inst
end

function Scene:onCreate()
    self._gameplayRequestsActive = true
    self._gameOverRequest = nil
    self._mapClickMoveBlockedUntilLateTick = false
    self._mapInputBlockFrames = 0
    local uiManager = assert(self:getUIManager(), "Scene map UI manager is unavailable")
    uiManager:setFocusNavigationEnabled(true)
    self.player = self.inst:getPlayer()
    self._environment = Environment.new()
    self._maps = MapSession.new(self, self._environment)
    self._dialogue = Dialogue.new(self)
    self._windows = Windows.new(self)
    self._tutorials = TutorialController.new(self)
    ---@type Source.Scenes.SceneMap.SceneMap[]
    local sceneRef = setmetatable({ self }, { __mode = "v" })
    self._playerHUD = PlayerAttrHUD.new(self.player, function ()
        local scene = sceneRef[1]
        if scene ~= nil then
            scene:openMenu()
        end
    end)
    self._regionTitleUI = RegionTitleController.new(Display.getGameSize())
    self._regionTitleUI:prepare()
    self._playerHUD:mount(uiManager)
    self._localeChangedToken = Engine.subscribe(EventKey.LocaleChanged, function ()
        local scene = sceneRef[1]
        if scene ~= nil then
            scene:refreshLocale()
        end
    end)
    local startMap = self.inst:getCurrentMapPath() or GameSystem.GetStartMap()
    self:gotoMapAndPos(startMap, nil, true)
    LiveDebug.BindScene(self)
end

function Scene:onEnter()
    self._environment:enter(self:getGameMap(), self.player)
    Transition.setTransition()
end

function Scene:_cancelInteractions()
    self:cancelTimers()
    self._gameOverRequest = nil
    self._dialogue:cancel()
    self._windows:cancelAsyncOperations()
    self._maps:cancelActorOperations()
    self._tutorials:cancel()
end

function Scene:_cancelPendingRequests()
    self._windows:cancelPendingRequests()
    self._maps:cancelPendingTransfers()
end

function Scene:onQuit()
    self:_cancelPendingRequests()
    self:_cancelInteractions()
    ManagerFunctions.stopVoice()
    self._environment:quit()
end

function Scene:onDestroy()
    self:_cancelPendingRequests()
    self:_cancelInteractions()
    self._gameplayRequestsActive = false
    LiveDebug.UnbindScene(self)
    if self._localeChangedToken ~= nil then
        Engine.unsubscribe(self._localeChangedToken)
        self._localeChangedToken = nil
    end
    ManagerFunctions.stopVoice()
    self._tutorials:dispose()
    self._maps:dispose()
    self._dialogue:dispose()
    self._windows:dispose()
    self._environment:dispose()
    self._playerHUD:dispose()
    self._regionTitleUI:dispose()
end

function Scene:refreshLocale()
    self._tutorials:refreshLocale()
    self._dialogue:refreshLocale()
    self._windows:refreshLocale()
end

function Scene:onFixedTick(fixedDelta)
    if not self._maps:isTransferInProgress() then
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
    self._tutorials:update()
    self._environment:updateAudio(deltaTime)
    self._dialogue:update()
    if self._maps:isTransferInProgress() then
        return super(Scene, self).onTick(deltaTime)
    end
    self._mapClickMoveBlockedUntilLateTick = self:_isMapClickMoveBlocked()
    local gameMap = self:getGameMap()
    gameMap:onTick(deltaTime)
    gameMap:updateAutoTileAnimation(deltaTime)
    self._environment:updateWorld(self:getGameMap(), self.player, deltaTime)
    self._regionTitleUI:update(deltaTime)
    if self._windows:canOpenMenu() and Scene.IsMenuOpenTriggered() then
        self:openMenu()
    end
    return super(Scene, self).onTick(deltaTime)
end

function Scene:onLateTick(deltaTime)
    if self._maps:isTransferInProgress() then
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
    self:_cancelInteractions()
    Logging.info("Loading map: %s", mapPath)
    local startTime = perfCounter()
    local mapFile, mapData = self._maps:replaceMap(mapPath, initialPosition)
    self._environment:applyMap(self:getGameMap(), mapData)
    local region = self._maps:updateRegion()
    if region ~= nil then
        Scene.ShowRegionTitle(region)
    end
    Engine.publish(EventKey.PlayerChanged, { owner = self.player, kind = PlayerChangeKind.Map })
    Logging.info("Loaded map %s in %.3fs", mapFile, perfCounter() - startTime)
    return mapFile
end

function Scene:_drawSceneAnims()
    local animSnapshot = self:getAnims()
    if not bool(animSnapshot) then
        return
    end
    local gameMap = self:getGameMap()
    Graphics.setWindowMapView(gameMap:getMapViewRect())
    for _, anim in ipairs(animSnapshot) do
        local worldPosition = anim:getPosition()
        local drawPosition = gameMap:worldToMapViewPosition(worldPosition)
        anim:setPosition(drawPosition)
        Graphics.draw(anim)
        anim:setPosition(worldPosition)
    end
    Graphics.setWindowDefaultView()
end

function Scene:_drawCommonTipOverlay()
    super(Scene, self)._drawCommonTipOverlay()
    if self._regionTitleUI:getVisible() then
        self._regionTitleUI:draw()
    end
    self._tutorials:draw()
end

function Scene:_renderHandle(deltaTime)
    self._tutorials:updateUI(deltaTime)
    self:getGameMap():show()
    super(Scene, self)._renderHandle(deltaTime)
    self._maps:processPendingTransfers()
    self._windows:afterRender()
end

function Scene.IsMenuOpenTriggered()
    return Input.isMouseButtonTriggered(sf.Mouse.Button.Right, true)
        or Input.isAnyJoystickButtonTriggered(Engine.JoystickButton.getMenu(), true)
end

function Scene.ConsumeMapClickMoveInput()
    Input.isMouseButtonTriggered(sf.Mouse.Button.Left, true)
    Input.isTouchTap(true)
    Input.isTouchTriggered(true)
end

function Scene:_isMapClickMoveBlocked()
    return self:isTutorialBlocking() or self:isDialogueBlocking()
        or self._windows:isBlocking() or self._mapInputBlockFrames > 0
end

function Scene:blockMapInput(frames)
    self._mapInputBlockFrames = math.max(self._mapInputBlockFrames, frames or 1)
end

function Scene:isTutorialBlocking()
    return self._tutorials:isBlocking()
end

function Scene:isDialogueBlocking()
    return self._dialogue:isBlocking()
end

function Scene:isGameplayRequestActive()
    return self._gameplayRequestsActive and SceneManager.getScene() == self
end

function Scene:getGameInstance()
    return self.inst
end

function Scene:getPlayer()
    return self.player
end

function Scene:applyLoadedGame(inst)
    self:_cancelPendingRequests()
    self:_cancelInteractions()
    self.inst = inst
    self.player = inst:getPlayer()
    self._windows:rebindPlayer()
    self._playerHUD:setPlayer(self.player)
    self._maps:invalidateLoadedMap()
    self:gotoMapAndPos(inst:getCurrentMapPath() or GameSystem.GetStartMap(), self.player:getMapPosition())
end

function Scene:applyPrimaryPlayer()
    self:_cancelPendingRequests()
    self:_cancelInteractions()
    self.player = self.inst:getPlayer()
    self._windows:rebindPlayer()
    self._playerHUD:setPlayer(self.player)
    self._maps:invalidateLoadedMap()
    local mapPath = assert(self.inst:getCurrentMapPath(), "Primary player has no stored map")
    self:gotoMapAndPos(mapPath, self.player:getMapPosition())
end

function Scene:requestGameOver(player, delay)
    assert(math.isFinite(delay) and delay >= 0, "Game over delay must be finite and non-negative")
    local gameMap = self._maps:peekGameMap()
    if not self:isGameplayRequestActive() or gameMap == nil or player ~= self.player or gameMap:getPlayer() ~= player
        or player:isDestroyed() or player:getMap() ~= gameMap or self._gameOverRequest ~= nil then
        return
    end
    local request = { player = player, gameMap = gameMap }
    self._gameOverRequest = request
    local function finishGameOver()
        if not self:isGameplayRequestActive() or self._gameOverRequest ~= request or self.player ~= request.player
            or self._maps:peekGameMap() ~= request.gameMap or request.gameMap:getPlayer() ~= request.player
            or request.player:isDestroyed() or request.player:getMap() ~= request.gameMap then
            return
        end
        local SceneGameOver = require("Source.Scenes.SceneGameOver")

        self._gameplayRequestsActive = false
        self._gameOverRequest = nil
        SceneManager.setScene(SceneGameOver.new())
    end
    if delay == 0 then
        finishGameOver()
    else
        self:addTimer(delay, finishGameOver)
    end
end

function Scene:showMessage(name, message, refActor, localeArgs)
    return self:showMessageWithContext(
        name, message, refActor, localeArgs, Dialogue.CaptureLocalVars(Scene.showMessage)
    )
end

function Scene:showMessageWithContext(name, message, refActor, localeArgs, localVars)
    return self._dialogue:showMessage(name, message, refActor, localeArgs, localVars)
end

function Scene:showVoiceMessage(name, message, voiceFileName, refActor, minDistance, spatial)
    return self:showVoiceMessageWithContext(
        name, message, voiceFileName, refActor, minDistance, spatial, Dialogue.CaptureLocalVars(Scene.showVoiceMessage)
    )
end

function Scene:showVoiceMessageWithContext(name, message, voiceFileName, refActor, minDistance, spatial, localVars)
    return self._dialogue:showVoiceMessage(name, message, voiceFileName, refActor, minDistance, spatial, localVars)
end

function Scene:showSelection(name, options, refActor, allowCancel, localeArgs)
    return self:showSelectionWithContext(
        name, options, refActor, allowCancel, localeArgs, Dialogue.CaptureLocalVars(Scene.showSelection)
    )
end

function Scene:showSelectionWithContext(name, options, refActor, allowCancel, localeArgs, localVars)
    return self._dialogue:showSelection(name, options, refActor, allowCancel, localeArgs, localVars)
end

function Scene:getGameMap()
    return self._maps:getGameMap()
end

function Scene:requestFloorStep(teleporter, step)
    return self._maps:requestFloorStep(teleporter, step)
end

function Scene:requestMapTransfer(teleporter, mapPath, position, record)
    return self._maps:requestMapTransfer(teleporter, mapPath, position, record)
end

function Scene:requestTeleporterTransfer(
    targetMap, targetPosition, moveEnabled, findNearest, record, targetStairDirection
)
    return self._maps:requestTeleporterTransfer(
        targetMap, targetPosition, moveEnabled, findNearest, record, targetStairDirection
    )
end

function Scene:gotoMapAndPos(mapPath, pos, blockTransition)
    return self._maps:gotoMapAndPos(mapPath, pos, blockTransition)
end

function Scene:tryCenterSymmetricTeleport()
    return self._maps:tryCenterSymmetricTeleport()
end

function Scene:tryAdjacentFloorSamePos(step)
    return self._maps:tryAdjacentFloorSamePos(step)
end

function Scene:resolveRegionMapPath(mapKey)
    return self._maps:resolveRegionMapPath(mapKey)
end

function Scene:buildFloorMapPreview(mapKey, telepoint, previewSize, previewScale, showTelepointMarker)
    return self._maps:buildFloorMapPreview(mapKey, telepoint, previewSize, previewScale, showTelepointMarker)
end

function Scene:clearFloorMapPreviewCache()
    return self._maps:clearFloorMapPreviewCache()
end

function Scene:recordAddedActor(actor)
    return self._maps:recordAddedActor(actor)
end

function Scene:recordActorPosition(actor, position)
    return self._maps:recordActorPosition(actor, position)
end

function Scene:recordDestroyedActor(actor)
    return self._maps:recordDestroyedActor(actor)
end

function Scene:recordDestroyedActorTag(actorTag)
    return self._maps:recordDestroyedActorTag(actorTag)
end

function Scene:recordTerrainDestructions(layerName, positions)
    return self._maps:recordTerrainDestructions(layerName, positions)
end

function Scene:showEnemyBook()
    return self._windows:showEnemyBook()
end

function Scene:showFloorTeleporter()
    return self._windows:showFloorTeleporter()
end

function Scene:showShopRecords()
    return self._windows:showShopRecords()
end

function Scene:openMenu()
    return self._windows:openMenu()
end

function Scene:openSaveUI()
    return self._windows:openSaveUI()
end

function Scene:openLoadUI()
    return self._windows:openLoadUI()
end

function Scene:openItemUI()
    return self._windows:openItemUI()
end

function Scene:openEquipUI()
    return self._windows:openEquipUI()
end

function Scene:quickSave()
    return self._windows:quickSave()
end

function Scene:quickLoad()
    return self._windows:quickLoad()
end

function Scene:openPlayerName()
    return self._windows:openPlayerName()
end

function Scene:openShop(buyItemIDs, canSell, actor)
    return self._windows:openShop(buyItemIDs, canSell, actor)
end

function Scene:openAttrShop(actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName)
    return self._windows:openAttrShop(actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName)
end

function Scene:playBgm(bgm, bgmFilter)
    return self._environment:playBgm(bgm, bgmFilter)
end

function Scene:setBgmFilter(attr, value)
    return self._environment:setBgmFilter(attr, value)
end

function Scene:setBgsFilter(attr, value)
    return self._environment:setBgsFilter(attr, value)
end

function Scene:getFloorMapPreviewRevision(mapKey)
    return self._maps:getFloorMapPreviewState(mapKey)
end

function Scene.FindRegionForMap(mapFile)
    return MapSession.FindRegionForMap(mapFile)
end

function Scene.NormaliseRegionMapName(mapPath)
    return MapSession.NormaliseRegionMapName(mapPath)
end

function Scene.ShowRegionTitle(region)
    RegionTitleController.Publish({
        region = region
    })
end

return class(Scene, GameplayScene)
