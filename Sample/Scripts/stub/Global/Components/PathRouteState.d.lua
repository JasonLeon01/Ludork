---@meta Global.Components.PathRouteState
---@class PathRouteState
---@field _route sf.Vector2i[]
---@field _revision integer
local PathRouteState = {}

---@return PathRouteState
function PathRouteState.new(...) end

--- @brief State container for path movement execution.
---
--- This class stores the planned path route and provides
--- cooperative single-threaded access to read and modify the route.

--- @brief Initialize the PathRouteState.
function PathRouteState:init() end

--- @brief Set a new route.
--- - route: The new path route to store.
---@param route sf.Vector2i[]
function PathRouteState:setRoute(route) end

--- @brief Clear the current route.
function PathRouteState:clear() end

--- @brief Get a copy of the current route.
---@return sf.Vector2i[] # A copy of the current route list.
function PathRouteState:getRoute() end

--- @brief Get the current route revision.
---@return integer # The revision incremented after each route change.
function PathRouteState:getRevision() end

return PathRouteState
