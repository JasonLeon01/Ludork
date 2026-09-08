local Engine = require("Engine")
local WindowMenuUI = require("Source.UI.WindowMenu")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local WindowMenuController = require("Source.Windows.WindowMenu.Controller")

---@class Source.Windows.WindowMenu
local WindowMenu = {}

WindowMenu.controllerClass = WindowMenuController

function WindowMenu:init(player, windows)
    self._player = player
    local commands = WindowMenuController.CreateCommands(self)
    super(WindowMenu, self).init(Engine.ToIntRect(0, 0, 192, 192), nil, 160, 32, nil, nil, nil, nil, true)
    self._ui = WindowMenuUI.new(self)
    self._ui:attach()
    self:setHasReturnBtn(true)
    self:setScrollBox(self._ui:getScrollBox())
    self:setListView(self._ui:getListView())
    self._menuController = self.controllerClass.new(self, self.content:getSize(), 32, 1, windows)
    self._menuController:attachTo(self._ui:getListView(), commands)
    ---@cast self._menuController Source.Windows.WindowMenu.Controller
    self:hideImmediate()
end

function WindowMenu:setPlayer(player)
    self._player = player
end

function WindowMenu:setMoveRestoreGuard(guard)
    self._menuController:setMoveRestoreGuard(guard)
end

function WindowMenu:onMouseButtonDown(kwargs)
    if self._menuController:handleMouseButtonDown(kwargs) then
        return true
    end
    return super(WindowMenu, self).onMouseButtonDown(kwargs)
end

function WindowMenu:onTick(deltaTime)
    super(WindowMenu, self).onTick(deltaTime)
    self._menuController:tick()
end

function WindowMenu:onDirectionalKey(direction)
    if self._menuController:handleDirectionalKey(direction) then
        return true
    end
    return super(WindowMenu, self).onDirectionalKey(direction)
end

function WindowMenu:open()
    self._menuController:open()
end

function WindowMenu:close(onHidden)
    self._menuController:close(onHidden)
end

function WindowMenu:isBlocking()
    return self._menuController:isBlocking()
end

function WindowMenu:onReturn()
    self._menuController:handleCancel()
end

function WindowMenu:openInventory()
    self._menuController:openInventory()
end

function WindowMenu:openEquipment()
    self._menuController:openEquipment()
end

function WindowMenu:openSaveLoad()
    self._menuController:openSaveLoad()
end

function WindowMenu:openConfig()
    self._menuController:openConfig()
end

function WindowMenu:exitGame()
    self._menuController:onMenuExit()
end

function WindowMenu:onSaveLoadClose()
    self._menuController:onSaveLoadClose()
end

function WindowMenu:onConfigClose()
    self._menuController:onConfigClose()
end

function WindowMenu:getPlayer()
    return self._player
end

return class(WindowMenu, WindowSelectable)
