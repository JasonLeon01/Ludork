---@class Internal.UIBase.UiObserver<T: Internal.UIBase.UiController>
local UiObserver = {}

local nextIdentifier = 0

function UiObserver:init(controller)
    self._controller = setmetatable({ controller }, { __mode = "v" })
    self._stops = {}
end

function UiObserver:watch(target, field, method, immediate)
    local controller = self._controller[1]
    if controller == nil then
        error("UI observations require a live Controller")
    end
    local callback = controller:bindCallback(method)
    ---@type { observer: Internal.UIBase.UiObserver<T> | nil, target: table | userdata | nil, controller: T | nil }
    local references = setmetatable({ observer = self, target = target, controller = controller }, { __mode = "v" })
    nextIdentifier = nextIdentifier + 1
    local identifier = "UiObserver:" .. tostring(nextIdentifier)
    local active = true

    local function stop()
        if not active then
            return
        end
        active = false
        local observer = references.observer
        local observed = references.target
        if observer ~= nil then
            observer._stops[stop] = nil
        end
        references.observer = nil
        references.target = nil
        references.controller = nil
        if observed ~= nil then
            Class.unmonitor(observed, field, identifier)
        end
    end

    Class.monitor(target, field, function (oldValue, newValue)
        if not active then
            return
        end
        if references.observer == nil or references.controller == nil then
            stop()
            return
        end
        callback(newValue, oldValue)
    end, nil, false, identifier
    )
    self._stops[stop] = true
    if immediate ~= false then
        callback(target[field], Class.MISSING)
    end
    return stop
end

function UiObserver:dispose()
    self._controller[1] = nil
    for stop in pairs(self._stops) do
        stop()
    end
end

return class(UiObserver)
