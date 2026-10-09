---@meta Source.SceneComponents.FloorMapPreview

---@class Source.SceneComponents.FloorMapPreviewState
---@field addedActors     Source.GameInstance.AddedActorRecord[]
---@field actorPositions  table<string, sf.Vector2i>
---@field destroyedActors string[]
---@field excludedActors  string[]

--- Static map content without gameplay Actor instances or lifecycle callbacks.
---@class Source.SceneComponents.FloorMapPreview
---@field _terrain            Global.GameMap.RegionTerrain
---@field _map                GlobalCore.GameMapBase
---@field _renderer           GlobalCore.GameMapRenderer
---@field _sprites            GlobalCore.PreviewSprite[]
---@field _appearances        Source.Data.ActorPreviewData[]
---@field _conditionVariables table<string, boolean>
---@field _tags               table<string, boolean>
---@field new                 fun(tilemap: Engine.Tilemap, data: Source.SceneComponents.MapData, state: Source.SceneComponents.FloorMapPreviewState): Source.SceneComponents.FloorMapPreview
local FloorMapPreview = {}

---@param tilemap Engine.Tilemap
---@param data    Source.SceneComponents.MapData
---@param state   Source.SceneComponents.FloorMapPreviewState
function FloorMapPreview:init(tilemap, data, state) end

---@param classPath string
---@param tag       string | nil
---@param layer     string
---@param position  sf.Vector2i | sf.Vector2u
---@param changes   table<string, Source.Data.ClassVarValue> | nil
---@param destroyed table<string, boolean>
---@param positions table<string, sf.Vector2i>
function FloorMapPreview:_addRecord(classPath, tag, layer, position, changes, destroyed, positions) end

---@param appearance  Source.Data.ActorPreviewData
---@param tag         string
---@param layer       string
---@param position    sf.Vector2f
---@param parentIndex integer
---@param destroyed   table<string, boolean>
---@param positions   table<string, sf.Vector2i>
function FloorMapPreview:_append(appearance, tag, layer, position, parentIndex, destroyed, positions) end

---@param variables table<string, Source.GameInstance.RecordValue>
function FloorMapPreview:applyConditions(variables) end

---@return table<string, boolean>
function FloorMapPreview:getConditionVariables() end

---@param enabled boolean
function FloorMapPreview:setHideDisconnectedRegions(enabled) end

---@param position sf.Vector2i
function FloorMapPreview:setVisibilityObserver(position) end

---@param changes table<string, table<string, { position: sf.Vector2i, tileID: Global.GameMap.TerrainTileID }>>
function FloorMapPreview:applyTerrainDestructions(changes) end

---@param target sf.RenderTarget
---@param states sf.RenderStates
function FloorMapPreview:drawMapContent(target, states) end

return FloorMapPreview
