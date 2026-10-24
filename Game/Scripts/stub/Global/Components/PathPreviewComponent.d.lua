---@meta Global.Components.PathPreviewComponent

---@class Global.Components.PathPreviewComponent: ComponentBase
---@field new                             fun(gameMap: GameMap, routeState: PathRouteState): Global.Components.PathPreviewComponent
---@field private _DEFAULT_FILL_COLOUR    sf.Color
---@field private _DEFAULT_OUTLINE_COLOUR sf.Color
---@field private _CELL_PADDING_RATIO     number
---@field private _MINIMUM_PADDING        number
---@field private _OUTLINE_THICKNESS      number
local PathPreviewComponent = {}

---@brief Component that renders a visual preview of the pathfinding route.
---
--- This component renders semi-transparent rectangles along the
--- planned path to show where the actor will move.

---@brief Initialize the PathPreviewComponent.
--- - gameMap: The game map this component operates on.
--- - routeState: The path route state to read the planned path from.
---@param gameMap    GameMap
---@param routeState PathRouteState
function PathPreviewComponent:init(gameMap, routeState) end

---@brief Render the path preview on the map.
--- - camera: The camera to use for rendering.
---@param camera GlobalCore.Camera
function PathPreviewComponent:onRender(camera) end

return PathPreviewComponent
