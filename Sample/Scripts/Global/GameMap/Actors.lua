local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local ActorPixelShatterEffect = require("Global.CustomEffects.ActorPixelShatterEffect")

local Actor = Engine.Actor
local ComponentsFunctions = GlobalFunctions.Components

---@type GameMapImplState
local GameMapActors = {}

---@param actor Engine.Actor
local function initialiseActorCreateEvent(actor)
    Actor.BlueprintEvent(actor, Actor, "onCreate")
end

---@param actor Engine.Actor
local function initialiseActorComponents(actor)
    ComponentsFunctions.attachInstanceComponents(actor)
end

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
    local registeredLayer = self:_getRegisteredActorLayer(actor)
    if registeredLayer ~= nil then
        return registeredLayer
    end
    for layerName, actorList in pairs(self._actors) do
        if table.contains(actorList, actor) then
            return layerName
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
    ---@type table<string, Engine.Actor>
    local actorsByTag = {}
    for _, actor in ipairs(self:getAllActors()) do
        local actorTag = actor:getMapTag()
        if actorTag ~= nil and actorsByTag[actorTag] == nil then
            actorsByTag[actorTag] = actor
        end
    end
    local movedAny = false
    for actorTag, position in pairs(actorPositions) do
        local actor = actorsByTag[actorTag]
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

function GameMapActors:spawnActor(actor, layer, emitCreateEvent)
    if emitCreateEvent == nil then
        emitCreateEvent = true
    end
    self:_addActorTreeToLayer(actor, layer)
    self:_flushActorChanges()
    self._materialDirty = true
    if emitCreateEvent then
        self:initialiseActorsAndComponents()
    end
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
    self:_drainActorLifecycle(initialiseActorCreateEvent, initialiseActorComponents)
    self._materialDirty = true
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
    if self:_registerLayerActor(actor, layer) then
        self._actors[layer][#self._actors[layer] + 1] = actor
    end
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
    self:_setActorEffectHidden(actor, true)
    return true
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
    self:_syncActorViews(self._actors)
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
                        self:_setActorEffectHidden(actor, false)
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

return GameMapActors
