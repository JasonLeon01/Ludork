---@class Source.UIBase.LazyWindow
local LazyWindow = {}

function LazyWindow:init(factory)
    self._factory = factory
    self._window = nil
end

function LazyWindow:get()
    if self._window == nil then
        local factory = assert(self._factory, "Lazy window has been disposed")
        self._window = factory()
        self._factory = nil
    end
    return self._window
end

function LazyWindow:peek()
    return self._window
end

function LazyWindow:dispose()
    self._factory = nil
    if self._window ~= nil then
        self._window:dispose()
        self._window = nil
    end
end

return class(LazyWindow)
