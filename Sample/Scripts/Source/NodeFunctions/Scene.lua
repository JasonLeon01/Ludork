local GlobalCore = require("GlobalCore")
local Context = require("Source.NodeFunctions.Context")

local AudioManager = GlobalCore.AudioManager
local System = GlobalCore.System

local Scene = {}

---@return GlobalCore.Camera | nil
local function getSceneCamera()
    local gameMap = Context.RequireSceneMap():getGameMap()
    if gameMap == nil then
        return nil
    end
    return gameMap:getCamera()
end

---@param scene       Source.Scenes.SceneMap.SceneMap
---@param refActorTag string
---@return Engine.Actor | nil
local function getActorByTag(scene, refActorTag)
    if not bool(refActorTag) then
        return nil
    end
    return scene:getGameMap():getActorByTag(refActorTag)
end

---@param condition fun(): boolean
---@param voice     sf.Sound | nil
---@return fun(): boolean
local function stopVoiceAfterDialogue(condition, voice)
    return function ()
        if not condition() then
            return false
        end
        if voice ~= nil then
            voice:stop()
        end
        return true
    end
end

---@param camera GlobalCore.Camera
---@param actor  Engine.Actor
local function snapCameraToActor(camera, actor)
    local viewSize = camera:getViewSize()
    if viewSize == nil then
        return
    end
    camera:setViewPosition(actor:getPosition() - viewSize / 2)
    camera:fixViewPosition()
end

function Scene.GotoMap(mapPath, blockTransition, position)
    mapPath = mapPath == nil and "" or mapPath
    if blockTransition == nil then
        blockTransition = false
    end
    Context.RequireSceneMap():gotoMapAndPos(mapPath, position, blockTransition)
end

function Scene.GameOver()
    ---@type { new: fun(): GlobalCore.SceneBase }
    local GameOverScene = require("Source.Scenes.SceneGameOver")

    System.setScene(GameOverScene.new())
end

function Scene.AddTimer(interval, blocking)
    if blocking == nil then
        blocking = false
    end
    local scene = System.getScene()
    if scene ~= nil then
        return scene:addTimer(interval, function ()
        end, blocking
        )
    end
    return function ()
        return true
    end
end

function Scene.ShowMessageByTag(name, message, refActorTag)
    refActorTag = refActorTag == nil and "" or refActorTag
    local scene = Context.RequireSceneMap()
    return scene:showMessage(name, message, getActorByTag(scene, refActorTag))
end

function Scene.ShowMessage(name, message, actor)
    return Context.RequireSceneMap():showMessage(name, message, actor)
end

function Scene.ShowVoiceMessageByTag(name, message, voiceFileName, refActorTag)
    refActorTag = refActorTag == nil and "" or refActorTag
    local scene = Context.RequireSceneMap()
    local voice = AudioManager.playVoice(voiceFileName)
    local dialogueFinished = scene:showMessage(name, message, getActorByTag(scene, refActorTag))
    return stopVoiceAfterDialogue(dialogueFinished, voice)
end

function Scene.ShowVoiceMessage(name, message, voiceFileName, refActor, minDistance)
    minDistance = minDistance == nil and 64.0 or minDistance
    local scene = Context.RequireSceneMap()
    local voice = AudioManager.playVoice(voiceFileName, nil, refActor, minDistance)
    local dialogueFinished = scene:showMessage(name, message, refActor)
    return stopVoiceAfterDialogue(dialogueFinished, voice)
end

function Scene.ShowSelection(name, options, refActorTag, allowCancel)
    name = name == nil and "" or name
    options = options or {}
    refActorTag = refActorTag == nil and "" or refActorTag
    if allowCancel == nil then
        allowCancel = true
    end
    local scene = Context.RequireSceneMap()
    return scene:showSelection(name, options, getActorByTag(scene, refActorTag), allowCancel)
end

function Scene.ShowRefSelection(name, options, refActor, allowCancel)
    name = name == nil and "" or name
    options = options or {}
    if allowCancel == nil then
        allowCancel = true
    end
    return Context.RequireSceneMap():showSelection(name, options, refActor, allowCancel)
end

function Scene.LockCamera()
    local gameMap = Context.RequireSceneMap():getGameMap()
    if gameMap ~= nil then
        local camera = getSceneCamera()
        local player = gameMap:getPlayer()
        if camera ~= nil and player ~= nil then
            camera:setParent(player)
            snapCameraToActor(camera, player)
        end
    end
end

function Scene.UnlockCamera()
    local camera = getSceneCamera()
    if camera ~= nil then
        camera:setParent(nil)
    end
end

function Scene.AttachCamera(actor)
    local camera = getSceneCamera()
    if camera == nil then
        return
    end
    camera:setParent(actor)
    if actor ~= nil then
        snapCameraToActor(camera, actor)
    end
end

function Scene.MoveCamera(delta)
    local camera = getSceneCamera()
    if camera == nil then
        return
    end
    camera:moveView(delta)
    camera:fixViewPosition()
end

function Scene.RecordTelepoint(mapPath, x, y)
    mapPath = mapPath == nil and "" or mapPath
    x = x == nil and 0 or x
    y = y == nil and 0 or y
    Context.RequireGameInstance():recordTelepoint(mapPath, sf.Vector2u.new(x, y))
end

