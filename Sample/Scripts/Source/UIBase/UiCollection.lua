local Engine = require("Engine")

---@class Source.UIBase.UiCollection
local UiCollection = {}

function UiCollection:init(container, controllerClass)
    self.container = container
    self._controllerClass = controllerClass
    self.items = {}
    self._disposers = {}
    for _, child in ipairs(container:getChildren()) do
        if child ~= nil then
            container:removeChild(child)
        end
    end
end

function UiCollection:add(model, logicalSize)
    local controller = self._controllerClass.new(model)
    self.items[#self.items + 1] = controller
    self._disposers[#self._disposers + 1] = controller.dispose
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
        local dispose = assert(self._disposers[index])
        dispose(controller)
    end
    self.items = {}
    self._disposers = {}
    self:layout()
end

function UiCollection:dispose()
    self:clear()
end

return class(UiCollection)
