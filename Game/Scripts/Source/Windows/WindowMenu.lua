local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local WindowTransition = require("Source.UIBase.WindowTransition")
local CommandRowController = require("Source.UIBase.CommandRow.Controller")
local Ui = require("Source.UIBase.Ui")
local UiLayout = require("Source.UIBase.UiLayout")
local View = require("Source.UI.WindowMenu")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local Input = Engine.Input
local Direction = Engine.FocusDirection
local AudioManager = GlobalCore.AudioManager

---@param window Source.UIBase.Ui.Window | nil
---@return boolean
local function isVisible(window)
    return window ~= nil and window:getVisible()
end

---@param window   Source.UIBase.Ui.Window | nil
---@param position sf.Vector2f
---@return boolean
local function containsPointer(window, position)
    return window ~= nil and window:getVisible() and sf.FloatRect.contains(window:getAbsoluteBounds(), position)
end

---@class Source.Windows.WindowMenu.Controller
local Controller = {}

Controller.windowOptions = { hidden = true, returnButton = true, list = "MenuList", scroll = "MenuScrollBox" }

function Controller:init(player, windows, onExit)
    self._player = player
    self._windowItem = windows.item
    self._windowEquip = windows.equip
    self._windowSaveLoad = windows.saveLoad
    self._configWindow = windows.config
    self._onExit = onExit
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
    return self.host:getVisible() or isVisible(self._windowItem:peek()) or isVisible(self._windowEquip:peek())
        or isVisible(self._windowSaveLoad:peek()) or isVisible(self._configWindow:peek())
end

function Controller:onReturn()
    self:handleCancel()
end

function Controller:openInventory()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("item")
    self._windowItem:get():open(WindowTransition.MENU, UiLayout.GetMenuDockPosition(self.host))
    self:_syncReturnButtonSuppression()
end

function Controller:openEquipment()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("equip")
    self._windowEquip:get():open(WindowTransition.MENU, UiLayout.GetMenuDockPosition(self.host))
    self:_syncReturnButtonSuppression()
end

function Controller:openSaveLoad()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("save")
    self._windowSaveLoad:get():open(WindowTransition.MENU, nil, UiLayout.GetMenuDockPosition(self.host))
    self:_syncReturnButtonSuppression()
end

function Controller:openConfig()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("config")
    self.host:setActive(false)
    self._configWindow:get():open()
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

function Controller:onSubMenuClose()
    self:_syncReturnButtonSuppression()
    if self.host:isTransitionOpen() then
        self.host:requestKeyboardFocus()
    end
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
        row:prepare()
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
    self._moveRestoreGuard = function ()
        return true
    end
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
    self:close(self._onExit)
end

function Controller:_getCurrentSubMenuFocusTarget()
    if self.host.index == 0 then
        local window = self._windowItem:peek()
        if window ~= nil and window:getVisible() then
            return window
        end
    elseif self.host.index == 1 then
        local window = self._windowEquip:peek()
        if window ~= nil and window:getVisible() then
            return window:getSlotFocusTarget()
        end
    elseif self.host.index == 2 then
        local window = self._windowSaveLoad:peek()
        if window ~= nil and window:getVisible() then
            return window:getSlotWindow()
        end
    end
    return nil
end

function Controller:_isPointerInsideMenuGroup(position)
    return containsPointer(self.host, position) or containsPointer(self._windowItem:peek(), position)
        or containsPointer(self._windowEquip:peek(), position) or containsPointer(self._windowSaveLoad:peek(), position)
        or containsPointer(self._configWindow:peek(), position)
end

function Controller:_closeSubMenus(exceptName)
    exceptName = exceptName or ""
    local closed = false
    local item = self._windowItem:peek()
    local equip = self._windowEquip:peek()
    local saveLoad = self._windowSaveLoad:peek()
    local config = self._configWindow:peek()
    if exceptName ~= "item" and item ~= nil and item:getVisible() then
        item:close()
        closed = true
    end
    if exceptName ~= "equip" and equip ~= nil and equip:getVisible() then
        equip:close()
        closed = true
    end
    if exceptName ~= "save" and saveLoad ~= nil and saveLoad:getVisible() then
        saveLoad:close()
        closed = true
    end
    if exceptName ~= "config" and config ~= nil and config:isOpen() then
        config:close()
        closed = true
    end
    self:_syncReturnButtonSuppression()
    return closed
end

function Controller:_syncReturnButtonSuppression()
    local config = self._configWindow:peek()
    local suppressed = isVisible(self._windowItem:peek()) or isVisible(self._windowEquip:peek())
        or isVisible(self._windowSaveLoad:peek()) or (config ~= nil and config:isOpen())
    self.host:setReturnButtonSuppressed(suppressed)
end

function Controller:_returnEquipSelectToSlot()
    local window = self._windowEquip:peek()
    return window ~= nil and window:returnSelectToSlot()
end

function Controller:attach(commands)
    local itemSize = self.ui.controls["MenuList"]:getDefaultItemSize()
    local rowSize = sf.Vector2u.new(math.floor(itemSize.x), math.floor(itemSize.y))
    ---@cast rowSize sf.Vector2u
    for _, model in ipairs(commands) do
        self._commands:add(model, rowSize)
    end
    self._commands:layout()
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
