local Engine = require("Engine")
local GlobalCore = require("GlobalCore")

local System = GlobalCore.System

local EVENT_KEY = "ConditionalDoorAutoOpen"

---@class (partial) Mixins.Doors.ConditionDoor
local ConditionDoor = {}

ConditionDoor.openConditionName = ""
ConditionDoor.openConditionVal = 0

---@return Source.GameInstance.GameInstance
local function getGameInstance()
    local scene = System.getScene()
    assert(scene ~= nil, "Condition door requires an active scene")
    ---@cast scene Source.Scenes.SceneMap.SceneMap
    local inst = scene.inst
    assert(inst ~= nil, "Condition door requires an active game instance")
    return inst
end

function ConditionDoor:onCreate()
    self._conditionDoorPending = true
    if not bool(self.openConditionName) then
        return
    end
    local inst = getGameInstance()
    local variables = inst:getVariables()
    local value = variables[self.openConditionName]
    if value == nil then
        value = 0
    end
    inst:setVariable(self.openConditionName, value)
end

function ConditionDoor:onDestroy()
    Engine.unsubscribeObjectHandler(EVENT_KEY, self)
end

function ConditionDoor:onTick(deltaTime)
    super().onTick(deltaTime)
    if not self._conditionDoorPending or not bool(self.openConditionName) then
        return
    end
    local value = getGameInstance():getVariable(self.openConditionName)
    if value == nil or value < self.openConditionVal then
        return
    end
    self._conditionDoorPending = false
    self:openDoor()
end

return ConditionDoor
