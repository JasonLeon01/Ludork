local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local cjson = require("cjson")

---@class MotaFill.Request
---@field operation  "inspect" | "evaluate"
---@field savePath   string
---@field playerKey  string
---@field blueprints string[]

---@class MotaFill.PlayerInfo
---@field key     string
---@field name    string
---@field hp      number
---@field attack  number
---@field defense number

---@class MotaFill.BlueprintInfo
---@field reference         string
---@field category          "unsupported" | "door" | "enemy" | "item" | "teleporter"
---@field doorKeyId?        string
---@field doorKeyCount?     integer
---@field damage?           number
---@field battleResultCode? Enums.BattleResultCode
---@field canWin?           boolean

---@class MotaFill.KeyDoorDefinition: Class.ClassType<any>
---@field needKeyID?    string
---@field needKeyCount? integer

---@class MotaFill.Result
---@field success           boolean
---@field error             string
---@field selectedPlayerKey string
---@field players           MotaFill.PlayerInfo[]
---@field blueprints        MotaFill.BlueprintInfo[]

---@return MotaFill.Result
local function evaluate()
    local requestPath = assert(arg[1], "Request path is required")
    local request = Engine.loadData(requestPath)
    assert(Class.isInstance(request, "table"), "Evaluation request must be an object")
    ---@cast request MotaFill.Request
    ---@diagnostic disable-next-line: unnecessary-assert, the external request operation must be validated before dispatch
    assert(request.operation == "inspect" or request.operation == "evaluate", "Unknown evaluation operation")
    assert(Class.isInstance(request.savePath, "string") and bool(request.savePath), "Save path is required")
    assert(Class.isInstance(request.blueprints, "table"), "Blueprint references must be an array")
    local save = Engine.loadData(request.savePath)
    assert(Class.isInstance(save, "table"), "Save data must be an object")
    ---@cast save Source.GameInstance.SaveData
    assert(save.version == 1, "Unsupported save version: expected 1")
    assert(Class.isInstance(save.playerKeys, "table") and #save.playerKeys > 0, "Save has no player keys")
    assert(Class.isInstance(save.players, "table"), "Save players must be an object")

    local GlobalData = require("Global.Data")
    local Data = require("Source.Data")
    local SourceSystem = require("Source.System")
    local Locale = require("Source.Locale.Core")
    local Player = require("Source.MapActors.Player")
    local Enemy = require("Source.MapActors.Enemy")
    local DoorBase = require("Source.MapActors.DoorBase")
    local Teleporter = require("Source.MapActors.Teleporter")
    local MotaBattleAbility = require("Source.Gameplay.MotaBattleAbility")
    local BattleResultCode = require("Enums.BattleResultCode")

    GlobalFunctions.NodeGraph.initLatent()
    Data.InitializeRuntime()
    SourceSystem.InstallRuntimeProviders()
    GlobalData.LoadGeneralData()
    GlobalData.LoadCommonFunctions()
    GlobalData.LoadCurves()
    GlobalData.LoadTextConfigs()
    Locale.Init()

    local selectedPlayerKey = bool(request.playerKey) and request.playerKey or save.playerKeys[1]
    ---@type Source.MapActors.Player.Player | nil
    local selectedPlayer
    local players = list():toTable()
    ---@cast players MotaFill.PlayerInfo[]
    local seenPlayerKeys = {}
    for _, key in ipairs(save.playerKeys) do
        assert(Class.isInstance(key, "string") and bool(key), "Saved player key must be a non-empty string")
        assert(not seenPlayerKeys[key], "Duplicate saved player key: " .. key)
        seenPlayerKeys[key] = true
        ---@diagnostic disable-next-line: unnecessary-assert, decoded external saves must reject missing keyed players
        local data = assert(save.players[key], "Saved player data is missing: " .. key)
        local player = Player.FromDict(data)
        assert(player.ID == key, "Saved player ID does not match its key: " .. key)
        players[#players + 1] = {
            key = key,
            name = player:getDisplayName(),
            hp = player:getAttr("HP"),
            attack = player:getAttr("ATK"),
            defense = player:getAttr("DEF")
        }
        if key == selectedPlayerKey then
            selectedPlayer = player
        end
    end
    for key in pairs(save.players) do
        assert(seenPlayerKeys[key], "Saved player is missing from playerKeys: " .. key)
    end
    assert(selectedPlayer ~= nil, "Selected player is missing from the save: " .. tostring(selectedPlayerKey))

    local blueprints = list():toTable()
    ---@cast blueprints MotaFill.BlueprintInfo[]
    local seenBlueprints = {}
    for _, reference in ipairs(request.blueprints) do
        assert(
            Class.isInstance(reference, "string") and bool(reference), "Blueprint reference must be a non-empty string"
        )
        assert(not seenBlueprints[reference], "Duplicate blueprint reference: " .. reference)
        seenBlueprints[reference] = true
        local classPath = GlobalData.ResolveClassPath(reference)
        local definition = assert(GlobalData.GetClass(classPath), "Blueprint class is missing: " .. reference)
        ---@type MotaFill.BlueprintInfo
        local info = { reference = reference, category = "unsupported" }
        if Class.isSubclass(definition, Teleporter) then
            info.category = "teleporter"
        elseif Class.isSubclass(definition, DoorBase) then
            ---@cast definition MotaFill.KeyDoorDefinition
            info.category = "door"
            info.doorKeyId = definition.needKeyID or ""
            info.doorKeyCount = definition.needKeyCount or 0
            assert(Class.isInstance(info.doorKeyId, "string"), "Door key ID must be a string: " .. reference)
            assert(Class.isInstance(info.doorKeyCount, "number") and math.type(info.doorKeyCount) == "integer"
                    and info.doorKeyCount >= 0, "Door key count must be a non-negative integer: " .. reference)
        elseif Class.isSubclass(definition, Enemy) then
            info.category = "enemy"
            if request.operation == "evaluate" then
                local enemy = assert(GlobalData.GenActorFromClassPath(classPath, "MOTA_FILL_PREVIEW"))
                assert(Class.isInstance(enemy, Enemy), "Enemy blueprint created a different Actor type: " .. reference)
                ---@cast enemy Source.MapActors.Enemy
                local result = MotaBattleAbility
                    .new()
                    :calculate(enemy:getAbilitySystemComponent(), GlobalCore.GameplayEventData.new(nil, selectedPlayer))
                local damage = result.data.damage
                assert(
                    Class.isInstance(damage, "number") and math.isFinite(damage) and damage >= 0,
                    "Battle preview returned invalid damage: " .. reference
                )
                info.damage = damage
                info.battleResultCode = result.code
                info.canWin = result.code == BattleResultCode.WIN
            end
        elseif Class.isSubclass(definition, Engine.Actor) and not Class.isSubclass(definition, Player) then
            info.category = "item"
        end
        blueprints[#blueprints + 1] = info
    end
    return {
        success = true,
        error = "",
        selectedPlayerKey = selectedPlayerKey,
        players = players,
        blueprints = blueprints
    }
end

local success, result = xpcall(evaluate, debug.traceback)
local response = success and result or { success = false, error = tostring(result) }
local output = assert(io.open(assert(arg[2], "Result path is required"), "wb"))
assert(output:write(cjson.encode(response)))
assert(output:close())
