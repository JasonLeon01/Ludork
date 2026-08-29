local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local Pool = require("Global.Pool")

local System = GlobalCore.System

local Character = Engine.Character

local LISTENER_DIRECTION_UP = sf.Vector3f.new(0.0, -1.0, 0.0)
local LISTENER_DIRECTION_DOWN = sf.Vector3f.new(0.0, 1.0, 0.0)
local LISTENER_DIRECTION_LEFT = sf.Vector3f.new(-1.0, 0.0, 0.0)
local LISTENER_DIRECTION_RIGHT = sf.Vector3f.new(1.0, 0.0, 0.0)
local DEFAULT_LISTENER_DIRECTION = sf.Vector3f.new(0.0, 0.0, -1.0)
local CHARACTER_LISTENER_UP_VECTOR = sf.Vector3f.new(0.0, 0.0, -1.0)
local DEFAULT_LISTENER_UP_VECTOR = sf.Vector3f.new(0.0, 1.0, 0.0)

---@type GameMapImplState
local GameMapPresentation = {}

function GameMapPresentation:getPlayer()
    return self._player
end

function GameMapPresentation:setPlayer(player)
    if self._camera == nil then
        return
    end
    if self._player == nil then
        self._camera:setParent(player)
    end
    self._player = player
    self:setPlayerActor(player)
    self:_updateAudioListener()
end

function GameMapPresentation:worldToMapViewPosition(position)
    local camera = self:getCamera()
    if camera == nil then
        return copy(position)
    end
    local viewPosition = camera:getViewPosition()
    if viewPosition == nil then
        return copy(position)
    end
    return sf.Vector2f.new(position.x - viewPosition.x, position.y - viewPosition.y)
end

function GameMapPresentation:worldToUIScreenPosition(position)
    local mapPosition = self:worldToMapViewPosition(position)
    local mapViewPosition = self._mapViewRect.position
    return sf.Vector2f.new(mapPosition.x + mapViewPosition.x, mapPosition.y + mapViewPosition.y)
end

function GameMapPresentation:worldToCanvasPosition(position)
    local uiPosition = self:worldToUIScreenPosition(position)
    local scale = System.getScale()
    return sf.Vector2f.new(uiPosition.x * scale, uiPosition.y * scale)
end

function GameMapPresentation:_updateAudioListener()
    if self._player == nil then
        return
    end
    local position = self._player:getPosition()
    local listenerVector = Pool.Get("sf.Vector3f", sf.Vector3f, {
        x = position.x,
        y = position.y,
        z = 0.0
    })
    ---@cast listenerVector sf.Vector3f
    sf.Listener.setPosition(listenerVector)
    Pool.Put("sf.Vector3f", listenerVector)
    if Class.isInstance(self._player, Character) then
        ---@cast self._player Engine.Character
        sf.Listener.setDirection(self:_getAudioListenerDirection(self._player.direction))
        sf.Listener.setUpVector(CHARACTER_LISTENER_UP_VECTOR)
    else
        sf.Listener.setDirection(DEFAULT_LISTENER_DIRECTION)
        sf.Listener.setUpVector(DEFAULT_LISTENER_UP_VECTOR)
    end
end

---@param direction integer
---@return sf.Vector3f
---@diagnostic disable-next-line: unused
function GameMapPresentation:_getAudioListenerDirection(direction)
    if direction == Engine.Direction.UP then
        return LISTENER_DIRECTION_UP
    elseif direction == Engine.Direction.LEFT then
        return LISTENER_DIRECTION_LEFT
    elseif direction == Engine.Direction.RIGHT then
        return LISTENER_DIRECTION_RIGHT
    end
    return LISTENER_DIRECTION_DOWN
end

return GameMapPresentation
