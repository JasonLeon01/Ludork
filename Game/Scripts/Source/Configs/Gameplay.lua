local Engine = require("Engine")
local cjson = require("cjson")
local DamageHintLevel = require("Enums.DamageHintLevel")

local GameplayConfig = {}

---@param data table<string, Engine.RuntimeDataValue>
---@param name string
---@return Engine.RuntimeDataValue
local function readOverride(data, name)
    local setting = data[name]
    assert(Class.isInstance(setting, "table"), "Gameplay configuration field must be an object: " .. name)
    ---@cast setting table<string, Engine.RuntimeDataValue>
    if setting.value == cjson.null then
        return nil
    end
    return setting.value
end

---@param data    table<string, Engine.RuntimeDataValue>
---@param name    string
---@param integer boolean
---@return number | nil
local function readPositiveNumber(data, name, integer)
    local value = readOverride(data, name)
    if value == nil then
        return nil
    end
    assert(Class.isInstance(value, "number"), "Gameplay configuration must be a number: " .. name)
    ---@cast value number
    assert(math.isFinite(value) and value > 0, "Gameplay configuration must be finite and positive: " .. name)
    assert(not integer or math.type(value) == "integer", "Gameplay configuration must be an integer: " .. name)
    return value
end

function GameplayConfig.Apply()
    local data = Engine.loadData("./Data/Configs/Gameplay.json")
    assert(Class.isInstance(data, "table"), "Gameplay configuration must be an object")
    ---@cast data table<string, Engine.RuntimeDataValue>
    local damageHintLevel = readOverride(data, "enemyDamageHintLevel")
    local defeatShatterEffectEnabled = readOverride(data, "enemyDefeatShatterEffectEnabled")
    local maxSaveSlots = readPositiveNumber(data, "maxSaveSlots", true)
    local tutorialShrinkDuration = readPositiveNumber(data, "tutorialShrinkDuration", false)
    if damageHintLevel ~= nil then
        assert(Class.isInstance(damageHintLevel, "number"), "Gameplay enemyDamageHintLevel must be an integer")
        ---@cast damageHintLevel number
        assert(math.type(damageHintLevel) == "integer", "Gameplay enemyDamageHintLevel must be an integer")
        local valid = false
        for _, value in pairs(DamageHintLevel) do
            if damageHintLevel == value then
                valid = true
                break
            end
        end
        assert(valid, "Gameplay enemyDamageHintLevel must be a member of Enums.DamageHintLevel")
    end
    if defeatShatterEffectEnabled ~= nil then
        assert(
            Class.isInstance(defeatShatterEffectEnabled, "boolean"),
            "Gameplay enemyDefeatShatterEffectEnabled must be a boolean"
        )
    end
    if damageHintLevel ~= nil then
        local EnemyDamageText = require("Source.MapActors.EnemyDamageText")

        ---@cast damageHintLevel Enums.DamageHintLevel
        EnemyDamageText.EnemyDamageHintLevel = damageHintLevel
    end
    if defeatShatterEffectEnabled ~= nil then
        local Enemy = require("Source.MapActors.Enemy")

        ---@cast defeatShatterEffectEnabled boolean
        Enemy.DefeatShatterEffectEnabled = defeatShatterEffectEnabled
    end
    if maxSaveSlots ~= nil then
        local WindowSaveSlot = require("Source.Windows.WindowSaveLoad.Slot")

        ---@cast maxSaveSlots integer
        WindowSaveSlot.MAX_SAVE_SLOTS = maxSaveSlots
    end
    if tutorialShrinkDuration ~= nil then
        local WindowTutorial = require("Source.Windows.WindowTutorial")

        WindowTutorial.SHRINK_DURATION = tutorialShrinkDuration
    end
end

return GameplayConfig
