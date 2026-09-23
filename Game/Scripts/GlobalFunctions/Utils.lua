local Engine
local function loadEngine()
    if Engine == nil then
        Engine = require("Engine")
    end
    return Engine
end

local GlobalCore
local function loadGlobalCore()
    if GlobalCore == nil then
        GlobalCore = require("GlobalCore")
    end
    return GlobalCore
end

local GlobalFunctions
local function loadGlobalFunctions()
    if GlobalFunctions == nil then
        GlobalFunctions = require("GlobalFunctions")
    end
    return GlobalFunctions
end

local Logging
local function loadLogging()
    if Logging == nil then
        Logging = require("Global.Utils.Logging")
    end
    return Logging
end

local Data
local function loadData()
    if Data == nil then
        Data = require("Source.Data")
    end
    return Data
end

local Context
local function loadContext()
    if Context == nil then
        Context = require("GlobalFunctions.Context")
    end
    return Context
end

local NumberFormat
local function loadNumberFormat()
    if NumberFormat == nil then
        NumberFormat = require("Source.Utils.NumberFormat")
    end
    return NumberFormat
end

local Utils = {}
local AttrRef = {}
local LocalRef = {}

function Utils.IsNodeReference(value)
    return Class.isInstance(value, AttrRef) or Class.isInstance(value, LocalRef)
end

---@param animationData Engine.AnimationData
---@return number
local function getAnimationDataVisualDuration(animationData)
    local getAnimationVisualDuration = loadEngine().getAnimationVisualDuration
    ---@cast getAnimationVisualDuration function
    return getAnimationVisualDuration(animationData)
end

---@generic T
---@param value T | GlobalFunctions.Utils.NodeReference<T>
---@return T
local function referenceValue(value)
    if Utils.IsNodeReference(value) then
        ---@cast value GlobalFunctions.Utils.NodeReference<T>
        return value:get()
    end
    return value
end

---@param left  any
---@param right any
---@return any
local function referenceAdd(left, right)
    return referenceValue(left) + referenceValue(right)
end

---@param left  any
---@param right any
---@return any
local function referenceSub(left, right)
    return referenceValue(left) - referenceValue(right)
end

---@param left  any
---@param right any
---@return any
local function referenceMul(left, right)
    return referenceValue(left) * referenceValue(right)
end

---@param left  any
---@param right any
---@return any
local function referenceDiv(left, right)
    return referenceValue(left) / referenceValue(right)
end

---@param left  any
---@param right any
---@return any
local function referenceMod(left, right)
    return referenceValue(left) % referenceValue(right)
end

---@param left  any
---@param right any
---@return any
local function referencePow(left, right)
    return referenceValue(left) ^ referenceValue(right)
end

---@param left  any
---@param right any
---@return boolean
local function referenceEq(left, right)
    return referenceValue(left) == referenceValue(right)
end

---@param left  any
---@param right any
---@return boolean
local function referenceLt(left, right)
    return referenceValue(left) < referenceValue(right)
end

---@param left  any
---@param right any
---@return boolean
local function referenceLe(left, right)
    return referenceValue(left) <= referenceValue(right)
end

---@generic T
---@param value GlobalFunctions.Utils.NodeReference<T>
---@return string
local function referenceToString(value)
    return tostring(value:get())
end

AttrRef.__add = referenceAdd
AttrRef.__sub = referenceSub
AttrRef.__mul = referenceMul
AttrRef.__div = referenceDiv
AttrRef.__mod = referenceMod
AttrRef.__pow = referencePow
AttrRef.__eq = referenceEq
AttrRef.__lt = referenceLt
AttrRef.__le = referenceLe
AttrRef.__tostring = referenceToString

function AttrRef:init(obj, name)
    self.obj = obj
    self.name = name
end

function AttrRef:get()
    local value = loadGlobalFunctions().Components.getComponentFieldValue(self.obj, self.name)
    if value ~= nil then
        return value
    end
    value = self.obj[self.name]
    if value == nil then
        error("attribute not found: " .. tostring(self.name))
    end
    return value
end

function AttrRef:set(value)
    if not loadGlobalFunctions().Components.setComponentFieldValue(self.obj, self.name, value) then
        self.obj[self.name] = value
    end
    return value
end

local FinalAttrRef = class(AttrRef)

LocalRef.__add = referenceAdd
LocalRef.__sub = referenceSub
LocalRef.__mul = referenceMul
LocalRef.__div = referenceDiv
LocalRef.__mod = referenceMod
LocalRef.__pow = referencePow
LocalRef.__eq = referenceEq
LocalRef.__lt = referenceLt
LocalRef.__le = referenceLe
LocalRef.__tostring = referenceToString

function LocalRef:init(loc, name, default)
    self.loc = loc
    self.name = name
    self.default = default
end

