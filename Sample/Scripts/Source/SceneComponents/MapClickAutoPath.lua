local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local ComponentBase = require("Global.Components.ComponentBase")
local Pool = require("Global.Pool")
local Enemy = require("Source.Enemy")
---@type { Special: Source.Configs.GeneralEnum.Special }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local MovementSpecials = require("Source.MovementSpecials")
local Teleporter = require("Source.Teleporter")

local Input = Engine.Input
local Actor = Engine.Actor
local System = GlobalCore.System
local Special = GeneralEnum.Special

local NEIGHBOUR_OFFSET_DOWN = sf.Vector2i.new(0, 1)
local NEIGHBOUR_OFFSET_UP = sf.Vector2i.new(0, -1)
local NEIGHBOUR_OFFSET_RIGHT = sf.Vector2i.new(1, 0)
local NEIGHBOUR_OFFSET_LEFT = sf.Vector2i.new(-1, 0)
local NEIGHBOUR_OFFSETS = { NEIGHBOUR_OFFSET_DOWN, NEIGHBOUR_OFFSET_UP, NEIGHBOUR_OFFSET_RIGHT, NEIGHBOUR_OFFSET_LEFT }

---@param gameMap GameMap
---@param goal    sf.Vector2i
---@return boolean
local function hasTeleporterAt(gameMap, goal)
    for _, actor in ipairs(gameMap:getAllActors()) do
        if Class.isInstance(actor, Teleporter) and not actor:isDestroyed() then
            for _, cell in ipairs(actor:getOccupiedMapCells()) do
                if cell == goal then
                    return true
                end
            end
        end
    end
    return false
end

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
    local gameMap = self._parent
    local player = gameMap:getPlayer()
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
    if self._autoPathing and self._routeDangerRevision ~= self._dangerState:getRevision() then
        self:_replanForDangerChange(player)
    end
    local currentPos = player:getMapPosition()
    if self._previewMapX ~= currentPos.x or self._previewMapY ~= currentPos.y then
        self:_trimPreviewRoute(currentPos)
        self._previewMapX = currentPos.x
        self._previewMapY = currentPos.y
    end
    if self._autoPathing and not player:isMoving() and not player:isInRoute() then
        self._autoPathing = false
        self._activeGoal = nil
        self._routeDangerRevision = nil
        self._routeState:clear()
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
                MapClickAutoPath._setActorDirection(player, start, goal, true)
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
    local activeGoal = sf.Vector2i.new(goal.x, goal.y)
    ---@cast activeGoal sf.Vector2i
    self._activeGoal = activeGoal
    self._routeDangerRevision = self._dangerState:getRevision()
    player:setRoute(plan.routeSteps)
    self._autoPathing = true
end

