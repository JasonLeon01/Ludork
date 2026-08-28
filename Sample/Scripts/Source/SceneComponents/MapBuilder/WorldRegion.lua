local WorldRegionBuildState = require("Source.SceneComponents.MapBuilder.WorldRegionBuildState")

---@class (partial) Source.SceneComponents.SceneMapBuilder
local MapBuilderWorldRegion = {}

---@param worldData    Source.SceneComponents.WorldMapData
---@param region       Source.SceneComponents.WorldRegionData
---@param data         Source.SceneComponents.SerializedMapData
---@param inst         Source.GameInstance.GameInstance
---@param worldPath    string
---@param addedActors  Source.GameInstance.AddedActorRecord[]
---@param movedActors  Source.GameInstance.WorldMovedActorRecord[]
---@param priorityRect Global.WorldGeometry.CellRect | nil
---@return Global.WorldGameMap.RegionBuildState
function MapBuilderWorldRegion:createWorldRegionBuildState(
    worldData, region, data, inst, worldPath, addedActors, movedActors, priorityRect
)
    return WorldRegionBuildState.Create(
        self, worldData, region, data, inst, worldPath, addedActors, movedActors, priorityRect
    )
end

return class(MapBuilderWorldRegion)