function LocalRef:get()
    return self.loc[self.name] == nil and self.default or self.loc[self.name]
end

function LocalRef:set(value)
    self.loc[self.name] = value
    return value
end

local FinalLocalRef = class(LocalRef)

function Utils.CreateLocalRef(loc, name, default)
    return FinalLocalRef.new(loc, name, default)
end

function Utils.IF(condition)
    return bool(referenceValue(condition)) and 0 or 1
end

function Utils.SetLocalValue(valueName, value)
    loadContext().GetRefLocal(Utils.SetLocalValue)[valueName] = value
end

function Utils.GetLocalValue(valueName, default)
    local value = loadContext().GetRefLocal(Utils.GetLocalValue)[valueName]
    return value == nil and default or value
end

function Utils.GetLocalValueRef(valueName, default)
    return Utils.CreateLocalRef(loadContext().GetRefLocal(Utils.GetLocalValueRef), valueName, default)
end

function Utils.SetGameVariable(valueName, value)
    loadContext().RequireGameInstance():setVariable(valueName, referenceValue(value))
end

function Utils.GetGameVariable(valueName, default)
    local value = loadContext().RequireGameInstance():getVariables()[valueName]
    return value == nil and default or value
end

function Utils.GetGameVariableRef(valueName, default)
    return Utils.CreateLocalRef(loadContext().RequireGameInstance():getVariables(), valueName, default)
end

function Utils.AddPlayerByClass(playerClass, mapPath, position)
    playerClass = playerClass == nil and "" or playerClass
    mapPath = mapPath == nil and "" or mapPath
    loadContext().RequireGameInstance():addPlayerByClass(playerClass, mapPath, position)
end

function Utils.SetPlayerByClass(playerClass)
    playerClass = playerClass == nil and "" or playerClass
    loadContext().RequireGameInstance():setPlayerByClass(playerClass)
    loadContext().RequireSceneMap():applyPrimaryPlayer()
end

function Utils.RemovePlayerByClass(playerClass)
    playerClass = playerClass == nil and "" or playerClass
    loadContext().RequireGameInstance():removePlayerByClass(playerClass)
end

---@param animName string
---@param position sf.Vector2f
---@param rotation number
---@param scale    sf.Vector2f
local function spawnAnim(animName, position, rotation, scale)
    local animData = loadData().GetAnimation(animName)
    if animData == nil then
        error("Animation '" .. tostring(animName) .. "' not found")
    end
    local anim = loadGlobalCore().Animation.new(animData)
    anim:setPosition(position)
    anim:setRotation(sf.degrees(rotation))
    anim:setScale(scale)
    local scene = loadGlobalCore().SceneManager.getScene()
    if scene ~= nil then
        scene:addAnim(anim)
    end
end

---@param actorTag string
---@return Engine.Actor | nil
local function getActorByTag(actorTag)
    if bool(actorTag) then
        local gameMap = loadContext().RequireSceneMap():getGameMap()
        if gameMap ~= nil then
            return gameMap:getActorByTag(actorTag)
        end
    end
    return nil
end

function Utils.AddAnim(animName, position, rotation, scale)
    position = position or sf.Vector2f.new(0.0, 0.0)
    rotation = rotation == nil and 0.0 or rotation
    scale = scale or sf.Vector2f.new(1.0, 1.0)
    spawnAnim(animName, position, rotation, scale)
end

function Utils.AddAnimOn(animName, actorTag, rotation, scale)
    rotation = rotation == nil and 0.0 or rotation
    scale = scale or sf.Vector2f.new(1.0, 1.0)
    local actor = getActorByTag(actorTag)
    if actor == nil then
        error("Actor with tag '" .. tostring(actorTag) .. "' not found")
    end
    spawnAnim(
        animName, actor:getPosition() + sf.Vector2f.new(loadEngine().GetCellSize(), loadEngine().GetCellSize()) * 0.5, rotation,
        scale
    )
end

function Utils.GetAnimLength(animName)
    local animData = loadData().GetAnimation(animName)
    if animData == nil then
        error("Animation '" .. tostring(animName) .. "' not found")
    end
    return assert(tonumber(animData.duration), "Animation duration is missing")
end

function Utils.GetAnimVisualLength(animName)
    local animData = loadData().GetAnimation(animName)
    if animData == nil then
        error("Animation '" .. tostring(animName) .. "' not found")
    end
    return getAnimationDataVisualDuration(animData)
end

function Utils.SUPER(obj, params)
    params = params or {}
    local refLocal = loadContext().GetRefLocal(Utils.SUPER)
    local graphContext = refLocal.__graph__
    local eventName = refLocal.__key__
    if graphContext == nil or not bool(eventName) then
        error("SUPER must be called from a blueprint event graph")
    end
    ---@cast eventName string
    local class = graphContext.parentClass or Class.type(obj)
    return loadEngine().BPBase.ExecuteParentEvent(obj, class, eventName, params, nil, refLocal)
