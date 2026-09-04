local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local WindowMenuUI = require("Source.UI.WindowMenu")
local WindowTransition = require("Source.UI.WindowTransition")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local WindowCommand = require("Source.Windows.WindowCommand")

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

---@class Source.Windows.WindowMenu.Controller
local WindowMenuController = {}

function WindowMenuController.CreateCommands(owner)
    return {
        {
            localeKey = "MENU_ITEM",
            callback = function ()
                owner:_onMenuItem()
            end
        },
        {
            localeKey = "MENU_EQUIP",
            callback = function ()
                owner:_onMenuEquip()
            end
        },
        {
            localeKey = "MENU_SAVE_FILE",
            callback = function ()
                owner:_onMenuSave()
            end
        },
        {
            localeKey = "MENU_CONFIG",
            callback = function ()
                owner:_onMenuConfig()
            end
        },
        {
            localeKey = "MENU_EXIT",
            callback = function ()
                owner:_onMenuExit()
            end
        }
    }
end

function WindowMenuController:bind()
    self._menuControls = {
        configureMenuControl(self.model), configureMenuControl(self.model._windowItem),
        configureMenuControl(self.model._windowEquip), configureMenuControl(self.model._windowSaveLoad),
        configureMenuControl(self.model._configWindow)
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
    self.model._windowItem._onCloseCallback = onSubMenuClose
    self.model._windowEquip:setOnCloseCallback(onSubMenuClose)
    self.model._windowItem._onUseCallback = function ()
        self:close()
    end
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
    self.model._player:setMoveEnabled(false)
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
            self.model._player:setMoveEnabled(true)
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

function WindowMenuController:getMenuControls()
    return self._menuControls
end

function WindowMenuController:_closeByCancel()
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:close()
end

function WindowMenuController:_handleCancel()
    if self:_returnEquipSelectToSlot() then
        return
    end
    if self:_closeSubMenus() then
        AudioManager.playSound(GameSystem.GetCancelSE())
        return
    end
    self:_closeByCancel()
end

function WindowMenuController:_onMenuItem()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("item")
    self.model._windowItem:open()
    self:_syncReturnButtonSuppression()
end

function WindowMenuController:_onMenuEquip()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("equip")
    self.model._windowEquip:open()
    self:_syncReturnButtonSuppression()
end

function WindowMenuController:_onMenuSave()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("save")
    self.model._windowSaveLoad:open(WindowTransition.MENU)
    self:_syncReturnButtonSuppression()
end

function WindowMenuController:_onMenuConfig()
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:_closeSubMenus("config")
    self.model:setActive(false)
    self.model._configWindow:open()
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
    if self.model.index == 0 and self.model._windowItem:getVisible() then
        return asSelectableWindow(self.model._windowItem)
    end
    if self.model.index == 1 and self.model._windowEquip:getVisible() then
        return asSelectableWindow(self.model._windowEquip:getSlotFocusTarget())
    end
    if self.model.index == 2 and self.model._windowSaveLoad:getVisible() then
        return asSelectableWindow(self.model._windowSaveLoad:getSlotWindow())
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
    if exceptName ~= "item" and self.model._windowItem:getVisible() then
        self.model._windowItem:close()
        closed = true
    end
    if exceptName ~= "equip" and self.model._windowEquip:getVisible() then
        self.model._windowEquip:close()
        closed = true
    end
    if exceptName ~= "save" and self.model._windowSaveLoad:getVisible() then
        self.model._windowSaveLoad:close()
        closed = true
    end
    if exceptName ~= "config" and self.model._configWindow:isOpen() then
        self.model._configWindow:close()
        closed = true
    end
    self:_syncReturnButtonSuppression()
    return closed
end

function WindowMenuController:_syncReturnButtonSuppression()
    local suppressed = self.model._windowItem:getVisible() or self.model._windowEquip:getVisible()
        or self.model._windowSaveLoad:getVisible() or self.model._configWindow:isOpen()
    self.model:_setReturnButtonSuppressed(suppressed)
end

function WindowMenuController:_returnEquipSelectToSlot()
    return self.model._windowEquip:returnSelectToSlot()
end

local FinalWindowMenuController = class(WindowMenuController, WindowCommand.Controller)

---@class Source.Windows.WindowMenu
local WindowMenu = {}

WindowMenu.controllerClass = FinalWindowMenuController

function WindowMenu:init(player, windows)
    self._player = player
    self._windowItem = windows.item
    self._windowEquip = windows.equip
    self._windowSaveLoad = windows.saveLoad
    self._configWindow = windows.config
    local commands = FinalWindowMenuController.CreateCommands(self)
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
