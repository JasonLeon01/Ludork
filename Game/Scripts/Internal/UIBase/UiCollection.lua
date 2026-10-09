local Engine = require("Engine")

---@class Internal.UIBase.UiCollection
local UiCollection = {}

local MAX_IDLE_ROWS = 64

function UiCollection:init(container, controllerClass)
    self.container = container
    self._controllerClass = controllerClass
    self.items = {}
    self._disposers = {}
    self._idle = {}
    self._idleDisposers = {}
    self._disposed = false
    for _, child in ipairs(container:getChildren()) do
        if child ~= nil then
            container:removeChild(child)
        end
    end
end

function UiCollection:add(model, logicalSize)
    assert(not self._disposed, "Disposed UiCollection cannot add rows")
    local index = #self._idle
    local controller = self._idle[index]
    local dispose = self._idleDisposers[index]
    if controller ~= nil then
        assert(dispose ~= nil)
        self._idle[index] = nil
        self._idleDisposers[index] = nil
        controller:reuse(model)
    else
        controller = self._controllerClass.new(model)
        dispose = controller.dispose
    end
    self.items[#self.items + 1] = controller
    self._disposers[#self._disposers + 1] = dispose
    controller:attachTo(self.container, logicalSize)
    return controller
end

function UiCollection:layout()
    if Class.isInstance(self.container, Engine.ListView) then
        ---@cast self.container Engine.ListView
        self.container:applyPositions()
    end
end

function UiCollection:clear()
    for index, controller in ipairs(self.items) do
        local dispose = self._disposers[index]
        assert(dispose ~= nil)
        if #self._idle < MAX_IDLE_ROWS then
            controller:releaseForReuse()
            self._idle[#self._idle + 1] = controller
            self._idleDisposers[#self._idleDisposers + 1] = dispose
        else
            dispose(controller)
        end
    end
    self.items = {}
    self._disposers = {}
    self:layout()
end

function UiCollection:dispose()
    if self._disposed then
        return
    end
    self._disposed = true
    for index, controller in ipairs(self.items) do
        local dispose = assert(self._disposers[index])
        dispose(controller)
    end
    for index, controller in ipairs(self._idle) do
        local dispose = assert(self._idleDisposers[index])
        dispose(controller)
    end
    self.items = {}
    self._disposers = {}
    self._idle = {}
    self._idleDisposers = {}
    self:layout()
end

return class(UiCollection)
