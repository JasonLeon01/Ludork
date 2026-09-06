local Engine = require("Engine")
local WindowMenuUI = require("Source.UI.WindowMenu")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local WindowMenuController = require("Source.Windows.WindowMenu.Controller")

---@class Source.Windows.WindowMenu
local WindowMenu = {}

WindowMenu.controllerClass = WindowMenuController

function WindowMenu:init(player, windows)
    self._player = player
    self._windowItem = windows.item
    self._windowEquip = windows.equip
    self._windowSaveLoad = windows.saveLoad
    self._configWindow = windows.config
    local commands = WindowMenuController.CreateCommands(self)
    super(WindowMenu, self).init(Engine.ToIntRect(0, 0, 192, 192), nil, 160, 32, nil, nil, nil, nil, true)
    self._ui = WindowMenuUI.new(self)
    self._ui:attach()
    self:setHasReturnBtn(true)
    self:setScrollBox(self._ui:getScrollBox())
    self:setListView(self._ui:getListView())
    self._menuController = self.controllerClass.new(self, self.content:getSize(), 32, 1)
    self._menuController:attachTo(self._ui:getListView(), commands)
    ---@cast self._menuController Source.Windows.WindowMenu.Controller
    self._menuControls = self._menuController:getMenuControls()
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
    self._menuController:_handleCancel()
end

function WindowMenu:_onMenuItem()
    self._menuController:_onMenuItem()
end

function WindowMenu:_onMenuEquip()
    self._menuController:_onMenuEquip()
end

function WindowMenu:_onMenuSave()
    self._menuController:_onMenuSave()
end

function WindowMenu:_onMenuConfig()
    self._menuController:_onMenuConfig()
end

function WindowMenu:_onMenuExit()
    self._menuController:onMenuExit()
end

function WindowMenu:onSaveLoadClose()
    self._menuController:onSaveLoadClose()
end

function WindowMenu:onConfigClose()
    self._menuController:onConfigClose()
end

---@return Source.Windows.Base.WindowSelectable | nil
function WindowMenu:_getCurrentSubMenuFocusTarget()
    return self._menuController:_getCurrentSubMenuFocusTarget()
end

---@param position sf.Vector2f
---@return boolean
function WindowMenu:_isPointerInsideMenuGroup(position)
    return self._menuController:_isPointerInsideMenuGroup(position)
end

---@param exceptName string | nil
---@return boolean
function WindowMenu:_closeSubMenus(exceptName)
    return self._menuController:_closeSubMenus(exceptName)
end

---@return boolean
function WindowMenu:_returnEquipSelectToSlot()
    return self._menuController:_returnEquipSelectToSlot()
end

return class(WindowMenu, WindowSelectable)
