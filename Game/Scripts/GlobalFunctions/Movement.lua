local Player
local function loadPlayer()
    if Player == nil then
        Player = require("Source.MapActors.Player")
    end
    return Player
end

local Context
local function loadContext()
    if Context == nil then
        Context = require("GlobalFunctions.Context")
    end
    return Context
end

local MovementLatentOutputs
local function loadMovementLatentOutputs()
    if MovementLatentOutputs == nil then
        MovementLatentOutputs = require("Source.Configs.MovementLatentOutputs")
    end
    return MovementLatentOutputs
end

local Movement = {}

---@param actor       Engine.Actor
---@param destination sf.Vector2i
---@return sf.Vector2i[]
local function buildRouteToDestination(actor, destination)
    local gameMap = actor:getMap()
    if gameMap == nil then
        return {}
    end
    ---@cast gameMap GameMap
    local start = actor:getMapPosition()
    local goal = destination
    if start == goal then
        return {}
    end
    local pathResult = gameMap:findPathResult(start, goal, actor)
    if not bool(pathResult.route) or pathResult.route[#pathResult.route] ~= goal then
        return {}
    end
    return pathResult.offsets
end

---@param actor Engine.Actor | nil
---@return boolean
local function isMovementFinished(actor)
    if actor == nil or actor:isDestroyed() then
        return true
    end
    return not actor:isMoving() and not actor:isInRoute()
end

---@param actor Engine.Actor | nil
---@return boolean
local function isMovementBlocked(actor)
    if actor == nil then
        return true
    end
    if Class.isInstance(actor, loadPlayer()) then
        ---@cast actor Source.MapActors.Player.Player
        if actor:getForbiddenMoving() then
            return true
        end
    end
    local gameMap = actor:getMap()
    if gameMap == nil then
        return false
    end
    ---@cast gameMap GameMap
    local scene = gameMap:getScene()
    ---@cast scene Source.Scenes.SceneMap.SceneMap | nil
    return scene ~= nil and scene:isInputBlocked()
end

local MovementCondition = {}

function MovementCondition:init(actor)
    self._actor = actor
    ---@type boolean
    self._startedEmitted = false
    self._finished = false
end

function MovementCondition:poll()
    if not self._startedEmitted then
        self._startedEmitted = true
        return { loadMovementLatentOutputs().STARTED }
    end
    if isMovementFinished(self._actor) then
        self._finished = true
        return { loadMovementLatentOutputs().FINISHED }
    end
    return {}
end

function MovementCondition:isFinished()
    return self._finished
end

local FinalMovementCondition = class(MovementCondition)
FinalMovementCondition.__call = MovementCondition.poll

function Movement.SetMoveEnabledByTag(tag, enabled)
    enabled = enabled == nil and true or enabled
    local scene = loadContext().RequireSceneMap()
    local actor = scene:getGameMap():getActorByTag(tag)
    if actor ~= nil then
        actor:setMoveEnabled(enabled)
    end
end

function Movement.SetMoveRoute(actor, route)
    route = route or {}
    if actor ~= nil and not isMovementBlocked(actor) then
        actor:setRoute(route)
    end
    return FinalMovementCondition.new(actor)
end

function Movement.SetAutoPathToDestination(actor, destination)
    destination = destination or sf.Vector2i.new(0, 0)
    if actor ~= nil and not isMovementBlocked(actor) then
        actor:setRoute(buildRouteToDestination(actor, destination))
    end
    return FinalMovementCondition.new(actor)
end

function Movement.SetAutoPathToDestinationByTag(tag, destination)
    destination = destination or sf.Vector2i.new(0, 0)
    local actor = nil
    local scene = loadContext().RequireSceneMap()
    if bool(tag) then
        actor = scene:getGameMap():getActorByTag(tag)
    end
    return Movement.SetAutoPathToDestination(actor, destination)
end

return Movement
