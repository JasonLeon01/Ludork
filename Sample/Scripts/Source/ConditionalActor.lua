local Engine = require("Engine")
local GameplayScene = require("Source.Gameplay.GameplayScene")

local Actor = Engine.Actor

---@type table<GameMap, table<Source.ConditionalActor, boolean>>
local monitoredActors = setmetatable({}, { __mode = "k" })

---@class Source.ConditionalActor
local ConditionalActor = {}

ConditionalActor.conditionVariable = ""
ConditionalActor.conditionOperator = "=="
ConditionalActor.conditionValue = 0

function ConditionalActor:_updateConditionVisibility()
    local variables = assert(self._conditionTarget, "ConditionalActor has no variable subscription")
    local current = variables[assert(self._conditionName)]
    assert(current ~= nil, "ConditionalActor variable is missing: " .. self._conditionName)
    local comparison = self.conditionValue
    local isNumber = Class.isInstance(current, "number") and Class.isInstance(comparison, "number")
    local isBoolean = Class.isInstance(current, "boolean") and Class.isInstance(comparison, "boolean")
    local isString = Class.isInstance(current, "string") and Class.isInstance(comparison, "string")
    assert(
        isNumber or isBoolean or isString,
        "ConditionalActor comparison requires matching number, boolean or string values"
    )
    local operator = self.conditionOperator
    ---@cast operator string
    local visible
    if operator == "==" then
        visible = current == comparison
    elseif operator == "~=" then
        visible = current ~= comparison
    else
        assert(isNumber, "ConditionalActor ordering requires numeric values")
        ---@cast current number
        ---@cast comparison number
        if operator == ">" then
            visible = current > comparison
        elseif operator == ">=" then
            visible = current >= comparison
        elseif operator == "<" then
            visible = current < comparison
        elseif operator == "<=" then
            visible = current <= comparison
        else
            error("Unsupported ConditionalActor operator: " .. tostring(operator))
        end
    end
    self:setVisible(visible, false)
end

function ConditionalActor:_registerConditionMonitor()
    self:releaseConditionMonitor()
    assert(Class.isInstance(self.conditionVariable, "string"), "ConditionalActor variable name must be a string")
    if self.conditionVariable == "" then
        return
    end
    local gameMap = assert(self:getMap(), "ConditionalActor requires an owning map")
    ---@cast gameMap GameMap
    local scene = gameMap:getScene()
    assert(Class.isInstance(scene, GameplayScene), "ConditionalActor requires a GameplayScene on its owning map")
    ---@cast scene Source.Gameplay.GameplayScene
    local instance = scene:getGameInstance()
    local mapPath = instance:getCurrentMapPath()
    local mapTag = self:getMapTag()
    assert(bool(mapPath), "ConditionalActor requires a current map identity")
    assert(bool(mapTag), "ConditionalActor requires a map placement tag")
    ---@cast mapPath string
    ---@cast mapTag string
    self._conditionTarget = instance:getVariables()
    self._conditionName = self.conditionVariable
    self._conditionIdentifier = "ConditionalActor:" .. #mapPath .. ":" .. mapPath .. ":" .. mapTag
    self._conditionMap = gameMap
    local succeeded, failure = pcall(function ()
        Class.monitor(self._conditionTarget, self._conditionName, function ()
            self:_updateConditionVisibility()
        end, nil, false, self._conditionIdentifier
        )
        monitoredActors[gameMap] = monitoredActors[gameMap] or {}
        monitoredActors[gameMap][self] = true
        self:_updateConditionVisibility()
    end)
    if not succeeded then
        self:releaseConditionMonitor()
        error(failure, 0)
    end
end

function ConditionalActor:releaseConditionMonitor()
    if self._conditionTarget ~= nil then
        Class.unmonitor(self._conditionTarget, assert(self._conditionName), assert(self._conditionIdentifier))
    end
    if self._conditionMap ~= nil then
        local actors = monitoredActors[self._conditionMap]
        if actors ~= nil then
            actors[self] = nil
            if not bool(actors) then
                monitoredActors[self._conditionMap] = nil
            end
        end
    end
    self._conditionTarget = nil
    self._conditionName = nil
    self._conditionIdentifier = nil
    self._conditionMap = nil
end

function ConditionalActor.ReleaseMapMonitors(gameMap)
    local actors = monitoredActors[gameMap]
    if actors ~= nil then
        for actor in pairs(actors) do
            actor:releaseConditionMonitor()
        end
    end
end

function ConditionalActor:onCreate()
    super(ConditionalActor, self).onCreate()
    self:_registerConditionMonitor()
end

function ConditionalActor:onDestroy()
    self:releaseConditionMonitor()
    super(ConditionalActor, self).onDestroy()
end

function ConditionalActor:onWorldSleep()
    self:releaseConditionMonitor()
    super(ConditionalActor, self).onWorldSleep()
end

function ConditionalActor:onWorldWake(elapsedSeconds)
    super(ConditionalActor, self).onWorldWake(elapsedSeconds)
    self:_registerConditionMonitor()
end

return class(ConditionalActor, Actor)
