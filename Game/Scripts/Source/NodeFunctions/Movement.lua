local Player = require("Source.Player")
local Context = require("Source.NodeFunctions.Context")

local Movement = {}

local LATENT_STARTED = 0
local LATENT_FINISHED = 1

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
    if Class.isInstance(actor, Player) then
        ---@cast actor Source.Player.Player
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
        return { LATENT_STARTED }
    end
    if isMovementFinished(self._actor) then
        self._finished = true
        return { LATENT_FINISHED }
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
    local scene = Context.RequireSceneMap()
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
    local scene = Context.RequireSceneMap()
    if bool(tag) then
        actor = scene:getGameMap():getActorByTag(tag)
    end
    return Movement.SetAutoPathToDestination(actor, destination)
end

return Movement
