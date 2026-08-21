local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local ActorPixelShatterEffect = require("Global.CustomEffects.ActorPixelShatterEffect")

local Actor = Engine.Actor
local ComponentsFunctions = GlobalFunctions.Components

---@class (partial) GameMap
local GameMapActors = {}

function GameMapActors:_syncActorsForMapCache()
    for _, actor in ipairs(self:getAllActors()) do
        actor:syncMapCache()
        actor:refreshDescendantCache()
    end
end

function GameMapActors:_syncActorsForPathfinding()
    for _, actor in ipairs(self:getAllActors()) do
        actor:setPathfindingBlocks(Actor.HasBlueprintEvent(actor, "onOverlap") and not actor:getCollisionEnabled())
    end
end

---@param fromPosition sf.Vector2i
---@param toPosition   sf.Vector2i
---@param direction    integer
---@return boolean
function GameMapActors:_checkDir4Between(fromPosition, toPosition, direction)
    local oppositeDirection = Engine.OppositeDirection(direction)
    local fromBlocked = false
    local toBlocked = false
    for _, layer in ipairs(self._layersTopFirst) do
        if layer.visible then
            if not fromBlocked then
                local tileFrom = layer:get(fromPosition)
                if tileFrom ~= nil then
                    if not layer:isDirectionPassable(fromPosition, direction) then
                        return false
                    end
                    fromBlocked = true
                end
            end
            if not toBlocked then
                local tileTo = layer:get(toPosition)
                if tileTo ~= nil then
                    if not layer:isDirectionPassable(toPosition, oppositeDirection) then
                        return false
                    end
                    toBlocked = true
                end
            end
            if fromBlocked and toBlocked then
                break
            end
        end
    end
    return true
end

