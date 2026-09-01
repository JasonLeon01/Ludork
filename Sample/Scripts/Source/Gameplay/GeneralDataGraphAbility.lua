local Engine = require("Engine")
local Data = require("Source.Data")
local GameplayAbility = require("Global.Gameplay.GameplayAbility")
local GameplayAbilityResult = require("Global.Gameplay.GameplayAbilityResult")

local BPBase = Engine.BPBase

---@class Source.Gameplay.GeneralDataGraphAbility: Global.Gameplay.GameplayAbility
local GeneralDataGraphAbility = {}

GeneralDataGraphAbility.generalType = ""
GeneralDataGraphAbility.memberID = ""
GeneralDataGraphAbility.graphEvent = ""

function GeneralDataGraphAbility:init(generalType, memberID, graphEvent, triggerTags)
    GameplayAbility.init(self, {
        id = "GeneralData." .. generalType .. "." .. memberID .. "." .. graphEvent,
        triggerTags = triggerTags or {}
    })
    self.generalType = generalType
    self.memberID = memberID
    self.graphEvent = graphEvent
end

function GeneralDataGraphAbility:calculate(_abilitySystem, _eventData)
    local generalData = Data.GetGeneralData(self.generalType)
    local member = assert(generalData.members[self.memberID], "General Data member not found: " .. self.memberID)
    local graphData = member._graph
    if graphData == nil or graphData.nodeGraph[self.graphEvent] == nil or graphData.startNodes[self.graphEvent] == nil then
        return GameplayAbilityResult.Success("NoGraph")
    end
    return GameplayAbilityResult.Success("ExecutableGraph", { graphData = graphData })
end

function GeneralDataGraphAbility:activate(abilitySystem, eventData)
    local result = self:calculate(abilitySystem, eventData)
    if result.code == "NoGraph" then
        return result
    end
    eventData.payload.generalType = self.generalType
    eventData.payload.memberID = self.memberID
    eventData.payload.graphEvent = self.graphEvent
    if self.generalType == "State" then
        for _, activeEffect in ipairs(abilitySystem:getActiveGameplayEffects()) do
            if activeEffect.spec.effect.id == "State." .. self.memberID then
                eventData.payload.activeEffect = activeEffect
                break
            end
        end
    end
    local graph = Data.GenGraphFromData(result.data.graphData, eventData, nil)
    local executed = BPBase.ExecuteGraph(graph, self.graphEvent, { eventData = eventData })
    assert(executed, "General Data graph failed to execute: " .. self.id)
    return GameplayAbilityResult.Success("GraphExecuted")
end

return class(GeneralDataGraphAbility, GameplayAbility)
