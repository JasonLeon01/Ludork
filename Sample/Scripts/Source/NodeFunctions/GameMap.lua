local Context = require("Source.NodeFunctions.Context")

local GameMap = {}

---@return GameMap | nil
local function getCurrentGameMap()
    return Context.requireSceneMap():getGameMap()
end

function GameMap.GetActorByTag(tag)
    local gameMap = getCurrentGameMap()
    if gameMap == nil then
        return nil
    end
    return gameMap:getActorByTag(tag)
end

function GameMap.GetAllActors()
    local gameMap = getCurrentGameMap()
    if gameMap == nil then
        return {}
    end
    return gameMap:getAllActors()
end

return GameMap
