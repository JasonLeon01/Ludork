---@class PathRouteState
local PathRouteState = {}

function PathRouteState:init()
    self._route = {}
    self._revision = 0
end

function PathRouteState:setRoute(route)
    self._route = deepcopy(route)
    self._revision = self._revision + 1
end

function PathRouteState:clear()
    if not bool(self._route) then
        return
    end
    self._route = {}
    self._revision = self._revision + 1
end

function PathRouteState:getRoute()
    return deepcopy(self._route)
end

function PathRouteState:getRevision()
    return self._revision
end

return class(PathRouteState)
