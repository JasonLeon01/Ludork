local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local GameSystem = require("Source.System")
local LocaleCore = require("Source.Locale.Core")
local WindowCommand = require("Source.Windows.WindowCommand")

local Input = Engine.Input
local Direction = Engine.FocusDirection
local ManagerFunctions = GlobalFunctions.Manager
local GlobalSystem = GlobalCore.System
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

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

function WindowMenuController.createCommands(owner)
    return {
        {
            text = LOC("MENU_ITEM"),
            callback = function ()
                owner:_onMenuItem()
            end
        },
        {
            text = LOC("MENU_EQUIP"),
            callback = function ()
                owner:_onMenuEquip()
            end
        },
        {
            text = LOC("MENU_SAVE_FILE"),
            callback = function ()
                owner:_onMenuSave()
            end
        },
        {
            text = LOC("MENU_EXIT"),
            callback = function ()
                WindowMenuController._onMenuExit()
            end
        }
    }
end

function WindowMenuController:bind()
    local model = self.model
    self._menuControls = {
        configureMenuControl(model), configureMenuControl(model._windowItem),
        configureMenuControl(model._windowEquipSlot), configureMenuControl(model._windowEquipSelect),
        configureMenuControl(model._windowEquipStatus),
        configureMenuControl(assert(model._windowSaveLoad:getCommandWindow(), "Menu save command window is missing")),
        configureMenuControl(model._windowSaveLoad:getSlotWindow()),
        configureMenuControl(model._windowSaveLoad:getDetailWindow())
    }
    self._moveRestoreGuard = function ()
        return true
    end
    local onSubMenuClose = function ()
        model:requestKeyboardFocus()
    end
    model._windowItem._onCloseCallback = onSubMenuClose
    model._windowEquipSlot._onCloseCallback = onSubMenuClose
    model._windowItem._onUseCallback = function ()
        self:close()
    end
end

function WindowMenuController:setMoveRestoreGuard(guard)
    self._moveRestoreGuard = guard
end

function WindowMenuController:handleKeyDown()
    local cancelKeys = Input.getCancelKeys()
    if not Input.isActionTriggered(cancelKeys, false) then
        return false
    end
    self:_handleCancel()
    Input.isActionTriggered(cancelKeys, true)
    return true
end

function WindowMenuController:handleMouseButtonDown(kwargs)
    if kwargs.button == sf.Mouse.Button.Right then
        self:_handleCancel()
        return true
    end
    if kwargs.button == sf.Mouse.Button.Left
        and not self:_isPointerInsideMenuGroup(Engine.ToVector2f(Input.getMousePosition())) then
        self:_closeByCancel()
        return true
    end
    return false
end

function WindowMenuController:tick()
    if Input.isTouchBlocked() then
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
    ManagerFunctions.playSE(GameSystem.getDecisionSE())
    self.model._player:setMoveEnabled(false)
    self.model:setVisible(true)
    self.model:setActive(true)
    self.model:requestKeyboardFocus()
end

function WindowMenuController:close()
    self:_closeSubMenus()
    self.model:setVisible(false)
    self.model:setActive(false)
    if self._moveRestoreGuard() then
        self.model._player:setMoveEnabled(true)
    end
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
    ManagerFunctions.playSE(GameSystem.getCancelSE())
    self:close()
end

function WindowMenuController:_handleCancel()
    if self:_returnEquipSelectToSlot() then
        return
    end
    if self:_returnSaveSlotToCommand() then
        return
    end
    if self:_closeSubMenus() then
        self.model:requestKeyboardFocus()
        return
    end
    self:_closeByCancel()
end

function WindowMenuController:_onMenuItem()
    ManagerFunctions.playSE(GameSystem.getDecisionSE())
    self:_closeSubMenus("item")
    self.model._windowItem:open()
    self.model._windowItem:requestKeyboardFocusAtCursor()
end

function WindowMenuController:_onMenuEquip()
    ManagerFunctions.playSE(GameSystem.getDecisionSE())
    self:_closeSubMenus("equip")
    self.model._windowEquipSelect:open()
    self.model._windowEquipSlot:open()
    self.model._windowEquipSlot:requestKeyboardFocusAtCursor()
end

