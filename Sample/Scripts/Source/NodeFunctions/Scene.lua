local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Context = require("Source.NodeFunctions.Context")

local ManagerFunctions = GlobalFunctions.Manager
local System = GlobalCore.System

local Scene = {}

---@return GlobalCore.Camera | nil
local function getSceneCamera()
    local gameMap = Context.requireSceneMap():getGameMap()
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
    blockTransition = bool(blockTransition)
    Context.requireSceneMap():gotoMapAndPos(mapPath, position, blockTransition)
end

function Scene.GameOver()
    ---@type { new: fun(): GlobalCore.SceneBase }
    local GameOverScene = require("Source.Scenes.SceneGameOver")

    System.setScene(GameOverScene.new())
end

function Scene.AddTimer(interval, blocking)
    blocking = bool(blocking)
    local scene = System.getScene()
    if scene ~= nil then
        local timerInterval = tonumber(interval)
        ---@cast timerInterval number
        return scene:addTimer(timerInterval, function ()
        end, blocking
        )
    end
    return function ()
        return true
    end
end

function Scene.ShowMessageByTag(name, message, refActorTag)
    refActorTag = refActorTag == nil and "" or refActorTag
    local scene = Context.requireSceneMap()
    return scene:showMessage(name, message, getActorByTag(scene, refActorTag))
end

function Scene.ShowMessage(name, message, actor)
    return Context.requireSceneMap():showMessage(name, message, actor)
end

function Scene.ShowVoiceMessageByTag(name, message, voiceFileName, refActorTag)
    refActorTag = refActorTag == nil and "" or refActorTag
    local scene = Context.requireSceneMap()
    ManagerFunctions.playVoice(voiceFileName)
    return scene:showMessage(name, message, getActorByTag(scene, refActorTag))
end

function Scene.ShowVoiceMessage(name, message, voiceFileName, refActor, minDistance)
    minDistance = minDistance == nil and 64.0 or minDistance
    local scene = Context.requireSceneMap()
    ManagerFunctions.playVoice(voiceFileName, nil, refActor, tonumber(minDistance))
    return scene:showMessage(name, message, refActor)
end

function Scene.ShowSelection(name, options, refActorTag, allowCancel)
    name = name == nil and "" or name
    options = options or {}
    refActorTag = refActorTag == nil and "" or refActorTag
    if allowCancel == nil then
        allowCancel = true
    else
        allowCancel = bool(allowCancel)
    end
    local scene = Context.requireSceneMap()
    return scene:showSelection(name, options, getActorByTag(scene, refActorTag), allowCancel)
end

function Scene.ShowRefSelection(name, options, refActor, allowCancel)
    name = name == nil and "" or name
    options = options or {}
    if allowCancel == nil then
        allowCancel = true
    else
        allowCancel = bool(allowCancel)
    end
    return Context.requireSceneMap():showSelection(name, options, refActor, allowCancel)
end

function Scene.LockCamera()
    local gameMap = Context.requireSceneMap():getGameMap()
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
    Context.requireGameInstance():recordTelepoint(mapPath, sf.Vector2u.new((math.modf(x)), (math.modf(y))))
end

function Scene.CreateActorFromBPPath(bpPath, layerName, position, tag, emitCreateEvent)
    bpPath = bpPath == nil and "" or bpPath
    layerName = layerName == nil and "default" or layerName
    tag = tag == nil and "" or tag
    emitCreateEvent = emitCreateEvent == nil and true or emitCreateEvent
    local gameMap = Context.requireSceneMap():getGameMap()
    if gameMap == nil then
        return nil
    end
    local Data = require("Source.Data")

    local actor = Data.genActorFromClassPath(bpPath, tag)
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
    local gameMap = Context.requireSceneMap():getGameMap()
    if gameMap == nil then
        return nil
    end
    local Data = require("Source.Data")

    local actor = Data.genActorFromClassPath(bpPath, tag, defaults)
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
    local gameMap = Context.requireSceneMap():getGameMap()
    if gameMap ~= nil then
        gameMap:destroyTerrain(layerName, position, tileID)
    end
end

