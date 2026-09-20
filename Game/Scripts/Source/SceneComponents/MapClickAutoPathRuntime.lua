local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameplayConstants = require("Source.Configs.GameplayConstants")

local GameplayEventData = GlobalCore.GameplayEventData
local MapClickAutoPathRuntime = {}

function MapClickAutoPathRuntime.IsRouteInvalidatedByDanger(
    gameMap, dangerState, route, goal, ignoredGoalEnemies, player
)
    if goal == nil then
        return false
    end
    ---@cast route sf.Vector2i[]
    if not bool(route) then
        return false
    end
    local excludedAnchors = dangerState:getExcludedAnchors(goal, ignoredGoalEnemies, true)
    local excludedRows = {}
    for _, position in ipairs(excludedAnchors) do
        local row = excludedRows[position.y] or {}
        excludedRows[position.y] = row
        row[position.x] = true
    end
    for _, position in ipairs(route) do
        local row = excludedRows[position.y]
        if row ~= nil and row[position.x] then
            return true
        end
    end
    return route[#route] == goal and bool(ignoredGoalEnemies) and gameMap:isPathfindingPassable(player, goal)
        and gameMap:isPassable(player, goal) and dangerState:getDamageAt(goal, ignoredGoalEnemies) > 0
end

function MapClickAutoPathRuntime.GetTeleportPathPositions(route, destination)
    local path = {}
    for _, point in ipairs(route) do
        path[#path + 1] = point:copy()
        if point == destination then
            break
        end
    end
    return path
end

function MapClickAutoPathRuntime.GetInstantWalkCount(route, destination)
    return table.index(route, destination) or 0
end

function MapClickAutoPathRuntime.TriggerInstantWalkStates(player, walkCount)
    if walkCount <= 0 then
        return
    end
    for _ = 1, walkCount do
        player
            :getAbilitySystemComponent()
            :handleGameplayEvent(GameplayEventData.new(player, player, GameplayConstants.MOVEMENT_STEP_EVENT, {}))
    end
end

function MapClickAutoPathRuntime.SetActorDirection(actor, fromPos, goal, rotateWhenSame)
    if actor.direction == nil then
        return
    end
    if rotateWhenSame then
        actor.direction = (actor.direction + 1) % 4
        return
    end
    local dx = goal.x - fromPos.x
    local dy = goal.y - fromPos.y
    if dx == 0 and dy == 0 then
        return
    end
    if math.abs(dx) > math.abs(dy) then
        actor.direction = dx > 0 and Engine.Direction.RIGHT or Engine.Direction.LEFT
    else
        actor.direction = dy > 0 and Engine.Direction.DOWN or Engine.Direction.UP
    end
end

return MapClickAutoPathRuntime
