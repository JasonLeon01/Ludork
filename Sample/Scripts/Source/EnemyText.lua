local MotaBattleAbility = require("Source.Gameplay.MotaBattleAbility")
local NumberFormat = require("Source.Utils.NumberFormat")

local EnemyText = {}

function EnemyText.FormatCritical(result)
    if result.code == MotaBattleAbility.CriticalResult.NOT_NEEDED then
        return ""
    end
    if result.code == MotaBattleAbility.CriticalResult.UNKNOWN then
        return "???"
    end
    assert(
        result.code == MotaBattleAbility.CriticalResult.VALUE,
        "Unsupported critical-value result: " .. tostring(result.code)
    )
    local criticalValue = result.data.value
    ---@cast criticalValue integer
    return tostring(NumberFormat.ToShortNumber(criticalValue))
end

return EnemyText
