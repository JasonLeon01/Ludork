local Engine = require("Engine")
local GameplayEventData = require("Global.Gameplay.GameplayEventData")
local Teleporter = require("Source.Teleporter")

local MapClickAutoPathRuntime = {}

function MapClickAutoPathRuntime.HasTeleporterAt(gameMap, goal)
    for _, actor in ipairs(gameMap:getActorsAt(goal.x, goal.y)) do
        if Class.isInstance(actor, Teleporter) and not actor:isDestroyed() then
            return true
        end
    end
    return false
end

function MapClickAutoPathRuntime.IsRouteInvalidatedByDanger(autoPath, player)
    local goal = autoPath._activeGoal
    if goal == nil then
        return false
    end
    local route = autoPath._routeState:getRoute()
    ---@cast route sf.Vector2i[]
    if not bool(route) then
        return false
    end
    local ignoredGoalEnemies = autoPath:_getIgnoredGoalEnemies(goal)
    local excludedAnchors = autoPath._dangerState:getExcludedAnchors(goal, ignoredGoalEnemies, true)
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
    return route[#route] == goal and bool(ignoredGoalEnemies) and autoPath._parent:isPathfindingPassable(player, goal)
        and autoPath._parent:isPassable(player, goal)
        and autoPath._dangerState:getDamageAt(goal, ignoredGoalEnemies) > 0
end

function MapClickAutoPathRuntime.GetTeleportPathPositions(route, destination)
    local path = {}
    for _, point in ipairs(route) do
        path[#path + 1] = copy(point)
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
        player:getAbilitySystemComponent():handleGameplayEvent(GameplayEventData.new({
                instigator = player,
                target = player,
                eventTag = "Event.Movement.Step",
                payload = {}
            }))
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
