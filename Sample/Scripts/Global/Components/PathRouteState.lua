---@class PathRouteState
local PathRouteState = {}

function PathRouteState:init()
    self._route = {}
end

function PathRouteState:setRoute(route)
    self._route = deepcopy(route)
end

function PathRouteState:clear()
    self._route = {}
end

function PathRouteState:getRoute()
    return deepcopy(self._route)
end

return class(PathRouteState)