end

function Utils.SELF()
    local parent = loadContext().RequireGraphParent(Utils.SELF)
    ---@cast parent table
    return parent
end

function Utils.GetAttrRef(obj, attrName)
    return FinalAttrRef.new(obj, attrName)
end

function Utils.GetAttr(obj, attrName)
    local value = loadGlobalFunctions().Components.getComponentFieldValue(obj, attrName)
    if value ~= nil then
        return value
    end
    value = obj[attrName]
    if value == nil then
        error("attribute not found: " .. tostring(attrName))
    end
    return value
end

function Utils.SetAttr(obj, attrName, value)
    if not loadGlobalFunctions().Components.setComponentFieldValue(obj, attrName, value) then
        obj[attrName] = value
    end
end

function Utils.GetScene()
    return loadGlobalCore().SceneManager.getScene()
end

function Utils.IsValidValue(value)
    return value ~= nil
end

function Utils.ToShortNumber(value)
    return loadNumberFormat().ToShortNumber(value)
end

function Utils.RunCommonFunction(commonFunctionName)
    commonFunctionName = commonFunctionName == nil and "" or commonFunctionName
    ---@type Engine.Graph | nil
    local callerGraph = loadContext().GetRefLocal(Utils.RunCommonFunction).__graph__
    local commonGraph = loadData().GetCommonFunction(commonFunctionName)
    if callerGraph ~= nil then
        commonGraph.parent = callerGraph.parent
        commonGraph.localGraph = callerGraph.localGraph
    end
    if commonGraph:hasKey("common") then
        return commonGraph:execute("common")
    end
    local keys = table.orderedStringKeys(commonGraph.startNodes or {})
    local startKey = keys[1]
    if startKey ~= nil then
        return commonGraph:execute(startKey)
    end
    error("Common function '" .. commonFunctionName .. "' has no start nodes")
end

function Utils.RegisterEventBus(key, obj, functionName)
    loadEngine().subscribeObjectHandler(key, obj, function (payload)
        loadEngine().BPBase.BlueprintEvent(obj, Class.type(obj), functionName, payload or {})
    end)
end

function Utils.RegisterEventBusEvent(key, obj, eventName)
    loadEngine().subscribeBlueprintEvent(key, obj, eventName)
end

function Utils.UnregisterEventBus(key)
    return loadEngine().unsubscribeEvent(key)
end

function Utils.UnregisterEventBusEvent(key, obj)
    if obj ~= nil then
        return loadEngine().unsubscribeObjectHandler(key, obj)
    end
    return loadEngine().unsubscribeEvent(key)
end

function Utils.TriggerEventBus(key, kwargs)
    loadEngine().post(key, kwargs or {})
end

function Utils.TriggerBlueprintEvent(obj, eventName)
    loadEngine().triggerBlueprintEvent(obj, eventName)
end

function Utils.BackToTitle()
    ---@type { new: fun(): GlobalCore.SceneBase }
    local Title = require("Source.Scenes.SceneTitle")

    loadGlobalCore().SceneManager.setScene(Title.new())
end

function Utils.Print(message)
    message = message == nil and "" or message
    loadLogging().info("%s", tostring(message))
end

function Utils.EXEC(script)
    script = script == nil and "" or script
    local chunk, message = load(script, "=(EXEC)", "t", _G)
    assert(chunk ~= nil, message)
    chunk()
end

function Utils.GetSelfAttr(attrName)
    local obj = loadContext().RequireGraphParent(Utils.GetSelfAttr)
    ---@cast obj table
    return Utils.GetAttr(obj, attrName)
end

function Utils.SetSelfAttr(attrName, value)
    local obj = loadContext().RequireGraphParent(Utils.SetSelfAttr)
    ---@cast obj table
    Utils.SetAttr(obj, attrName, value)
end

function Utils.IfPlayerOverlaps()
    local obj = loadContext().RequireGraphParent(Utils.IfPlayerOverlaps)
    ---@cast obj Engine.Actor
    local gameMap = loadContext().RequireSceneMap():getGameMap()
    if gameMap == nil then
        return false
    end
    local player = gameMap:getPlayer()
    if player == nil then
        return false
    end
    return table.contains(gameMap:getOverlaps(player), obj)
end

function Utils.IfGameVar(varName, op, value)
    varName = varName == nil and "" or varName
    op = op == nil and "==" or op
    value = referenceValue(value)
    local current = loadContext().RequireGameInstance():getVariable(varName)
    if op == "==" then
        return current == value
    end
    if op == "!=" then
        return current ~= value
    end
    if op == "<" then
        return current < value
    end
    if op == "<=" then
        return current <= value
    end
    if op == ">" then
        return current > value
    end
    if op == ">=" then
        return current >= value
    end
    return false
end

return Utils
