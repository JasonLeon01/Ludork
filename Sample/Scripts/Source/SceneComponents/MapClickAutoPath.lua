local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local ComponentBase = require("Global.Components.ComponentBase")
local Pool = require("Global.Pool")
local Enemy = require("Source.Enemy")
local MovementSpecials = require("Source.MovementSpecials")
local SpecialAbilities = require("Source.Gameplay.SpecialAbilities")
local MapClickAutoPathRuntime = require("Source.SceneComponents.MapClickAutoPathRuntime")

local Input = Engine.Input
local Actor = Engine.Actor
local System = GlobalCore.System
local NEIGHBOUR_OFFSET_DOWN = sf.Vector2i.new(0, 1)
local NEIGHBOUR_OFFSET_UP = sf.Vector2i.new(0, -1)
local NEIGHBOUR_OFFSET_RIGHT = sf.Vector2i.new(1, 0)
local NEIGHBOUR_OFFSET_LEFT = sf.Vector2i.new(-1, 0)
local NEIGHBOUR_OFFSETS = { NEIGHBOUR_OFFSET_DOWN, NEIGHBOUR_OFFSET_UP, NEIGHBOUR_OFFSET_RIGHT, NEIGHBOUR_OFFSET_LEFT }

---@class Source.SceneComponents.MapClickAutoPath
local MapClickAutoPath = {}

function MapClickAutoPath:init(gameMap, routeState, dangerState)
    super(MapClickAutoPath, self).init(gameMap)
    self._routeState = routeState
    self._dangerState = dangerState
    self._autoPathing = false
    self._activeGoal = nil
    self._routeDangerRevision = nil
    self._pendingGoals = {}
    self._previewMapX = nil
    self._previewMapY = nil
end

