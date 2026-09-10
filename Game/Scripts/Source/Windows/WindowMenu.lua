local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local WindowTransition = require("Source.UIBase.WindowTransition")
local CommandRowController = require("Source.UIBase.CommandRow.Controller")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.WindowMenu")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local Input = Engine.Input
local Direction = Engine.FocusDirection
local AudioManager = GlobalCore.AudioManager
local GlobalSystem = GlobalCore.System

local _MENU_Z_ORDER = 1

---@class Source.Windows.WindowMenu.Controller
local Controller = {}

Controller.windowOptions = {
    hidden = true,
    returnButton = true,
    list = "MenuList",
    scroll = "MenuScrollBox",
    itemWidth = 160,
    itemHeight = 32
}

function Controller:init(player, windows)
    self._player = player
    self._windowItem = windows.item
    self._windowEquip = windows.equip
    self._windowSaveLoad = windows.saveLoad
    self._configWindow = windows.config
    self._commands = self:createCollection(self.ui.controls["MenuList"], CommandRowController)
    self:attach(Controller.CreateCommands(self.host))
end

function Controller:setPlayer(player)
    self._player = player
end

function Controller:setMoveRestoreGuard(guard)
    self._moveRestoreGuard = guard
end

function Controller:onMouseButtonDown(kwargs)
    if self:handleMouseButtonDown(kwargs) then
        return true
    end
    return WindowSelectable.onMouseButtonDown(self.host, kwargs)
end

function Controller:onTick(deltaTime)
    WindowSelectable.onTick(self.host, deltaTime)
    self:tick()
end

function Controller:onDirectionalKey(direction)
    if self:handleDirectionalKey(direction) then
        return true
    end
    return WindowSelectable.onDirectionalKey(self.host, direction)
end

function Controller:open()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:getPlayer():setMoveEnabled(false)
    self.host:resetSelection()
    self:_syncReturnButtonSuppression()
    self.host:showWithAnimation("FadeIn", function ()
        self.host:setActive(true)
        self.host:requestKeyboardFocus()
    end)
end

function Controller:close(onHidden)
    self:_closeSubMenus()
    self.host:setActive(false)
    self:_syncReturnButtonSuppression()
    self.host:hideWithAnimation("FadeOut", function ()
        if self._moveRestoreGuard() then
            self:getPlayer():setMoveEnabled(true)
        end
        if onHidden ~= nil then
            onHidden()
        end
    end)
end

function Controller:isBlocking()
    for _, window in ipairs(self._menuControls) do
        if window:getVisible() then
            return true
        end
    end
    return false
end

function Controller:onReturn()
    self:handleCancel()
end

function Controller:openInventory()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("item")
    self._windowItem:open()
    self:_syncReturnButtonSuppression()
end

function Controller:openEquipment()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("equip")
    self._windowEquip:open()
    self:_syncReturnButtonSuppression()
end

function Controller:openSaveLoad()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("save")
    self._windowSaveLoad:open(WindowTransition.MENU)
    self:_syncReturnButtonSuppression()
end

function Controller:openConfig()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("config")
    self.host:setActive(false)
    self._configWindow:open()
    self:_syncReturnButtonSuppression()
end

function Controller:exitGame()
    self:onMenuExit()
end

function Controller:onSaveLoadClose()
    if not self.host:isTransitionOpen() then
        return
    end
    self:_syncReturnButtonSuppression()
    self.host:requestKeyboardFocus()
end

function Controller:onConfigClose()
    if not self.host:isTransitionOpen() then
        return
    end
    self.host:setActive(true)
    self:_syncReturnButtonSuppression()
    self.host:requestKeyboardFocus()
end

function Controller:getPlayer()
    return self._player
end

function Controller:refreshRows()
    for _, row in ipairs(self._commands.items) do
        row:refresh()
    end
end

function Controller.CreateCommands(owner)
    return {
        {
            localeKey = "MENU_ITEM",
            callback = function ()
                owner:openInventory()
            end
        },
        {
            localeKey = "MENU_EQUIP",
            callback = function ()
                owner:openEquipment()
            end
        },
        {
            localeKey = "MENU_SAVE_FILE",
            callback = function ()
                owner:openSaveLoad()
            end
        },
        {
            localeKey = "MENU_CONFIG",
            callback = function ()
                owner:openConfig()
            end
        },
        {
            localeKey = "MENU_EXIT",
            callback = function ()
                owner:exitGame()
            end
        }
    }