function Scene.CreateActorFromBPPath(bpPath, layerName, position, tag, emitCreateEvent)
    bpPath = bpPath == nil and "" or bpPath
    layerName = layerName == nil and "default" or layerName
    tag = tag == nil and "" or tag
    emitCreateEvent = emitCreateEvent == nil and true or emitCreateEvent
    local gameMap = Context.RequireSceneMap():getGameMap()
    if gameMap == nil then
        return nil
    end
    local Data = require("Source.Data")

    local actor = Data.GenActorFromClassPath(bpPath, tag)
    if actor == nil then
        return nil
    end
    if position ~= nil then
        actor:setMapPosition(position)
    end
    gameMap:spawnActor(actor, layerName, emitCreateEvent)
    return actor
end

function Scene.CreateActorFromBPPathWithDefaults(bpPath, defaults, layerName, position, tag, emitCreateEvent)
    bpPath = bpPath == nil and "" or bpPath
    defaults = bool(defaults) and copy(defaults) or nil
    layerName = layerName == nil and "default" or layerName
    tag = tag == nil and "" or tag
    emitCreateEvent = emitCreateEvent == nil and true or emitCreateEvent
    local gameMap = Context.RequireSceneMap():getGameMap()
    if gameMap == nil then
        return nil
    end
    local Data = require("Source.Data")

    local actor = Data.GenActorFromClassPath(bpPath, tag, defaults)
    if actor == nil then
        return nil
    end
    if position ~= nil then
        actor:setMapPosition(position)
    end
    gameMap:spawnActor(actor, layerName, emitCreateEvent)
    return actor
end

function Scene.DestroyTerrain(layerName, position, tileID)
    local scene = Context.RequireSceneMap()
    local changedPositions = scene:getGameMap():setTerrainTiles(layerName, { position }, tileID)
    scene:recordTerrainDestructions(layerName, changedPositions)
end

function Scene.DestroyTerrainList(layerName, positions, tileID)
    positions = positions or {}
    local scene = Context.RequireSceneMap()
    local changedPositions = scene:getGameMap():setTerrainTiles(layerName, positions, tileID)
    scene:recordTerrainDestructions(layerName, changedPositions)
end

function Scene.GetTerrainTile(layerName, position)
    local gameMap = Context.RequireSceneMap():getGameMap()
    if gameMap ~= nil then
        return gameMap:getTerrainTile(layerName, position)
    end
    return nil
end

function Scene.GetTerrainTilePositions(layerName, tileID)
    local gameMap = Context.RequireSceneMap():getGameMap()
    if gameMap ~= nil then
        return gameMap:getTerrainTilePositions(layerName, tileID)
    end
    return {}
end

function Scene.RecordAddedActor(actor)
    Context.RequireSceneMap():recordAddedActor(actor)
end

function Scene.SelfRecordAdded()
    local actor = Context.GetGraphOwner(Scene.SelfRecordAdded)
    ---@cast actor Engine.Actor | nil
    if actor ~= nil then
        Context.RequireSceneMap():recordAddedActor(actor)
    end
end

function Scene.RecordActorPosition(actor)
    Context.RequireSceneMap():recordActorPosition(actor)
end

function Scene.SelfRecordActorPosition()
    local actor = Context.GetGraphOwner(Scene.SelfRecordActorPosition)
    ---@cast actor Engine.Actor | nil
    if actor ~= nil then
        Context.RequireSceneMap():recordActorPosition(actor)
    end
end

function Scene.RecordDestroyedActor(actor)
    Context.RequireSceneMap():recordDestroyedActor(actor)
end

function Scene.SelfRecordDestroyed()
    local actor = Context.GetGraphOwner(Scene.SelfRecordDestroyed)
    ---@cast actor Engine.Actor | nil
    if actor ~= nil then
        Context.RequireSceneMap():recordDestroyedActor(actor)
    end
end

function Scene.RecordAndDestroyActor(actor)
    Context.RequireSceneMap():recordDestroyedActor(actor)
    actor:destroy()
end

function Scene.SelfRecordAndDestroy()
    local actor = Context.GetGraphOwner(Scene.SelfRecordAndDestroy)
    ---@cast actor Engine.Actor | nil
    if actor ~= nil then
        Context.RequireSceneMap():recordDestroyedActor(actor)
        actor:destroy()
    end
end

function Scene.OpenShop(items, canSell)
    items = copy(items or {})
    if canSell == nil then
        canSell = true
    end
    return Context.RequireSceneMap():openShop(items, canSell)
end

function Scene.OpenAttrShop(actor, shopName, shopDescription, abilities, price, priceIncrement, moneyName)
    shopName = shopName == nil and "" or shopName
    shopDescription = shopDescription == nil and "" or shopDescription
    abilities = copy(abilities or {})
    price = price == nil and 0 or price
    priceIncrement = priceIncrement == nil and 1 or priceIncrement
    moneyName = moneyName == nil and "GOLD" or moneyName
    local scene = Context.RequireSceneMap()
    local instance = Context.RequireGameInstance()
    local Utils = require("Source.NodeFunctions.Utils")

    local priceRef
    if Utils.IsNodeReference(price) then
        priceRef = price
    elseif Class.isInstance(price, "string") and bool(price) then
        priceRef = Utils.CreateLocalRef(instance:getVariables(), price, 0)
    else
        local priceValue = not bool(price) and 0 or price
        priceRef = Utils.CreateLocalRef({
            price = priceValue
        }, "price", priceValue)
    end
    return scene:openAttrShop(actor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName)
end

function Scene.OpenAttrShopByTag(actorTag, shopName, shopDescription, abilities, price, priceIncrement, moneyName)
    actorTag = actorTag == nil and "" or actorTag
    local scene = Context.RequireSceneMap()
    local actor = bool(actorTag) and scene:getGameMap():getActorByTag(actorTag) or nil
    return Scene.OpenAttrShop(actor, shopName, shopDescription, abilities, price, priceIncrement, moneyName)
end

return Scene
