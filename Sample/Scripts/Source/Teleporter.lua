local Engine = require("Engine")
require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local RegionDict = require("Source.Configs.RegionDict")
local MapPath = require("Source.MapPath")

local Actor = Engine.Actor
local ManagerFunctions = GlobalFunctions.Manager
---@type function
local normaliseMapName

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
    local MapScene = require("Source.Scenes.SceneMap")

    local map = self:getMap()
    if self._floorTransferPending or map == nil then
        return
    end
    local scene = map:getScene()
    if scene == nil then
        return
    end
    if not Class.isInstance(scene, MapScene) then
        return
    end
    local inst = scene.inst
    local regionMaps = RegionDict[inst:getCurrentRegion()] or {}
    local currentMap = scene._cachedMapFile
    if not bool(currentMap) then
        return
    end
    local currentIndex = Teleporter.FindCurrentMapIndex(regionMaps, currentMap)
    if currentIndex == nil then
        return
    end
    local targetIndex = currentIndex + step
    if targetIndex < 1 or targetIndex > #regionMaps then
        return
    end

    local player = map:getPlayer()
    if player == nil then
        return
    end
    local sourceTeleporter = Teleporter.FindNearestTeleporter(map:getAllActors(), player:getMapPosition())
    if sourceTeleporter == nil then
        return
    end
    local anchorPosition = sourceTeleporter:getTeleportPosition()
    inst:recordTelepoint(currentMap, sf.Vector2u.new(anchorPosition.x, anchorPosition.y))

    local targetMap = scene:resolveRegionMapPath(regionMaps[targetIndex])
    local moveEnabled = player:getMoveEnabled()
    player:setMoveEnabled(false)
    if scene:requestFloorTransfer(targetMap, anchorPosition, moveEnabled) then
        ManagerFunctions.playSE(self.stairSE)
        self._floorTransferPending = true
        return
    end
    player:setMoveEnabled(moveEnabled)
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
    return math.abs(actorPosition.x - position.x) + math.abs(actorPosition.y - position.y) <= 1
end

---@param regionMaps string[]
---@param currentMap string
---@return integer | nil
function Teleporter.FindCurrentMapIndex(regionMaps, currentMap)
    local currentName = normaliseMapName(currentMap)
    for index, mapPath in ipairs(regionMaps) do
        if normaliseMapName(mapPath) == currentName then
            return index
        end
    end
    return nil
end

---@param mapPath string
---@return string
function normaliseMapName(mapPath)
    local path = MapPath.Normalise(mapPath)
    path = path:match("([^/]+)$") or path
    return path:gsub("%.[^%.]+$", "")
end

return class(Teleporter, Actor)
