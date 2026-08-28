---@meta Source.SceneComponents.MapBuilder.WorldRegionBuildState

local WorldRegionBuildState = {}

---@param builder      Source.SceneComponents.SceneMapBuilder
---@param worldData    Source.SceneComponents.WorldMapData
---@param region       Source.SceneComponents.WorldRegionData
---@param data         Source.SceneComponents.SerializedMapData
---@param inst         Source.GameInstance.GameInstance
---@param worldPath    string
---@param addedActors  Source.GameInstance.AddedActorRecord[]
---@param movedActors  Source.GameInstance.WorldMovedActorRecord[]
---@param priorityRect Global.WorldGeometry.CellRect | nil
---@return Global.WorldGameMap.RegionBuildState
function WorldRegionBuildState.Create(
    builder, worldData, region, data, inst, worldPath, addedActors, movedActors, priorityRect
) end

return WorldRegionBuildState