---@param player Source.Player.Player
function MapClickAutoPath:_replanForDangerChange(player)
    local goal = self._activeGoal
    local stablePosition = player:getMapPosition()
    player:stop()
    player:setMapPosition(stablePosition)
    self._parent:updateActorOccupancy(player)
    self._autoPathing = false
    self._routeState:clear()
    self._routeDangerRevision = self._dangerState:getRevision()
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
        local pathPositions = bool(walkedPath) and walkedPath or { sf.Vector2i.new(start.x, start.y) }
        MovementSpecials.notifyPlayerMovementFinished(player, pathPositions)
        return
    end
    local goal = route[#route]
    local goalPassable = self._parent:isPassable(player, goal)
    player:stop()
    local destination = goalPassable and goal or (#route >= 2 and route[#route - 1] or start)
    local walkCount = MapClickAutoPath._getInstantWalkCount(route, destination)
    local teleportPath = MapClickAutoPath._getTeleportPathPositions(route, destination)
    local destinationPosition = Pool.Get("sf.Vector2u", sf.Vector2u, {
        x = destination.x,
        y = destination.y
    })
    player:setMapPosition(destinationPosition)
    Pool.Put("sf.Vector2u", destinationPosition)
    self._parent:updateActorOccupancy(player)
    self:_dispatchInstantMoveOverlaps(player)
    MapClickAutoPath._triggerInstantWalkStates(player, walkCount)
    local fromPos = #route >= 2 and route[#route - 1] or start
    MapClickAutoPath._setActorDirection(player, fromPos, goal, start == goal)
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
        pathPositions[1] = sf.Vector2i.new(destination.x, destination.y)
    end
    MovementSpecials.notifyPlayerMovementFinished(player, pathPositions)
end

---@param route       sf.Vector2i[]
---@param destination sf.Vector2i
---@return sf.Vector2i[]
function MapClickAutoPath._getTeleportPathPositions(route, destination)
    local path = {}
    for _, point in ipairs(route) do
        path[#path + 1] = sf.Vector2i.new(point.x, point.y)
        if point == destination then
            break
        end
    end
    return path
end

---@param route       sf.Vector2i[]
---@param destination sf.Vector2i
---@return integer
function MapClickAutoPath._getInstantWalkCount(route, destination)
    for index, point in ipairs(route) do
        if point == destination then
            return index
        end
    end
    return 0
end

---@param player    Source.Player.Player
---@param walkCount integer
function MapClickAutoPath._triggerInstantWalkStates(player, walkCount)
    if walkCount <= 0 then
        return
    end
    for _ = 1, walkCount do
        player:triggerStateWalk()
    end
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
        ---@type sf.Vector2i[]
        local routeSteps = {}
        local routeStart = sf.Vector2i.new(start.x, start.y)
        ---@cast routeStart sf.Vector2i
        ---@type sf.Vector2i[]
        local route = { routeStart }
        return { routeSteps = routeSteps, route = route, goalPassable = true }
    end
    local gameMap = self._parent
    local ignoredGoalEnemies = self:_getIgnoredGoalEnemies(goal)
    local excludedAnchors = self._dangerState:getExcludedAnchors(goal, ignoredGoalEnemies, true)
    local direct = self:_buildPathToTarget(actor, start, goal, excludedAnchors)
    local goalPassable = gameMap:isPathfindingPassable(actor, goal)
    local goalActuallyPassable = gameMap:isPassable(actor, goal)
    local goalWillBeEntered = goalPassable and goalActuallyPassable
    if goalWillBeEntered and bool(ignoredGoalEnemies)
        and self._dangerState:getDamageAt(goal, ignoredGoalEnemies) > 0 then
        return nil
    end
    if direct ~= nil and direct.route[#direct.route] == goal then
        if goalActuallyPassable and not goalPassable and not hasTeleporterAt(gameMap, goal) then
            ---@type sf.Vector2i[]
            local routeSteps = deepcopy(direct.routeSteps)
            table.remove(routeSteps)
            ---@type sf.Vector2i[]
            local route = deepcopy(direct.route)
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
        if self:_isInMap(neighbour) and (neighbour == start or gameMap:isPathfindingPassable(actor, neighbour)) then
            ---@type sf.Vector2i[] | nil
            local routeSteps
            ---@type sf.Vector2i[] | nil
            local route
            if neighbour == start then
                ---@type sf.Vector2i[]
                local emptyRouteSteps = {}
                routeSteps = emptyRouteSteps
                local routeStart = sf.Vector2i.new(start.x, start.y)
                ---@cast routeStart sf.Vector2i
                ---@type sf.Vector2i[]
                local startRoute = { routeStart }
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
    if goalActuallyPassable then
        return { routeSteps = bestPlan.routeSteps, route = bestPlan.route, goalPassable = false }
    end
    local fullRoute = {}
    for _, point in ipairs(bestPlan.route) do
        fullRoute[#fullRoute + 1] = sf.Vector2i.new(point.x, point.y)
    end
    fullRoute[#fullRoute + 1] = sf.Vector2i.new(goal.x, goal.y)
    local routeSteps = {}
    for _, point in ipairs(bestPlan.routeSteps) do
        routeSteps[#routeSteps + 1] = sf.Vector2i.new(point.x, point.y)
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

---@param actor  Engine.Actor
---@param start  sf.Vector2i
---@param target sf.Vector2i
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
            if not actor:isDestroyed()
                and (actor:hasSpecial(Special.Domain) or actor:hasSpecial(Special.Blockade)) then
                for _, cell in ipairs(actor:getOccupiedMapCells()) do
                    if cell == goal then
                        enemies[#enemies + 1] = actor
                        break
                    end
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
---@return sf.Vector2i
function MapClickAutoPath:_getInputMapPosition(positionX, positionY)
    local scale = System.getScale()
    if scale > 0.0 then
        positionX = math.floor(positionX / scale)
        positionY = math.floor(positionY / scale)
    end
    local mapOffset = self._parent:getMapViewOffset()
    local mapViewPixel = Pool.Get("sf.Vector2i", sf.Vector2i, {
        x = positionX - mapOffset.x,
        y = positionY - mapOffset.y
    })
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

---@param actor          Source.Player.Player
---@param fromPos        sf.Vector2i
---@param goal           sf.Vector2i
---@param rotateWhenSame boolean
function MapClickAutoPath._setActorDirection(actor, fromPos, goal, rotateWhenSame)
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

---@return sf.Vector2i[]
function MapClickAutoPath:_drainPendingGoals()
    local goals = self._pendingGoals
    self._pendingGoals = {}
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
    local index = -1
    for currentIndex, point in ipairs(route) do
        if point == currentPos then
            index = currentIndex
            break
        end
    end
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