function Scene.DestroyTerrainList(layerName, positions, tileID)
    positions = positions or {}
    local gameMap = Context.requireSceneMap():getGameMap()
    if gameMap ~= nil then
        gameMap:destroyTerrainList(layerName, positions, tileID)
    end
end

function Scene.GetTerrainTile(layerName, position)
    local gameMap = Context.requireSceneMap():getGameMap()
    if gameMap ~= nil then
        return gameMap:getTerrainTile(layerName, position)
    end
    return nil
end

function Scene.GetTerrainTilePositions(layerName, tileID)
    local gameMap = Context.requireSceneMap():getGameMap()
    if gameMap ~= nil then
        return gameMap:getTerrainTilePositions(layerName, tileID)
    end
    return {}
end

function Scene.RecordAddedActor(actor)
    Context.requireSceneMap():recordAddedActor(actor)
end

function Scene.SelfRecordAdded()
    local actor = Context._getGraphOwner(Scene.SelfRecordAdded)
    ---@cast actor Engine.Actor | nil
    if actor ~= nil then
        Context.requireSceneMap():recordAddedActor(actor)
    end
end

function Scene.RecordActorPosition(actor)
    Context.requireSceneMap():recordActorPosition(actor)
end

function Scene.SelfRecordActorPosition()
    local actor = Context._getGraphOwner(Scene.SelfRecordActorPosition)
    ---@cast actor Engine.Actor | nil
    if actor ~= nil then
        Context.requireSceneMap():recordActorPosition(actor)
    end
end

function Scene.RecordDestroyedActor(actor)
    Context.requireSceneMap():recordDestroyedActor(actor)
end

function Scene.SelfRecordDestroyed()
    local actor = Context._getGraphOwner(Scene.SelfRecordDestroyed)
    ---@cast actor Engine.Actor | nil
    if actor ~= nil then
        Context.requireSceneMap():recordDestroyedActor(actor)
    end
end

function Scene.RecordAndDestroyActor(actor)
    Context.requireSceneMap():recordDestroyedActor(actor)
    actor:destroy()
end

function Scene.SelfRecordAndDestroy()
    local actor = Context._getGraphOwner(Scene.SelfRecordAndDestroy)
    ---@cast actor Engine.Actor | nil
    if actor ~= nil then
        Context.requireSceneMap():recordDestroyedActor(actor)
        actor:destroy()
    end
end

function Scene.OpenShop(items, canSell)
    items = copy(items or {})
    if canSell == nil then
        canSell = true
    else
        canSell = bool(canSell)
    end
    return Context.requireSceneMap():openShop(items, canSell)
end

function Scene.OpenAttrShop(actor, shopName, shopDescription, abilities, price, priceIncrement, moneyName)
    shopName = shopName == nil and "" or shopName
    shopDescription = shopDescription == nil and "" or shopDescription
    abilities = copy(abilities or {})
    price = price == nil and 0 or price
    priceIncrement = priceIncrement == nil and 1 or priceIncrement
    moneyName = moneyName == nil and "GOLD" or moneyName
    local scene = Context.requireSceneMap()
    local instance = Context.requireGameInstance()
    local Utils = require("Source.NodeFunctions.Utils")

    local priceRef
    if Utils.IsNodeReference(price) then
        priceRef = price
    elseif type(price) == "string" and bool(price) then
        priceRef = Utils._localRef(instance:getVariables(), price, 0)
    else
        local priceValue = not bool(price) and 0 or price
        priceRef = Utils._localRef({
            price = priceValue
        }, "price", priceValue)
    end
    return scene:openAttrShop(
        actor, shopName, shopDescription, abilities, priceRef, (math.modf(priceIncrement)), moneyName
    )
end

function Scene.OpenAttrShopByTag(actorTag, shopName, shopDescription, abilities, price, priceIncrement, moneyName)
    actorTag = actorTag == nil and "" or actorTag
    local scene = Context.requireSceneMap()
    local actor = bool(actorTag) and scene:getGameMap():getActorByTag(actorTag) or nil
    return Scene.OpenAttrShop(actor, shopName, shopDescription, abilities, price, priceIncrement, moneyName)
end

return Scene