function MapClickAutoPath:onLateTick(_deltaTime)
    local player = self._parent:getPlayer()
    if player == nil then
        return
    end
    ---@cast player Source.Player.Player
    if self:_isAutoPathBlocked(player) then
        return
    end
    if Input.isMouseButtonTriggered(sf.Mouse.Button.Left, false) then
        local goal = self:_getMouseMapPosition()
        if goal ~= nil then
            self._pendingGoals[#self._pendingGoals + 1] = goal
            Input.isMouseButtonTriggered(sf.Mouse.Button.Left, true)
        end
        return
    end
    if Input.isTouchTap(false) then
        local goal = self:_getTouchMapPosition()
        if goal ~= nil then
            self._pendingGoals[#self._pendingGoals + 1] = goal
            Input.isTouchTap(true)
        end
    end
end

function MapClickAutoPath:onTick(_deltaTime)
    local player = self._parent:getPlayer()
    if player == nil then
        self._autoPathing = false
        self._activeGoal = nil
        self._routeDangerRevision = nil
        self._routeState:clear()
        self._pendingGoals = {}
        self._previewMapX = nil
        self._previewMapY = nil
        return
    end
    ---@cast player Source.Player.Player
    if self:_isAutoPathBlocked(player) then
        self._autoPathing = false
        self._activeGoal = nil
        self._routeDangerRevision = nil
        self._routeState:clear()
        self._pendingGoals = {}
        self._previewMapX = nil
        self._previewMapY = nil
        return
    end
    local currentPos = player:getMapPosition()
    if self._autoPathing and (not player:isInRoute() or self._activeGoal ~= nil and currentPos == self._activeGoal) then
        self._autoPathing = false
        self._activeGoal = nil
        self._routeDangerRevision = nil
        self._routeState:clear()
    end
    if self._previewMapX ~= currentPos.x or self._previewMapY ~= currentPos.y then
        self:_trimPreviewRoute(currentPos)
        self._previewMapX = currentPos.x
        self._previewMapY = currentPos.y
    end
    local dangerRevision = self._dangerState:getPathfindingRevision()
    if self._autoPathing and self._routeDangerRevision ~= dangerRevision then
        if MapClickAutoPathRuntime.IsRouteInvalidatedByDanger(self, player) then
            self:_replanForDangerChange(player)
        else
            self._routeDangerRevision = dangerRevision
        end
    end
    local goals = self:_drainPendingGoals()
    if not bool(goals) then
        return
    end
    if self._autoPathing then
        local clickedGoal = goals[#goals]
        local route = self._routeState:getRoute()
        ---@cast route sf.Vector2i[]
        if bool(route) and clickedGoal == route[#route] then
            self:_finishAutoPathImmediately(player, currentPos)
        else
            player:stop()
        end
        self._autoPathing = false
        self._activeGoal = nil
        self._routeDangerRevision = nil
        self._routeState:clear()
        return
    end
    for _, goal in ipairs(goals) do
        if self:_isInMap(goal) then
            local start = player:getMapPosition()
            if start == goal then
                self._routeState:clear()
                MapClickAutoPathRuntime.SetActorDirection(player, start, goal, true)
            else
                local plan = self:_buildAutoPathPlan(player, start, goal)
                if plan ~= nil then
                    self:_startAutoPath(player, start, goal, plan)
                end
            end
        end
    end
end

---@param player Source.Player.Player
---@param start  sf.Vector2i
---@param goal   sf.Vector2i
---@param plan   MapClickAutoPath.Plan
function MapClickAutoPath:_startAutoPath(player, start, goal, plan)
    self._routeState:setRoute(plan.route)
    self:_trimPreviewRoute(start)
    self._previewMapX = start.x
    self._previewMapY = start.y
    self._activeGoal = copy(goal)
    self._routeDangerRevision = self._dangerState:getPathfindingRevision()
    player:setRoute(plan.routeSteps)
    self._autoPathing = true
end

---@param player Source.Player.Player
function MapClickAutoPath:_replanForDangerChange(player)
    local goal = self._activeGoal ~= nil and copy(self._activeGoal) or nil
    local stablePosition = player:getMapPosition()
    player:stop()
    player:setMapPosition(stablePosition)
    self._parent:updateActorOccupancy(player)
    self._autoPathing = false
    self._routeState:clear()
    self._routeDangerRevision = self._dangerState:getPathfindingRevision()
    if goal == nil then
        return
    end
    local start = player:getMapPosition()
    if start == goal then
        self._activeGoal = nil
        self._routeDangerRevision = nil
        return
    end
    local plan = self:_buildAutoPathPlan(player, start, goal)
    if plan == nil then
        self._activeGoal = nil
        self._routeDangerRevision = nil
        return
    end
    self:_startAutoPath(player, start, goal, plan)
end

---@param player Source.Player.Player
---@param start  sf.Vector2i
function MapClickAutoPath:_finishAutoPathImmediately(player, start)
    local route = self._routeState:getRoute()
    ---@cast route sf.Vector2i[]
    local walkedPath = player:consumeMovementSpecialPath()
    if not bool(route) then
        player:stop()
        self:_dispatchInstantMoveOverlaps(player)
        local pathPositions = bool(walkedPath) and walkedPath or { copy(start) }
        MovementSpecials.NotifyPlayerMovementFinished(player, pathPositions)
        return
    end
    local goal = route[#route]
    local goalPassable = self._parent:isPassable(player, goal)
    player:stop()
    local destination = goalPassable and goal or (#route >= 2 and route[#route - 1] or start)
    local walkCount = MapClickAutoPathRuntime.GetInstantWalkCount(route, destination)
    local teleportPath = MapClickAutoPathRuntime.GetTeleportPathPositions(route, destination)
    local destinationPosition = Pool.Get("sf.Vector2u", sf.Vector2u, {
        x = destination.x,
        y = destination.y
    })
    player:setMapPosition(destinationPosition)
    Pool.Put("sf.Vector2u", destinationPosition)
    self._parent:updateActorOccupancy(player)
    self:_dispatchInstantMoveOverlaps(player)
    MapClickAutoPathRuntime.TriggerInstantWalkStates(player, walkCount)
    local fromPos = #route >= 2 and route[#route - 1] or start
    MapClickAutoPathRuntime.SetActorDirection(player, fromPos, goal, start == goal)
    if not goalPassable and destination ~= goal then
        local moveOffset = Pool.Get("sf.Vector2i", sf.Vector2i, {
            x = goal.x - destination.x,
            y = goal.y - destination.y
        })
        player:MapMove(moveOffset)
        Pool.Put("sf.Vector2i", moveOffset)
    end
    local pathPositions = {}
    for _, point in ipairs(walkedPath) do
        pathPositions[#pathPositions + 1] = point
    end
    for _, point in ipairs(teleportPath) do
        pathPositions[#pathPositions + 1] = point
    end
    if not bool(pathPositions) then
        pathPositions[1] = copy(destination)
    end
    MovementSpecials.NotifyPlayerMovementFinished(player, pathPositions)
end

---@param player Source.Player.Player
function MapClickAutoPath:_dispatchInstantMoveOverlaps(player)
    local overlaps = self._parent:getOverlaps(player)
    if not bool(overlaps) then
        return
    end
    Actor.BlueprintEvent(player, Actor, "onOverlap", {
        other = overlaps
    })
    for _, overlap in ipairs(overlaps) do
        Actor.BlueprintEvent(overlap, Actor, "onOverlap", {
            other = { player }
        })
    end
end

---@param actor Engine.Actor
---@param start sf.Vector2i
---@param goal  sf.Vector2i
---@return MapClickAutoPath.Plan | nil
function MapClickAutoPath:_buildAutoPathPlan(actor, start, goal)
    if start == goal then
        local routeSteps = {} ---@type sf.Vector2i[]
        local routeStart = copy(start)
        local route = { routeStart } ---@type sf.Vector2i[]
        return { routeSteps = routeSteps, route = route, goalPassable = true }
    end
    local ignoredGoalEnemies = self:_getIgnoredGoalEnemies(goal)
    local excludedAnchors = self._dangerState:getExcludedAnchors(goal, ignoredGoalEnemies, true)
    local direct = self:_buildPathToTarget(actor, start, goal, excludedAnchors)
    local goalPassable = self._parent:isPathfindingPassable(actor, goal)
    local goalActuallyPassable = self._parent:isPassable(actor, goal)
    local goalWillBeEntered = goalPassable and goalActuallyPassable
    if goalWillBeEntered and bool(ignoredGoalEnemies) and self._dangerState:getDamageAt(goal, ignoredGoalEnemies) > 0 then
        return nil
    end
    if direct ~= nil and direct.route[#direct.route] == goal then
        if goalActuallyPassable and not goalPassable and not MapClickAutoPathRuntime.HasTeleporterAt(self._parent, goal) then
            local routeSteps = deepcopy(direct.routeSteps) ---@type sf.Vector2i[]
            table.remove(routeSteps)
            local route = deepcopy(direct.route) ---@type sf.Vector2i[]
            table.remove(route)
            return { routeSteps = routeSteps, route = route, goalPassable = false }
        end
        return { routeSteps = direct.routeSteps, route = direct.route, goalPassable = goalActuallyPassable }
    end
    if goalPassable then
        return nil
    end
    ---@type MapClickAutoPath.Path | nil
    local bestPlan
    for _, offset in ipairs(NEIGHBOUR_OFFSETS) do
        local neighbour = Pool.Get("sf.Vector2i", sf.Vector2i, {
            x = goal.x + offset.x,
            y = goal.y + offset.y
        })
        if self:_isInMap(neighbour) and (neighbour == start or self._parent:isPathfindingPassable(actor, neighbour)) then
            local routeSteps
            local route
            ---@cast routeSteps sf.Vector2i[] | nil
            ---@cast route sf.Vector2i[] | nil
            if neighbour == start then
                local emptyRouteSteps = {} ---@type sf.Vector2i[]
                local routeStart = copy(start)
                local startRoute = { routeStart } ---@type sf.Vector2i[]
                routeSteps = emptyRouteSteps
                route = startRoute
            else
                local neighbourPlan = self:_buildPathToTarget(actor, start, neighbour, excludedAnchors)
                if neighbourPlan ~= nil and neighbourPlan.route[#neighbourPlan.route] == neighbour then
                    routeSteps = neighbourPlan.routeSteps
                    route = neighbourPlan.route
                end
            end
            if route ~= nil and (bestPlan == nil or #route < #bestPlan.route) then
                bestPlan = { routeSteps = assert(routeSteps), route = route }
            end
        end
        Pool.Put("sf.Vector2i", neighbour)
    end
    if bestPlan == nil then
        return nil
    end
    if goalActuallyPassable or not bool(self._parent:getCollisionAt(goal.x, goal.y, actor)) then
        return { routeSteps = bestPlan.routeSteps, route = bestPlan.route, goalPassable = false }
    end
    local fullRoute = {}
    for _, point in ipairs(bestPlan.route) do
        fullRoute[#fullRoute + 1] = copy(point)
    end
    fullRoute[#fullRoute + 1] = copy(goal)
    local routeSteps = {}
    for _, point in ipairs(bestPlan.routeSteps) do
        routeSteps[#routeSteps + 1] = copy(point)
    end
    local stopPos = assert(fullRoute[#fullRoute - 1])
    ---@cast stopPos sf.Vector2i
    local offsetX = goal.x - stopPos.x
    local offsetY = goal.y - stopPos.y
    ---@cast offsetX integer
    ---@cast offsetY integer
    routeSteps[#routeSteps + 1] = sf.Vector2i.new(offsetX, offsetY)
    return { routeSteps = routeSteps, route = fullRoute, goalPassable = false }
end

---@param actor           Engine.Actor
---@param start           sf.Vector2i
---@param target          sf.Vector2i
---@param excludedAnchors sf.Vector2i[]
---@return MapClickAutoPath.Path | nil
function MapClickAutoPath:_buildPathToTarget(actor, start, target, excludedAnchors)
    local pathResult = self._parent:findPathResult(start, target, actor, excludedAnchors)
    if not bool(pathResult.route) then
        return nil
    end
    return { routeSteps = pathResult.offsets, route = pathResult.route }
end

---@param goal sf.Vector2i
---@return Source.Enemy[]
function MapClickAutoPath:_getIgnoredGoalEnemies(goal)
    local enemies = {}
    for _, actor in ipairs(self._parent:getAllActors()) do
        if Class.isInstance(actor, Enemy) then
            ---@cast actor Source.Enemy
            local abilitySystem = actor:getAbilitySystemComponent()
            if not actor:isDestroyed() and abilitySystem:hasMatchingGameplayTag(SpecialAbilities.MOVEMENT_HAZARD_TAG) then
                if table.contains(actor:getOccupiedMapCells(), goal) then
                    enemies[#enemies + 1] = actor
                end
            end
        end
    end
    return enemies
end

---@return sf.Vector2i | nil
function MapClickAutoPath:_getMouseMapPosition()
    local mousePos = Input.getMousePosition()
    return self:_getInputMapPosition(mousePos.x, mousePos.y)
end

---@return sf.Vector2i | nil
function MapClickAutoPath:_getTouchMapPosition()
    local touchPos = Input.getTouchTapPosition()
    if touchPos == nil then
        return nil
    end
    return self:_getInputMapPosition(touchPos.x, touchPos.y)
end

---@param positionX number
---@param positionY number
---@return sf.Vector2i | nil
function MapClickAutoPath:_getInputMapPosition(positionX, positionY)
    local scale = System.getScale()
    if scale > 0.0 then
        positionX = math.floor(positionX / scale)
        positionY = math.floor(positionY / scale)
    end
    local mapViewRect = self._parent:getMapViewRect()
    local mapViewPixel = Pool.Get("sf.Vector2i", sf.Vector2i, {
        x = positionX,
        y = positionY
    })
    if not mapViewRect:contains(mapViewPixel) then
        Pool.Put("sf.Vector2i", mapViewPixel)
        return nil
    end
    mapViewPixel.x = mapViewPixel.x - mapViewRect.position.x
    mapViewPixel.y = mapViewPixel.y - mapViewRect.position.y
    local camera = assert(self._parent:getCamera(), "Map click pathfinding requires a camera")
    local worldPos = camera:mapPixelToCoords(mapViewPixel)
    Pool.Put("sf.Vector2i", mapViewPixel)
    local mapPosition = sf.Vector2i.new(
        math.floor(worldPos.x / Engine.CellSize), math.floor(worldPos.y / Engine.CellSize)
    )
    ---@cast mapPosition sf.Vector2i
    return mapPosition
end

---@param position sf.Vector2i
---@return boolean
function MapClickAutoPath:_isInMap(position)
    local size = self._parent:getSize()
    return position.x >= 0 and position.x < size.x and position.y >= 0 and position.y < size.y
end

---@return sf.Vector2i[]
function MapClickAutoPath:_drainPendingGoals()
    local goals = {}
    goals, self._pendingGoals = self._pendingGoals, goals
    return goals
end

---@param player Source.Player.Player
---@return boolean
function MapClickAutoPath:_isAutoPathBlocked(player)
    if not player:getMoveEnabled() then
        return true
    end
    if player:getForbiddenMoving() then
        return true
    end
    local scene = self._parent:getScene()
    return scene ~= nil and scene:isInputBlocked()
end

---@param currentPos sf.Vector2i
function MapClickAutoPath:_trimPreviewRoute(currentPos)
    local route = self._routeState:getRoute()
    ---@cast route sf.Vector2i[]
    if not bool(route) then
        return
    end
    local index = table.index(route, currentPos) or -1
    if index >= 0 then
        ---@type sf.Vector2i[]
        local remaining = {}
        for currentIndex = index + 1, #route do
            remaining[#remaining + 1] = route[currentIndex]
        end
        self._routeState:setRoute(remaining)
    end
end

return class(MapClickAutoPath, ComponentBase)
