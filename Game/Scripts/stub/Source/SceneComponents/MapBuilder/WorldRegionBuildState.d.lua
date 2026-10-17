---@meta Source.SceneComponents.MapBuilder.WorldRegionBuildState

---@class Source.SceneComponents.MapBuilder.WorldRegionBuildState.State: Global.WorldGameMap.RegionBuildState
---@field layerBuildStates Source.SceneComponents.WorldLayerBuildState[] | nil

local WorldRegionBuildState = {}

---@param builder      Source.SceneComponents.SceneMapBuilder
---@param worldData    Global.WorldGameMap.WorldMapData
---@param region       Global.WorldGameMap.WorldRegionData
---@param data         Global.GameMap.SerializedMapData
---@param inst         Source.GameInstance.GameInstance
---@param worldPath    string
---@param addedActors  Source.GameInstance.AddedActorRecord[]
---@param movedActors  Source.GameInstance.WorldMovedActorRecord[]
---@param priorityRect Global.WorldGeometry.CellRect | nil
---@return Source.SceneComponents.MapBuilder.WorldRegionBuildState.State
function WorldRegionBuildState.Create(
    builder, worldData, region, data, inst, worldPath, addedActors, movedActors, priorityRect
) end

return WorldRegionBuildState
