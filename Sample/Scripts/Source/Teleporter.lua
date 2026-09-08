local Engine = require("Engine")
local MapPath = require("Source.MapPath")
local GameplayScene = require("Source.Gameplay.GameplayScene")

local Actor = Engine.Actor

local Teleporter = {}

Teleporter.Offset = sf.Vector2i.new(0, 0)
Teleporter.stairSE = ""
Teleporter.transitionName = ""
Teleporter.transitionTime = 0.5

function Teleporter:init(texture, rect, tag)
    super(Teleporter, self).init(texture, rect, tag)
    self._floorTransferPending = false
end

function Teleporter:goUpstairs()
    self:_goFloor(1)
end

function Teleporter:goDownstairs()
    self:_goFloor(-1)
end

function Teleporter:getTeleportPosition()
    local position = self:getMapPosition()
    return sf.Vector2i.new(position.x + self.Offset.x, position.y + self.Offset.y)
end

---@param step integer
function Teleporter:_goFloor(step)
    local map = self:getMap()
    if self._floorTransferPending or map == nil then
        return
    end
    local scene = map:getScene()
    if not Class.isInstance(scene, GameplayScene) then
        return
    end
    ---@cast scene Source.Gameplay.GameplayScene
    ---@cast self Source.Teleporter.Teleporter
    self._floorTransferPending = true
    if not scene:requestFloorStep(self, step) then
        self._floorTransferPending = false
    end
end

---@param actors   Engine.Actor[]
---@param position sf.Vector2i
---@return Source.Teleporter.Teleporter | nil
function Teleporter.FindNearestTeleporter(actors, position)
    local nearest = nil
    local nearestDistance = nil
    for _, actor in ipairs(actors) do
        if Class.isInstance(actor, Teleporter) and not actor:isDestroyed() then
            local actorPosition = actor:getMapPosition()
            local dx = actorPosition.x - position.x
            local dy = actorPosition.y - position.y
            local distance = dx * dx + dy * dy
            if nearestDistance == nil or distance < nearestDistance then
                nearest = actor
                nearestDistance = distance
            end
        end
    end
    return nearest
end

function Teleporter.IsAsideOrOverlapping(actors, position)
    local nearest = Teleporter.FindNearestTeleporter(actors, position)
    if nearest == nil then
        return false
    end
    local actorPosition = nearest:getMapPosition()
    return Engine.ManhattanDistance(actorPosition, position) <= 1
end

---@param regionMaps string[]
---@param currentMap string
---@return integer | nil
function Teleporter.FindCurrentMapIndex(regionMaps, currentMap)
    local currentName = MapPath.BasenameWithoutExtension(currentMap)
    for index, mapPath in ipairs(regionMaps) do
        if MapPath.BasenameWithoutExtension(mapPath) == currentName then
            return index
        end
    end
    return nil
end

return class(Teleporter, Actor)
