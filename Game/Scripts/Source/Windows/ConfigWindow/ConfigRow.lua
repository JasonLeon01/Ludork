local UiController = require("Internal.UIBase.UiController")

local ConfigRowControllerBase = {}

function ConfigRowControllerBase:init(ui, labelText)
    self._labelText = labelText
    super(ConfigRowControllerBase, self).init(nil, ui)
end

function ConfigRowControllerBase:prepare()
    local root = super(ConfigRowControllerBase, self).prepare()
    root:render()
    return root
end

function ConfigRowControllerBase:getActive()
    return self.root:getActive()
end

function ConfigRowControllerBase:setActive(active)
    self.root:setActive(active)
end

function ConfigRowControllerBase:addConfirmCallback(callback)
    self.root:addConfirmCallback(callback)
end

function ConfigRowControllerBase:setLabelText(labelText)
    self._labelText = labelText
    self:prepare()
end

---@diagnostic disable-next-line: unused
function ConfigRowControllerBase:getChildren()
    return {}
end

function ConfigRowControllerBase:getSize()
    return self.root:getSize()
end

function ConfigRowControllerBase:getLocalBounds()
    return self.root:getLocalBounds()
end

function ConfigRowControllerBase:onTick(deltaTime)
    local _deltaTime = deltaTime

    self.root:render()
end

return class(ConfigRowControllerBase, UiController)