end

function Controller:bind()
    self._menuControls = { self.host, self._windowItem, self._windowEquip, self._windowSaveLoad, self._configWindow }
    for _, control in ipairs(self._menuControls) do
        control:setZOrder(_MENU_Z_ORDER)
    end
    self._moveRestoreGuard = function ()
        return true
    end
    local onSubMenuClose = function ()
        self:_syncReturnButtonSuppression()
        if self.host:isTransitionOpen() then
            self.host:requestKeyboardFocus()
        end
    end
    self._windowItem:setOnCloseCallback(onSubMenuClose)
    self._windowEquip:setOnCloseCallback(onSubMenuClose)
    self._windowItem:setOnUseCallback(function ()
        self:close()
    end)
end

function Controller:handleMouseButtonDown(kwargs)
    if kwargs.button == sf.Mouse.Button.Left
        and not self:_isPointerInsideMenuGroup(Engine.ToVector2f(Input.getMousePosition())) then
        self:_closeByCancel()
        return true
    end
    return false
end

function Controller:tick()
    if Input.isTouchBlocked() or not self.host:getActive() then
        return
    end
    local tapPosition = Input.getTouchTapPosition()
    if tapPosition == nil or self:_isPointerInsideMenuGroup(Engine.ToVector2f(tapPosition)) then
        return
    end
    self:_closeByCancel()
    Input.isTouchTap(true)
    Input.isTouchTriggered(true)
end

function Controller:handleDirectionalKey(direction)
    if direction ~= Direction.RIGHT then
        return false
    end
    local target = self:_getCurrentSubMenuFocusTarget()
    if target == nil then
        return false
    end
    target:requestKeyboardFocusAtCursor()
    return true
end

function Controller:_closeByCancel()
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:close()
end

function Controller:handleCancel()
    if self:_returnEquipSelectToSlot() then
        return
    end
    if self:_closeSubMenus() then
        AudioManager.playSound(GameSystem.GetCancelSE())
        return
    end
    self:_closeByCancel()
end

function Controller:onMenuExit()
    local Title = require("Source.Scenes.SceneTitle")

    self:close(function ()
        GlobalSystem.setScene(Title.new())
    end)
end

function Controller:_getCurrentSubMenuFocusTarget()
    if self.host.index == 0 and self._windowItem:getVisible() then
        return self._windowItem
    end
    if self.host.index == 1 and self._windowEquip:getVisible() then
        return self._windowEquip:getSlotFocusTarget()
    end
    if self.host.index == 2 and self._windowSaveLoad:getVisible() then
        return self._windowSaveLoad:getSlotWindow()
    end
    return nil
end

function Controller:_isPointerInsideMenuGroup(position)
    for _, window in ipairs(self._menuControls) do
        if window:getVisible() and sf.FloatRect.contains(window:getAbsoluteBounds(), position) then
            return true
        end
    end
    return false
end

function Controller:_closeSubMenus(exceptName)
    exceptName = exceptName or ""
    local closed = false
    if exceptName ~= "item" and self._windowItem:getVisible() then
        self._windowItem:close()
        closed = true
    end
    if exceptName ~= "equip" and self._windowEquip:getVisible() then
        self._windowEquip:close()
        closed = true
    end
    if exceptName ~= "save" and self._windowSaveLoad:getVisible() then
        self._windowSaveLoad:close()
        closed = true
    end
    if exceptName ~= "config" and self._configWindow:isOpen() then
        self._configWindow:close()
        closed = true
    end
    self:_syncReturnButtonSuppression()
    return closed
end

function Controller:_syncReturnButtonSuppression()
    local suppressed = self._windowItem:getVisible() or self._windowEquip:getVisible()
        or self._windowSaveLoad:getVisible() or self._configWindow:isOpen()
    self.host:setReturnButtonSuppressed(suppressed)
end

function Controller:_returnEquipSelectToSlot()
    return self._windowEquip:returnSelectToSlot()
end

function Controller:attach(commands)
    local rowSize = sf.Vector2u.new(math.max(1, math.floor(self.ui.controls["Content"]:getSize().x - 32)), 32)
    ---@cast rowSize sf.Vector2u
    for _, model in ipairs(commands) do
        local row = self._commands:add(model, rowSize)
        self.host:applyItem(row.ui.root)
    end
    self._commands:layout()
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