function WindowMenuController:_onMenuSave()
    ManagerFunctions.playSE(GameSystem.getDecisionSE())
    self:_closeSubMenus("save")
    self.model._windowSaveLoad:open()
    ---@type Source.Windows.WindowSaveCommand | Source.Windows.WindowSaveSlot | nil
    local focusTarget = self.model._windowSaveLoad:getCommandWindow()
    if focusTarget == nil or not focusTarget:getActive() then
        focusTarget = self.model._windowSaveLoad:getSlotWindow()
    end
    focusTarget:requestKeyboardFocusAtCursor()
end

function WindowMenuController:onSaveLoadClose()
    self.model:requestKeyboardFocus()
end

function WindowMenuController._onMenuExit()
    local Title = require("Source.Scenes.SceneTitle")

    GlobalSystem.setScene(Title.new())
end

---@return Source.Windows.Base.WindowSelectable | nil
function WindowMenuController:_getCurrentSubMenuFocusTarget()
    if self.model.index == 0 and self.model._windowItem:getVisible() then
        return asSelectableWindow(self.model._windowItem)
    end
    if self.model.index == 1 and self.model._windowEquipSlot:getVisible() then
        return asSelectableWindow(self.model._windowEquipSlot)
    end
    if self.model.index == 2 and self.model._windowSaveLoad:getVisible() then
        local saveCommandWindow = assert(
            self.model._windowSaveLoad:getCommandWindow(), "Menu save command window is missing"
        )
        if saveCommandWindow:getActive() then
            return asSelectableWindow(saveCommandWindow)
        end
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
    if exceptName ~= "equip"
        and (self.model._windowEquipSlot:getVisible() or self.model._windowEquipSelect:getVisible()
            or self.model._windowEquipStatus:getVisible()) then
        self.model._windowEquipSlot:close()
        closed = true
    end
    if exceptName ~= "save" and self.model._windowSaveLoad:getVisible() then
        self.model._windowSaveLoad:close()
        closed = true
    end
    return closed
end

function WindowMenuController:_returnEquipSelectToSlot()
    if not self.model._windowEquipSelect:getVisible() then
        return false
    end
    if not (self.model._windowEquipSelect:getActive() or self.model._windowEquipSelect:getFocused()) then
        return false
    end
    self.model._windowEquipSelect:returnToSlotWindow()
    return true
end

function WindowMenuController:_returnSaveSlotToCommand()
    if not self.model._windowSaveLoad:getVisible() then
        return false
    end
    local slotWindow = self.model._windowSaveLoad:getSlotWindow()
    if not (slotWindow:getActive() or slotWindow:getFocused()) then
        return false
    end
    return self.model._windowSaveLoad:returnToCommandWindow()
end

local FinalWindowMenuController = class(WindowMenuController, WindowCommand.Controller)

---@class Source.Windows.WindowMenu
local WindowMenu = {}

WindowMenu.controllerClass = FinalWindowMenuController

function WindowMenu:init(player, windows)
    self._player = player
    self._windowItem = windows.item
    self._windowEquipSlot = windows.equipSlot
    self._windowEquipSelect = windows.equipSelect
    self._windowEquipStatus = windows.equipStatus
    self._windowSaveLoad = windows.saveLoad
    local commands = FinalWindowMenuController.createCommands(self)
    super(WindowMenu, self).init(Engine.ToIntRect(0, 0, 192, 160), commands)
    self._menuController = self._commandController
    ---@cast self._menuController Source.Windows.WindowMenu.Controller
    self._menuControls = self._menuController:getMenuControls()
end

function WindowMenu:setMoveRestoreGuard(guard)
    self._menuController:setMoveRestoreGuard(guard)
end

function WindowMenu:onKeyDown(kwargs)
    if self._menuController:handleKeyDown() then
        return
    end
    return super(WindowMenu, self).onKeyDown(kwargs)
end

function WindowMenu:onMouseButtonDown(kwargs)
    return self._menuController:handleMouseButtonDown(kwargs)
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

function WindowMenu:close()
    self._menuController:close()
end

function WindowMenu:isBlocking()
    return self._menuController:isBlocking()
end

function WindowMenu:_closeByCancel()
    self._menuController:_closeByCancel()
end

function WindowMenu:_handleCancel()
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

function WindowMenu:onSaveLoadClose()
    self._menuController:onSaveLoadClose()
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

---@return boolean
function WindowMenu:_returnSaveSlotToCommand()
    return self._menuController:_returnSaveSlotToCommand()
end

return class(WindowMenu, WindowCommand)
