local Engine = require("Engine")
local UiController = require("Source.UI.UiController")

local ListView = Engine.ListView

local ConfigRowControllerBase = {}

function ConfigRowControllerBase:init(model, rowWidth, rowHeight)
    self._rowWidth = math.max(1, math.floor(rowWidth))
    self._rowHeight = math.max(1, math.ceil(rowHeight))
    super(ConfigRowControllerBase, self).init(model, nil)
end

function ConfigRowControllerBase:prepare()
    local root = super(ConfigRowControllerBase, self).prepare(sf.Vector2u.new(self._rowWidth, self._rowHeight))
    root:setOrigin(sf.Vector2f.new(0.0, 0.0))
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

function ConfigRowControllerBase:getChildren()
    local _ = self

    return {}
end

function ConfigRowControllerBase:getSize()
    return sf.Vector2u.new(self._rowWidth, self._rowHeight)
end

function ConfigRowControllerBase:getLocalBounds()
    local size = self:getSize()
    ---@cast size sf.Vector2u
    return sf.FloatRect.new(sf.Vector2f.new(0.0, 0.0), sf.Vector2f.new(size.x, size.y))
end

function ConfigRowControllerBase:onTick(deltaTime)
    local _deltaTime = deltaTime

    self.root:render()
end

function ConfigRowControllerBase:setRowHeight(rowHeight)
    self._rowHeight = math.max(1, math.ceil(rowHeight))
    self.view:reflow(sf.Vector2u.new(self._rowWidth, self._rowHeight))
    self.root:render()
    local parent = self.root:getParent()
    if Class.isInstance(parent, ListView) then
        parent:invalidatePositions()
        parent:applyPositions()
    end
end

return class(ConfigRowControllerBase, UiController)
