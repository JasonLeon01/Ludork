local CriticalResultCode = require("Enums.CriticalResultCode")
local NumberFormat = require("Source.Utils.NumberFormat")

local EnemyText = {}

function EnemyText.FormatCritical(result)
    if result.code == CriticalResultCode.NOT_NEEDED then
        return ""
    end
    if result.code == CriticalResultCode.UNKNOWN then
        return "???"
    end
    assert(result.code == CriticalResultCode.VALUE, "Unsupported critical-value result: " .. tostring(result.code))
    local criticalValue = result.data.value
    ---@cast criticalValue integer
    return tostring(NumberFormat.ToShortNumber(criticalValue))
end

return EnemyText
