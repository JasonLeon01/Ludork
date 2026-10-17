local Engine = require("Engine")
local GeneralDataKey = require("Enums.GeneralDataKey")
local GlobalData = require("Global.Data")
local Validation = require("Global.Data.Validation")
local GeneralDataTypes = require("Source.Configs.GeneralDataTypes")
local ConditionalActor = require("Source.MapActors.ConditionalActor")
local ChildActorComponent = require("Source.Components.ChildActorComponent")
local SourceSystem = require("Source.System")

local requireNamedValue = Validation.RequireNamedValue

---@class (partial) Source.Data
local Data = {}

function Data.InitializeRuntime()
    assert(Class.isSubclass(ConditionalActor, Engine.Actor), "ConditionalActor must derive from Engine.Actor")
    assert(
        Class.isSubclass(ChildActorComponent, Engine.Component), "ChildActorComponent must derive from Engine.Component"
    )
    ---@diagnostic disable-next-line: cast-type-mismatch, imported declarations describe instance fields rather than the validated class object
    ---@cast ConditionalActor Class.ClassType<Engine.Actor>
    ---@diagnostic disable-next-line: cast-type-mismatch, imported declarations describe instance fields rather than the validated class object
    ---@cast ChildActorComponent Class.ClassType<Engine.Component>
    GlobalData.InitializeRuntime({
        configResolver = SourceSystem.GetConfigValue,
        nodeModuleCandidates = function (prefix)
            return { "Source." .. prefix, "Global." .. prefix }
        end,
        conditionalActorBase = ConditionalActor,
        childActorComponentBase = ChildActorComponent
    })
end

---@param name string
---@return table<string, table<string, Global.Data.GeneralValue>>
local function generalMembers(name)
    local data = GlobalData.GetGeneralData(name)
    local members = requireNamedValue(data, "members", "General data members not found: " .. tostring(name))
    ---@cast members table<string, table<string, Global.Data.GeneralValue>>
    return members
end

function Data.CreateGeneralAttributeSet(typeName, memberID)
    local member = requireNamedValue(
        generalMembers(typeName), memberID,
        "General data member not found: " .. tostring(typeName) .. "." .. tostring(memberID)
    )
    return GeneralDataTypes.Create(typeName, memberID, member)
end

function Data.GetGeneralClassData(key)
    return requireNamedValue(
        generalMembers(GeneralDataKey.Class), key, "General class data not found: " .. tostring(key)
    )
end

function Data.GetGeneralEnemyData(key)
    return requireNamedValue(
        generalMembers(GeneralDataKey.Enemy), key, "General enemy data not found: " .. tostring(key)
    )
end

function Data.GetGeneralPlayerData(key)
    return requireNamedValue(
        generalMembers(GeneralDataKey.Player), key, "General player data not found: " .. tostring(key)
    )
end

function Data.GetAllGeneralEquipData()
    return generalMembers(GeneralDataKey.Equip)
end

function Data.GetGeneralEquipData(key)
    return requireNamedValue(
        generalMembers(GeneralDataKey.Equip), key, "General equip data not found: " .. tostring(key)
    )
end

function Data.GetAllGeneralItemData()
    return generalMembers(GeneralDataKey.Item)
end

function Data.GetGeneralItemData(key)
    return requireNamedValue(generalMembers(GeneralDataKey.Item), key, "General item data not found: " .. tostring(key))
end

function Data.GetGeneralSpecialData(key)
    return requireNamedValue(
        generalMembers(GeneralDataKey.Special), key, "General special data not found: " .. tostring(key)
    )
end

function Data.GetGeneralStateData(key)
    return requireNamedValue(
        generalMembers(GeneralDataKey.State), key, "General state data not found: " .. tostring(key)
    )
end

return Data
