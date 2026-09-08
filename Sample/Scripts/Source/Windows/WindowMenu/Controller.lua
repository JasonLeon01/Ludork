local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local WindowTransition = require("Source.UI.WindowTransition")
local WindowCommandController = require("Source.Windows.WindowCommand.Controller")

local Input = Engine.Input
local Direction = Engine.FocusDirection
local AudioManager = GlobalCore.AudioManager
local GlobalSystem = GlobalCore.System

local _MENU_Z_ORDER = 1

---@param control any
---@return Engine.Canvas
local function configureMenuControl(control)
    ---@cast control Engine.Canvas
    control:setZOrder(_MENU_Z_ORDER)
    return control
end

---@param control any
---@return Source.Windows.Base.WindowSelectable
local function asSelectableWindow(control)
    ---@cast control Source.Windows.Base.WindowSelectable
    return control
end

---@class (partial) Source.Windows.WindowMenu.Controller
local WindowMenuController = {}

function WindowMenuController:init(model, size, rowHeight, columns, windows)
    self._windowItem = windows.item
    self._windowEquip = windows.equip
    self._windowSaveLoad = windows.saveLoad
    self._configWindow = windows.config
    super(WindowMenuController, self).init(model, size, rowHeight, columns)
end

function WindowMenuController.CreateCommands(owner)
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

function WindowMenuController:bind()
    self._menuControls = {
        configureMenuControl(self.model), configureMenuControl(self._windowItem),
        configureMenuControl(self._windowEquip), configureMenuControl(self._windowSaveLoad),
        configureMenuControl(self._configWindow)
    }
    self._moveRestoreGuard = function ()
        return true
    end
    local onSubMenuClose = function ()
        self:_syncReturnButtonSuppression()
        if self.model:isTransitionOpen() then
            self.model:requestKeyboardFocus()
        end
    end
    self._windowItem:setOnCloseCallback(onSubMenuClose)
    self._windowEquip:setOnCloseCallback(onSubMenuClose)
    self._windowItem:setOnUseCallback(function ()
        self:close()
    end)
end

function WindowMenuController:setMoveRestoreGuard(guard)
    self._moveRestoreGuard = guard
end

function WindowMenuController:handleMouseButtonDown(kwargs)
    if kwargs.button == sf.Mouse.Button.Left
        and not self:_isPointerInsideMenuGroup(Engine.ToVector2f(Input.getMousePosition())) then
        self:_closeByCancel()
        return true
    end
    return false
end

function WindowMenuController:tick()
    if Input.isTouchBlocked() or not self.model:getActive() then
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

function WindowMenuController:handleDirectionalKey(direction)
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

function WindowMenuController:open()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self.model:getPlayer():setMoveEnabled(false)
    self.model:resetSelection()
    self:_syncReturnButtonSuppression()
    self.model:showWithAnimation("FadeIn", function ()
        self.model:setActive(true)
        self.model:requestKeyboardFocus()
    end)
end

function WindowMenuController:close(onHidden)
    self:_closeSubMenus()
    self.model:setActive(false)
    self:_syncReturnButtonSuppression()
    self.model:hideWithAnimation("FadeOut", function ()
        if self._moveRestoreGuard() then
            self.model:getPlayer():setMoveEnabled(true)
        end
        if onHidden ~= nil then
            onHidden()
        end
    end)
end

function WindowMenuController:isBlocking()
    for _, window in ipairs(self._menuControls) do
        if window:getVisible() then
            return true
        end
    end
    return false
end

function WindowMenuController:_closeByCancel()
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:close()
end

function WindowMenuController:handleCancel()
    if self:_returnEquipSelectToSlot() then
        return
    end
    if self:_closeSubMenus() then
        AudioManager.playSound(GameSystem.GetCancelSE())
        return
    end
    self:_closeByCancel()
end

function WindowMenuController:openInventory()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("item")
    self._windowItem:open()
    self:_syncReturnButtonSuppression()
end

function WindowMenuController:openEquipment()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("equip")
    self._windowEquip:open()
    self:_syncReturnButtonSuppression()
end

function WindowMenuController:openSaveLoad()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("save")
    self._windowSaveLoad:open(WindowTransition.MENU)
    self:_syncReturnButtonSuppression()
end

function WindowMenuController:openConfig()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("config")
    self.model:setActive(false)
    self._configWindow:open()
    self:_syncReturnButtonSuppression()
end

function WindowMenuController:onSaveLoadClose()
    if not self.model:isTransitionOpen() then
        return
    end
    self:_syncReturnButtonSuppression()
    self.model:requestKeyboardFocus()
end

function WindowMenuController:onConfigClose()
    if not self.model:isTransitionOpen() then
        return
    end
    self.model:setActive(true)
    self:_syncReturnButtonSuppression()
    self.model:requestKeyboardFocus()
end

function WindowMenuController:onMenuExit()
    local Title = require("Source.Scenes.SceneTitle")

    self:close(function ()
        GlobalSystem.setScene(Title.new())
    end)
end

---@return Source.Windows.Base.WindowSelectable | nil
function WindowMenuController:_getCurrentSubMenuFocusTarget()
    if self.model.index == 0 and self._windowItem:getVisible() then
        return asSelectableWindow(self._windowItem)
    end
    if self.model.index == 1 and self._windowEquip:getVisible() then
        return asSelectableWindow(self._windowEquip:getSlotFocusTarget())
    end
    if self.model.index == 2 and self._windowSaveLoad:getVisible() then
        return asSelectableWindow(self._windowSaveLoad:getSlotWindow())
    end
    return nil
end

function WindowMenuController:_isPointerInsideMenuGroup(position)
    for _, window in ipairs(self._menuControls) do
        if window:getVisible() and sf.FloatRect.contains(window:getAbsoluteBounds(), position) then
            return true
        end
    end
    return false
end

function WindowMenuController:_closeSubMenus(exceptName)
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

function WindowMenuController:_syncReturnButtonSuppression()
    local suppressed = self._windowItem:getVisible() or self._windowEquip:getVisible()
        or self._windowSaveLoad:getVisible() or self._configWindow:isOpen()
    self.model:setReturnButtonSuppressed(suppressed)
end

function WindowMenuController:_returnEquipSelectToSlot()
    return self._windowEquip:returnSelectToSlot()
end

return class(WindowMenuController, WindowCommandController)
