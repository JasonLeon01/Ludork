---@meta Source.SceneComponents.MapBuilder.WorldActorRecords

local WorldActorRecords = {}

---@param worldData    Global.WorldGameMap.WorldMapData
---@param inst         Source.GameInstance.GameInstance
---@param worldPath    string
---@param targetRegion Global.WorldGameMap.WorldRegionData | nil
---@return Source.GameInstance.AddedActorRecord[]
function WorldActorRecords.SelectAdded(worldData, inst, worldPath, targetRegion) end

---@param worldData Global.WorldGameMap.WorldMapData
---@param inst      Source.GameInstance.GameInstance
---@param worldPath string
---@return Source.GameInstance.WorldMovedActorRecord[]
function WorldActorRecords.CollectMoved(worldData, inst, worldPath) end

---@param inst      Source.GameInstance.GameInstance
---@param worldPath string
---@return string[]
function WorldActorRecords.CollectReservedTags(inst, worldPath) end

return WorldActorRecords
