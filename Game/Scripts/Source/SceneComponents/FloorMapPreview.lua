local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local RegionTerrain = require("Global.GameMap.RegionTerrain")
local Data = require("Source.Data")
local ActorVisibility = require("Source.Utils.ActorVisibility")

local FloorMapPreview = {}

local function drawLayerEffects(_layerName) end

function FloorMapPreview:init(tilemap, data, state)
    self._terrain = RegionTerrain.new(tilemap, Data.GetAutoTile)
    self._map = GlobalCore.GameMapBase.new()
    self._map:setTilemap(tilemap)
    self._renderer = GlobalCore.GameMapRenderer.new(self._map, tilemap, nil, data.layerOrder, 255, true)
    self._sprites = {}
    self._appearances = {}
    self._conditionVariables = {}
    self._tags = {}
    local destroyed = {}
    for _, tag in ipairs(state.destroyedActors) do
        destroyed[tag] = true
    end
    local excluded = {}
    for _, tag in ipairs(state.excludedActors) do
        excluded[tag] = true
    end
    for _, layerName in ipairs(data.layerOrder) do
        for _, actor in ipairs(data.actors[layerName] or {}) do
            if not excluded[actor.tag] then
                local changes = data.BPClassVarChanged ~= nil and data.BPClassVarChanged[tostring(actor.tag or "")]
                    or nil
                self:_addRecord(
                    actor.bp, actor.tag, layerName, actor.position, changes, destroyed, state.actorPositions
                )
            end
        end
    end
    for _, actor in ipairs(state.addedActors) do
        if not self._tags[actor.tag] then
            self:_addRecord(
                actor.bp, actor.tag, actor.layer, actor.position, actor.classVarChanges, destroyed, state.actorPositions
            )
        end
    end
    self._renderer:setPreviewSprites(self._sprites)
end

function FloorMapPreview:_addRecord(classPath, tag, layer, position, changes, destroyed, positions)
    tag = tag or ""
    if destroyed[tag] then
        return
    end
    local appearance = Data.GetActorPreviewData(classPath, changes)
    if appearance == nil then
        return
    end
    local restored = positions[tag] or position
    local pixelPosition = sf.Vector2f.new(restored.x * Engine.GetCellSize(), restored.y * Engine.GetCellSize())
    self:_append(appearance, tag, layer, pixelPosition, -1, destroyed, positions)
end

function FloorMapPreview:_append(appearance, tag, layer, position, parentIndex, destroyed, positions)
    if destroyed[tag] then
        return
    end
    local restored = positions[tag]
    if restored ~= nil then
        position = sf.Vector2f.new(restored.x * Engine.GetCellSize(), restored.y * Engine.GetCellSize())
    end
    local texture = appearance.texturePath ~= "" and GlobalCore.TextureManager.load(appearance.texturePath) or nil
    local sprite = GlobalCore.PreviewSprite.new({
        layer = layer,
        texture = texture,
        rect = appearance.rect,
        position = position,
        mapPosition = sf.Vector2i.new(
            math.trunc(position.x / Engine.GetCellSize() + 0.5), math.trunc(position.y / Engine.GetCellSize() + 0.5)
        ),
        translation = appearance.translation,
        rotation = appearance.rotation,
        scale = appearance.scale,
        origin = appearance.origin,
        visible = appearance.visible,
        hue = appearance.hue,
        shaderPath = appearance.shaderPath,
        parentIndex = parentIndex
    })
    local index = #self._sprites
    self._sprites[index + 1] = sprite
    self._appearances[index + 1] = appearance
    self._tags[tag] = true
    if appearance.conditionVariable ~= "" then
        self._conditionVariables[appearance.conditionVariable] = true
    end
    if appearance.child ~= nil then
        self:_append(
            appearance.child, tag .. "_child", layer, position + appearance.childOffset, index, destroyed, positions
        )
    end
end

function FloorMapPreview:applyConditions(variables)
    local visibility = {}
    for index, appearance in ipairs(self._appearances) do
        visibility[index] = ActorVisibility.Evaluate(
            appearance.conditionVariable, appearance.conditionOperator, appearance.conditionValue, appearance.visible,
            variables
        )
    end
    self._renderer:setPreviewVisibility(visibility)
end

function FloorMapPreview:getConditionVariables()
    return self._conditionVariables
end

function FloorMapPreview:setHideDisconnectedRegions(enabled)
    self._map:setHideDisconnectedRegions(enabled)
end

function FloorMapPreview:setVisibilityObserver(position)
    self._map:setVisibilityObserver(position)
end

function FloorMapPreview:applyTerrainDestructions(changes)
    self._terrain:applyTerrainDestructions(changes)
end

function FloorMapPreview:drawMapContent(target, states)
    self._renderer:drawContent(target, states, false, 0, 0, drawLayerEffects)
end

return class(FloorMapPreview)
