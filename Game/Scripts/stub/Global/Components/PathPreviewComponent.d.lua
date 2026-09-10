---@meta Global.Components.PathPreviewComponent

--- @brief Component that renders a visual preview of the pathfinding route.
---
--- This component renders semi-transparent rectangles along the
--- planned path to show where the actor will move.

--- @brief Initialize the PathPreviewComponent.
--- - gameMap: The game map this component operates on.
--- - routeState: The path route state to read the planned path from.
---@param gameMap    GameMap
---@param routeState PathRouteState
function PathPreviewComponent:init(gameMap, routeState) end

--- @brief Render the path preview on the map.
--- - camera: The camera to use for rendering.
---@param camera GlobalCore.Camera
function PathPreviewComponent:onRender(camera) end

return PathPreviewComponent
