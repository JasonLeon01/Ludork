local NodeUtils = require("Source.NodeFunctions.Utils")

local EnemyText = {}

function EnemyText.FormatCritical(criticalValue)
    if criticalValue == -2 then
        return ""
    end
    if criticalValue == -1 then
        return "???"
    end
    return tostring(NodeUtils.ToShortNumber(criticalValue))
end

return EnemyText
