local WorldRegionBuildState = require("Source.SceneComponents.MapBuilder.WorldRegionBuildState")

local MapBuilderWorldRegion = {}

---@param worldData    Global.WorldGameMap.WorldMapData
---@param region       Global.WorldGameMap.WorldRegionData
---@param data         Global.GameMap.SerializedMapData
---@param inst         Source.GameInstance.GameInstance
---@param worldPath    string
---@param addedActors  Source.GameInstance.AddedActorRecord[]
---@param movedActors  Source.GameInstance.WorldMovedActorRecord[]
---@param priorityRect Global.WorldGeometry.CellRect | nil
---@return Source.SceneComponents.MapBuilder.WorldRegionBuildState.State
---@param self         Source.SceneComponents.SceneMapBuilder
function MapBuilderWorldRegion.CreateWorldRegionBuildState(
    self, worldData, region, data, inst, worldPath, addedActors, movedActors, priorityRect
)
    return WorldRegionBuildState.Create(
        self, worldData, region, data, inst, worldPath, addedActors, movedActors, priorityRect
    )
end

return MapBuilderWorldRegion
