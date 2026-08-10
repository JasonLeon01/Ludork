local Engine = require("Engine")
local WindowCommand = require("Source.Windows.WindowCommand")
local WindowShopCommandController = require("Source.Windows.WindowShopCommandController")

local Input = Engine.Input

---@class Source.Windows.WindowShopCommand
local WindowShopCommand = {}

WindowShopCommand.controllerClass = WindowShopCommandController

function WindowShopCommand:init(rect, owner)
    local commands = self.controllerClass.CreateCommands(owner)
    super(WindowShopCommand, self).init(rect, commands, nil, 32, nil, nil, 2)
    self._owner = owner
    self._lastIndex = self.index
end

function WindowShopCommand:onTick(deltaTime)
    super(WindowShopCommand, self).onTick(deltaTime)
    if self.index ~= self._lastIndex then
        self._lastIndex = self.index
        self._owner:setMode(
            self.index == 1 and self._owner.SHOP_MODE_SELL or self._owner.SHOP_MODE_BUY
        )
    end
end

function WindowShopCommand:onKeyDown(kwargs)
    if Input.isActionTriggered(Input.getCancelKeys(), false) then
        self._owner:closeByCancel()
        Input.isActionTriggered(Input.getCancelKeys(), true)
        return
    end
    super(WindowShopCommand, self).onKeyDown(kwargs)
end

function WindowShopCommand:onMouseButtonDown(kwargs)
    if kwargs.button == sf.Mouse.Button.Right then
        self._owner:closeByCancel()
        return true
    end
    return false
end

return class(WindowShopCommand, WindowCommand)