function GameMapActors:getAllActors()
    local actors = {}
    for _, actorList in pairs(self._actors) do
        for _, actor in ipairs(actorList) do
            actors[#actors + 1] = actor
        end
    end
    return actors
end

function GameMapActors:getActorLayer(actor)
    for layerName, actorList in pairs(self._actors) do
        for _, listed in ipairs(actorList) do
            if listed == actor then
                return layerName
            end
        end
    end
    for layerName, actorList in pairs(self._wholeActorList) do
        for _, listed in ipairs(actorList) do
            if listed == actor then
                return layerName
            end
        end
    end
    return nil
end

function GameMapActors:getActorsByPosition(position)
    if self._tilePassableGrid == nil or self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    return self:getActorsAt(position.x, position.y)
end

function GameMapActors:getActorByLayerAndPosition(layer, position)
    for _, actor in ipairs(self._actors[layer] or {}) do
        if actor:getPosition() == position then
            return actor
        end
    end
    return nil
end

function GameMapActors:getActorsByRange(position, radius)
    if self._tilePassableGrid == nil or self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    return self:getActorsInRange(position.x, position.y, radius)
end

function GameMapActors:getActorByTag(tag)
    for _, actorList in pairs(self._actors) do
        for _, actor in ipairs(actorList) do
            if actor:getMapTag() == tag then
                return actor
            end
        end
    end
    return nil
end

function GameMapActors:getAllActorsByTag(tag)
    local actor = self:getActorByTag(tag)
    return actor == nil and {} or { actor }
end

function GameMapActors:removeActorsByTags(tags)
    if not bool(tags) then
        return
    end
    local tagSet = {}
    for _, tag in ipairs(tags) do
        tagSet[tag] = true
    end
    local actorsToRemove = {}
    for _, actorList in pairs(self._actors) do
        for _, actor in ipairs(actorList) do
            if tagSet[actor:getMapTag()] then
                actorsToRemove[actor] = true
                for descendant in pairs(self:_getDescendantActorIDs(actor)) do
                    actorsToRemove[descendant] = true
                end
            end
        end
    end
    if not bool(actorsToRemove) then
        return
    end
    local removed = false
    for layerName, actorList in pairs(self._actors) do
        local keptActors = {}
        for _, actor in ipairs(actorList) do
            if actorsToRemove[actor] then
                removed = true
            else
                keptActors[#keptActors + 1] = actor
            end
        end
        self._actors[layerName] = keptActors
    end
    if removed then
        self:updateActorList()
        self._materialDirty = true
    end
end

function GameMapActors:applyActorPositions(actorPositions)
    if actorPositions == nil then
        return
    end
    local movedAny = false
    for actorTag, position in pairs(actorPositions) do
        local actor = self:getActorByTag(actorTag)
        if actor ~= nil then
            actor:setMapPosition(position)
            movedAny = true
        end
    end
    if movedAny then
        self:updateActorList()
        self:markPassabilityDirty()
    end
end

function GameMapActors:isPassable(actor, targetPosition)
    if not actor:getCollisionEnabled() then
        return true
    end
    local size = self._tilemap:getSize()
    if self._tilePassableGrid == nil or self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    ---@type sf.Vector2i[]
    local occupied = actor:getOccupiedMapCellsAtMapPosition(targetPosition)
    for _, cell in ipairs(occupied) do
        if cell.x < 0 or cell.y < 0 or cell.x >= size.x or cell.y >= size.y then
            return false
        end
        if self._tilePassableGrid ~= nil then
            if not assert(self._tilePassableGrid[cell.y + 1])[cell.x + 1] then
                return false
            end
        end
    end
    local currentPosition = actor:getMapPosition()
    local delta = sf.Vector2i.new(targetPosition.x - currentPosition.x, targetPosition.y - currentPosition.y)
    local direction = nil
    if delta.x == 0 and delta.y == 1 then
        direction = Engine.Direction.DOWN
    elseif delta.x == 0 and delta.y == -1 then
        direction = Engine.Direction.UP
    elseif delta.x == 1 and delta.y == 0 then
        direction = Engine.Direction.RIGHT
    elseif delta.x == -1 and delta.y == 0 then
        direction = Engine.Direction.LEFT
    end
    if direction ~= nil then
        ---@type dict<tuple<any>, boolean>
        local currentCells = dict()
        for _, cell in ipairs(actor:getOccupiedMapCellsAtMapPosition(currentPosition)) do
            currentCells[tuple { cell.x, cell.y }] = true
        end
        for _, cell in ipairs(occupied) do
            if not currentCells:get(tuple { cell.x, cell.y }) then
                local previousX = cell.x - delta.x
                local previousY = cell.y - delta.y
                local previousPosition = sf.Vector2i.new(previousX, previousY)
                local occupiedPosition = sf.Vector2i.new(cell.x, cell.y)
                ---@cast previousPosition sf.Vector2i
                ---@cast occupiedPosition sf.Vector2i
                if not self:_checkDir4Between(previousPosition, occupiedPosition, direction) then
                    return false
                end
            end
        end
    end
    for _, cell in ipairs(occupied) do
        if bool(self:getCollisionAt(cell.x, cell.y, actor)) then
            return false
        end
    end
    return true
end

function GameMapActors:spawnActor(actor, layer, emitCreateEvent)
    if emitCreateEvent == nil then
        emitCreateEvent = true
    end
    self:_addActorTreeToLayer(actor, layer)
    if self._actorBatchDepth == 0 and not self._initialisingActors then
        self:updateActorList()
    end
    self._materialDirty = true
    if emitCreateEvent and self._actorBatchDepth == 0 and not self._initialisingActors then
        self:initialiseActorsAndComponents()
    end
end

function GameMapActors:beginActorBatch()
    self._actorBatchDepth = self._actorBatchDepth + 1
end

function GameMapActors:endActorBatch()
    assert(self._actorBatchDepth > 0, "Actor batch is not active")
    self._actorBatchDepth = self._actorBatchDepth - 1
    if self._actorBatchDepth > 0 then
        return
    end
    self:updateActorList()
end

function GameMapActors:createActor(actorClass, layer, kwargs, emitCreateEvent)
    if emitCreateEvent == nil then
        emitCreateEvent = true
    end
    local actor = Class.constructNamed(actorClass, kwargs or {})
    if actor.material ~= nil and not Class.isInstance(actor.material, Engine.Material) then
        local values = Engine.filterDataClassParams(actor.material, Engine.Material)
        actor.material = Engine.Material.new(
            values.lightBlock, values.mirror, values.reflectionStrength, values.opacity, values.speedRate,
            values.ignoreLighting
        )
    end
    self:spawnActor(actor, layer, emitCreateEvent)
    return actor
end

function GameMapActors:initialiseActorsAndComponents()
    if self._initialisingActors then
        return
    end
    self._initialisingActors = true
    while true do
        local createdAny = self:_initialisePendingActorCreateEvents()
        local componentAny = self:_initialisePendingActorComponents()
        if not createdAny and not componentAny then
            break
        end
    end
    self._initialisingActors = false
    self:updateActorList()
    self._materialDirty = true
end

---@return boolean
function GameMapActors:_initialisePendingActorCreateEvents()
    local createdAny = false
    while true do
        local pendingActors = {}
        for _, actor in ipairs(self:getAllActors()) do
            if not self._createInitialisedActorIDs[actor] and not actor:isDestroyed() then
                pendingActors[#pendingActors + 1] = actor
            end
        end
        if not bool(pendingActors) then
            return createdAny
        end
        for _, actor in ipairs(pendingActors) do
            if not self._createInitialisedActorIDs[actor] then
                self._createInitialisedActorIDs[actor] = true
                Actor.BlueprintEvent(actor, Actor, "onCreate")
                createdAny = true
            end
        end
    end
    return createdAny
end

---@return boolean
function GameMapActors:_initialisePendingActorComponents()
    local componentAny = false
    local pendingActors = {}
    for _, actor in ipairs(self:getAllActors()) do
        if not self._componentInitialisedActorIDs[actor] and not actor:isDestroyed() then
            pendingActors[#pendingActors + 1] = actor
        end
    end
    for _, actor in ipairs(pendingActors) do
        if not self._componentInitialisedActorIDs[actor] then
            self._componentInitialisedActorIDs[actor] = true
            ComponentsFunctions.attachInstanceComponents(actor)
            componentAny = true
        end
    end
    return componentAny
end

---@param actor Engine.Actor
---@param layer string
function GameMapActors:_addActorTreeToLayer(actor, layer)
    self:_addActorToLayer(actor, layer)
    for _, child in ipairs(actor:getChildren()) do
        self:_addActorTreeToLayer(child, layer)
    end
end

---@param actor Engine.Actor
---@param layer string
function GameMapActors:_addActorToLayer(actor, layer)
    if self._actors[layer] == nil then
        self._actors[layer] = {}
    end
    actor:setMap(self)
    actor:ensureMapTag()
    for _, listed in ipairs(self._actors[layer]) do
        if listed == actor then
            return
        end
    end
    self._actors[layer][#self._actors[layer] + 1] = actor
end

function GameMapActors:destroyActor(actor)
    self._actorsOnDestroy[#self._actorsOnDestroy + 1] = actor
    self._materialDirty = true
end

function GameMapActors:playActorPixelShatterEffect(actor)
    if self._previewOnly or self._actorPixelShatterShader == nil
        or actor:isDestroyed() or self._actorPixelShatterByActor[actor] ~= nil then
        return false
    end
    local layerName = self:getActorLayer(actor)
    if layerName == nil then
        return false
    end
    local layer = self._tilemap:getLayer(layerName)
    if layer == nil or not layer.visible then
        return false
    end
    self._actorPixelShatterSeed = self._actorPixelShatterSeed + 1
    local effect = ActorPixelShatterEffect.new(actor, self._actorPixelShatterShader, self._actorPixelShatterSeed)
    if self._actorPixelShatterEffects[layerName] == nil then
        self._actorPixelShatterEffects[layerName] = {}
    end
    local effectIndex = #self._actorPixelShatterEffects[layerName] + 1
    self._actorPixelShatterEffects[layerName][effectIndex] = effect
    self._actorPixelShatterByActor[actor] = effect
    return true
end

function GameMapActors:getTopMaterial(pos)
    for index = #self._layerNames, 1, -1 do
        local layer = self._tilemap:getLayer(self._layerNames[index])
        if layer ~= nil and layer.visible then
            for _, actor in ipairs(self._actors[self._layerNames[index]] or {}) do
                if actor ~= self._player and actor:getMapPosition() == pos then
                    return actor:getMaterial()
                end
            end
            local material = layer:getMaterial(pos)
            if material ~= nil then
                return material
            end
        end
    end
    return nil
end

function GameMapActors:findPathResult(start, goal, actor, excludedAnchors)
    self:_syncActorsForPathfinding()
    local result = self:findPathExt(start, goal, self._tilemap:getSize(), actor, excludedAnchors or {})
    self:_clearActorsPathfindingBlocks()
    return result
end

function GameMapActors:_clearActorsPathfindingBlocks()
    for _, actor in ipairs(self:getAllActors()) do
        actor:setPathfindingBlocks(false)
    end
end

function GameMapActors:findPath(start, goal, actor, excludedAnchors)
    return self:findPathResult(start, goal, actor, excludedAnchors).offsets
end

function GameMapActors:isPathfindingPassable(actor, targetPosition)
    if not self:isPassable(actor, targetPosition) then
        return false
    end
    return not self:hasPathBlockingOverlapActor(actor, targetPosition)
end

function GameMapActors:hasPathBlockingOverlapActor(actor, targetPosition)
    if self._tilePassableGrid == nil or self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    for _, cell in ipairs(actor:getOccupiedMapCellsAtMapPosition(targetPosition)) do
        for _, other in ipairs(self:getOverlapsAt(cell.x, cell.y, actor)) do
            if Actor.HasBlueprintEvent(other, "onOverlap") and not other:getCollisionEnabled() then
                return true
            end
        end
    end
    return false
end

function GameMapActors:getCollision(actor, targetPosition)
    if not actor:getCollisionEnabled() then
        return {}
    end
    if self._tilePassableGrid == nil or self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    local collisions = {}
    local seen = {}
    for _, cell in ipairs(actor:getOccupiedMapCellsAtMapPosition(targetPosition)) do
        for _, other in ipairs(self:getCollisionAt(cell.x, cell.y, actor)) do
            if not seen[other] then
                seen[other] = true
                collisions[#collisions + 1] = other
            end
        end
    end
    return collisions
end

function GameMapActors:getOverlaps(actor)
    if self._tilePassableGrid == nil or self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    local overlaps = {}
    local seen = {}
    for _, cell in ipairs(actor:getOccupiedMapCells()) do
        for _, other in ipairs(self:getOverlapsAt(cell.x, cell.y, actor)) do
            if not seen[other] then
                seen[other] = true
                overlaps[#overlaps + 1] = other
            end
        end
    end
    return overlaps
end

---@param actor Engine.Actor
---@return table<Engine.Actor, boolean>
---@diagnostic disable-next-line: unused
function GameMapActors:_getDescendantActorIDs(actor)
    local descendantActors = {}
    local stack = {}
    for _, child in ipairs(actor:getChildren()) do
        stack[#stack + 1] = child
    end
    while bool(stack) do
        local child = table.remove(stack)
        if not descendantActors[child] then
            descendantActors[child] = true
            for _, nested in ipairs(child:getChildren()) do
                stack[#stack + 1] = nested
            end
        end
    end
    return descendantActors
end

function GameMapActors:updateActorList()
    self._wholeActorList = {}
    self._actorUpdateList = {}
    for layerName, actorList in pairs(self._actors) do
        self._wholeActorList[layerName] = {}
        ---@type Engine.Actor[]
        local queue = {}
        for _, actor in ipairs(actorList) do
            queue[#queue + 1] = actor
            self._actorUpdateList[#self._actorUpdateList + 1] = actor
        end
        local index = 1
        while index <= #queue do
            local child = queue[index]
            ---@cast child Engine.Actor
            index = index + 1
            child:setMap(self)
            self._wholeActorList[layerName][#self._wholeActorList[layerName] + 1] = child
            for _, nested in ipairs(child:getChildren()) do
                queue[#queue + 1] = nested
            end
        end
    end
    self:syncActorsRef(self._wholeActorList)
    self:syncMaterialActorsRef(self._actors)
    self._actorUpdateBatch:syncActors(self._actorUpdateList)
end

function GameMapActors:_updateActorPixelShatterEffects(deltaTime)
    for layerName, effects in pairs(self._actorPixelShatterEffects) do
        local activeEffects = {}
        for _, effect in ipairs(effects) do
            effect:onTick(deltaTime)
            if effect:isFinished() then
                for actor, actorEffect in pairs(self._actorPixelShatterByActor) do
                    if actorEffect == effect then
                        self._actorPixelShatterByActor[actor] = nil
                        break
                    end
                end
            else
                activeEffects[#activeEffects + 1] = effect
            end
        end
        if bool(activeEffects) then
            self._actorPixelShatterEffects[layerName] = activeEffects
        else
            self._actorPixelShatterEffects[layerName] = nil
        end
    end
end

return class(GameMapActors)
