local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local GameSystem = require("Source.System")
local Data = require("Source.Data")
local LocaleCore = require("Source.Locale.Core")
local EquipItemRowUI = require("Source.UI.Parts.WindowEquip.EquipItemRow")
local EquipSlotRowUI = require("Source.UI.Parts.WindowEquip.EquipSlotRow")
local EquipViewUtils = require("Source.UI.Parts.WindowEquip.EquipView")
local ListViewController = require("Source.UI.Helpers.ListView")
local WindowEquipStatusUI = require("Source.UI.Parts.WindowEquip.WindowEquipStatus")
local WindowBase = require("Source.Windows.Base.WindowBase")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local Input = Engine.Input
local ManagerFunctions = GlobalFunctions.Manager
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local _SLOT_ROW_HEIGHT = 32
local _EQUIP_CELL_SIZE = 32
local _EQUIP_ORDER = { "Sword_A", "Shield_A" }
local _SLOT_ORDER = { "weapon", "shield", "accessory" }

local WindowEquipSlotController = {}

function WindowEquipSlotController:init(model)
    self._logicalSize = nil
    self._rowUIs = {}
    super(WindowEquipSlotController, self).init(model, sf.Vector2u.new(1, 1), _SLOT_ROW_HEIGHT, true, 1)
end

function WindowEquipSlotController:attach()
    self:_refreshLogicalSize()
    local listView = self:prepare(self._logicalSize)
    self.model:setListView(listView)
end

function WindowEquipSlotController:getSlotCellData(slotKey)
    local equipID = self.model._player:getEquipInfo(slotKey)
    if not bool(equipID) then
        return nil, LOC("EQUIP_UNEQUIPPED")
    end
    local equipInfo = Data.getGeneralEquipData(equipID)
    local name = equipInfo.name or ""
    local label = bool(name) and LOC(name) or equipID
    return EquipViewUtils.loadIcon(equipInfo.icon or ""), label
end

function WindowEquipSlotController:refreshSlots()
    local savedSlotKey = self:getCurrentSlotKey()
    local classData = Data.getGeneralClassData(self.model._player.infoComp.CLASS)
    local classSlots = classData.slot
    if classSlots == nil then
        classSlots = {}
    end
    self.model._slotKeys = table.orderedStringKeys(classSlots, _SLOT_ORDER)
    self:_refreshLogicalSize()
    self._columns = 1
    self.root:setColumns(self._columns)
    self.root:clearChildren()
    self._rowUIs = {}
    local cellWidth = self.model:_getRectWidth()
    for _, slotKey in ipairs(self.model._slotKeys) do
        local iconTexture, label = self:getSlotCellData(slotKey)
        local rowUI = EquipSlotRowUI.new({
            label = label,
            iconTexture = iconTexture
        })
        local child = rowUI:prepare(sf.Vector2u.new(cellWidth, _SLOT_ROW_HEIGHT))
        child:addConfirmCallback(function ()
            self:focusSelectWindow()
        end)
        self._rowUIs[#self._rowUIs + 1] = rowUI
        self.model:_applyItem(child)
        self.root:addChild(child)
    end
    self:prepare(self._logicalSize)
    self.model:setListView(self.root)
    if savedSlotKey ~= nil then
        local restoredIndex = nil
        for index, slotKey in ipairs(self.model._slotKeys) do
            if slotKey == savedSlotKey then
                restoredIndex = index - 1
                break
            end
        end
        self.model.index = restoredIndex
    else
        self.model.index = bool(self.model._slotKeys) and 0 or nil
    end
    if self.model.index == nil and bool(self.model._slotKeys) then
        self.model.index = 0
    end
    self.model._lastSlotIndex = self.model.index
    if self.model._rect:getParent() ~= nil then
        self.model.content:removeChild(self.model._rect)
    end
    self:redrawIfVisible()
end

function WindowEquipSlotController:refreshLocale()
    if not self.model:getVisible() then
        return
    end
    self:refreshSlots()
    local slotKey = self:getCurrentSlotKey()
    if slotKey == nil then
        return
    end
    if self.model._windowEquipSelect ~= nil and self.model._windowEquipSelect:getActive() then
        self.model._windowEquipSelect:updateStatus()
        return
    end
    if self.model._windowEquipStatus ~= nil then
        self.model._windowEquipStatus:refreshForSlot(slotKey)
    end
end

function WindowEquipSlotController:redrawIfVisible()
    if not self.model:getVisible() then
        return
    end
    local wasActive = self.model:getActive()
    local returnButtonSuppressed = self.model._returnButtonSuppressed
    if not wasActive then
        self.model:_setReturnButtonSuppressed(true)
        self.model:setActive(true)
    end
    self.model:update(0.0)
    self.model:render()
    if not wasActive then
        self.model:setActive(false)
        self.model:_setReturnButtonSuppressed(returnButtonSuppressed)
    end
end

function WindowEquipSlotController:getCurrentSlotKey()
    if self.model.index == nil or self.model.index < 0 or self.model.index >= #self.model._slotKeys then
        return nil
    end
    return self.model._slotKeys[self.model.index + 1]
end

function WindowEquipSlotController:notifySlotChanged()
    local slotKey = self:getCurrentSlotKey()
    if slotKey == nil then
        return
    end
    if self.model._windowEquipStatus ~= nil and self.model._windowEquipStatus:getVisible() then
        self.model._windowEquipStatus:refreshForSlot(slotKey)
    end
    if self.model._windowEquipSelect ~= nil and self.model._windowEquipSelect:getVisible() then
        self.model._windowEquipSelect:refreshForSlot(slotKey)
    end
end

function WindowEquipSlotController:tick()
    if self.model._lastSlotIndex == self.model.index then
        return
    end
    self.model._lastSlotIndex = self.model.index
    self:notifySlotChanged()
end

function WindowEquipSlotController:focusSelectWindow()
    if self.model._windowEquipSelect == nil then
        return
    end
    ManagerFunctions.playSE(GameSystem.getDecisionSE())
    local slotKey = self:getCurrentSlotKey()
    if slotKey ~= nil then
        self.model._windowEquipSelect:refreshForSlot(slotKey)
    end
    if self.model._windowEquipStatus ~= nil and slotKey ~= nil then
        self.model._windowEquipStatus:setVisible(true)
        self.model._windowEquipStatus:refreshForSlot(slotKey)
    end
    self.model:setActive(false)
    self.model._windowEquipSelect:setVisible(true)
    self.model._windowEquipSelect:setActive(true)
    self.model._windowEquipSelect:updateStatus()
    self.model._windowEquipSelect:requestKeyboardFocusAtCursor()
end

function WindowEquipSlotController:handleKeyDown()
    if not Input.isActionTriggered(Input.getCancelKeys(), false) then
        return false
    end
    self.model:onReturn()
    Input.isActionTriggered(Input.getCancelKeys(), true)
    return true
end

function WindowEquipSlotController:handleMouseButtonDown(kwargs)
    if kwargs.button ~= sf.Mouse.Button.Right then
        return false
    end
    self.model:onReturn()
    return true
end

function WindowEquipSlotController:open()
    self:refreshSlots()
    self.model:setVisible(true)
    self.model:setActive(true)
    local slotKey = self:getCurrentSlotKey()
    if slotKey == nil then
        self:closeChildWindows()
        return
    end
    if self.model._windowEquipStatus ~= nil then
        self.model._windowEquipStatus:openForSlot(slotKey)
    end
    if self.model._windowEquipSelect ~= nil then
        self.model._windowEquipSelect:refreshForSlot(slotKey)
        self.model._windowEquipSelect:open()
    end
end

function WindowEquipSlotController:closeChildWindows()
    if self.model._windowEquipStatus ~= nil then
        self.model._windowEquipStatus:close()
    end
    if self.model._windowEquipSelect ~= nil then
        self.model._windowEquipSelect:close()
    end
end

function WindowEquipSlotController:close()
    self.model:setVisible(false)
    self.model:setActive(false)
    self:closeChildWindows()
end

function WindowEquipSlotController:closeByCancel()
    ManagerFunctions.playSE(GameSystem.getCancelSE())
    self:close()
    if self.model._onCloseCallback ~= nil then
        self.model._onCloseCallback()
    end
end

function WindowEquipSlotController:_refreshLogicalSize()
    local contentSize = self.model.content:getSize()
    self._logicalSize = sf.Vector2u.new(math.max(1, math.floor(contentSize.x)), math.max(1, math.floor(contentSize.y)))
end

local FinalWindowEquipSlotController = class(WindowEquipSlotController, ListViewController)

local WindowEquipSelectController = {}

WindowEquipSelectController.UNEQUIP = {}

function WindowEquipSelectController:init(model)
    self._logicalSize = nil
    self._rowUIs = {}
    super(WindowEquipSelectController, self).init(model, sf.Vector2u.new(1, 1), _EQUIP_CELL_SIZE, true, 1)
end

function WindowEquipSelectController:attach()
    self:_updateLayout()
    local listView = self:prepare(self._logicalSize)
    self.model:setListView(listView)
end

function WindowEquipSelectController:resizeCanvas(target, width, height)
    local _ = self

    local logicalSize = sf.Vector2u.new(width, height)
    ---@cast logicalSize sf.Vector2u
    target:resize(logicalSize)
    target:setView(target:getDefaultView())
end

function WindowEquipSelectController:refreshForSlot(slotKey)
    self.model._slotKey = slotKey
    self:_updateLayout()
    self.root:clearChildren()
    self._rowUIs = {}
    local equipData = Data.getAllGeneralEquipData()
    local playerEquips = self.model._player._equips or {}
    local currentEquipped = self.model._player:getEquipInfo(slotKey)
    local orderedEquips = {}
    if bool(currentEquipped) then
        orderedEquips[#orderedEquips + 1] = self.UNEQUIP
    end
    self.model._equipCounts = {}
    for _, equipID in ipairs(table.orderedStringKeys(equipData, _EQUIP_ORDER)) do
        local equip = equipData[equipID] or {}
        if playerEquips[equipID] ~= nil and equip.slot == slotKey then
            orderedEquips[#orderedEquips + 1] = equipID
            self.model._equipCounts[equipID] = playerEquips[equipID]
        end
    end
    self.model._equipList = orderedEquips
    for _, entry in ipairs(orderedEquips) do
        local iconTexture = nil
        local count = 0
        if entry ~= self.UNEQUIP then
            local member = equipData[entry] or {}
            iconTexture = EquipViewUtils.loadIcon(member.icon or "")
            count = self.model._equipCounts[entry] or 1
        end
        local rowUI = EquipItemRowUI.new({
            iconTexture = iconTexture,
            count = count
        })
        local cell = rowUI:prepare(sf.Vector2u.new(_EQUIP_CELL_SIZE, _EQUIP_CELL_SIZE))
        cell:addConfirmCallback(function ()
            self:onConfirmAction()
        end)
        self._rowUIs[#self._rowUIs + 1] = rowUI
        self.model:_applyItem(cell)
        self.root:addChild(cell)
    end
    self:prepare(self._logicalSize)
    self.model:setListView(self.root)
    self.model.index = bool(self.root:getChildren()) and 0 or nil
    self.model._lastStatusIndex = nil
    if self.model:getActive() then
        self:updateStatus()
    end
end

function WindowEquipSelectController:tick()
    if self.model._lastStatusIndex == self.model.index then
        return
    end
    self.model._lastStatusIndex = self.model.index
    if self.model:getActive() then
        self:updateStatus()
    end
end

function WindowEquipSelectController:updateStatus()
    if self.model._windowEquipStatus == nil then
        return
    end
    if self.model.index == nil or self.model.index < 0 or self.model.index >= #self.model._equipList then
        self.model._windowEquipStatus:refreshForEquip(self.model._slotKey, nil)
        return
    end
    local entry = self.model._equipList[self.model.index + 1]
    local showUnequip = entry == self.UNEQUIP
    local candidateEquipID = entry
    if showUnequip then
        candidateEquipID = nil
    end
    self.model._windowEquipStatus:refreshForEquip(self.model._slotKey, candidateEquipID, showUnequip)
end

function WindowEquipSelectController:getGridColumns(contentWidth)
    local _ = self

    return math.max(1, math.floor((contentWidth - _EQUIP_CELL_SIZE) / _EQUIP_CELL_SIZE))
end

function WindowEquipSelectController:returnToSlotWindow(playSE)
    if playSE == nil then
        playSE = true
    end
    if playSE then
        ManagerFunctions.playSE(GameSystem.getCancelSE())
    end
    self.model:setActive(false)
    self.model:setVisible(true)
    if self.model._windowEquipSlot ~= nil then
        self.model._windowEquipSlot:setActive(true)
        self.model._windowEquipSlot:requestKeyboardFocus()
    end
    if self.model._windowEquipStatus ~= nil and bool(self.model._slotKey) then
        self.model._windowEquipStatus:refreshForSlot(self.model._slotKey)
    end
end

function WindowEquipSelectController:closeByCancel()
    self:returnToSlotWindow()
end

function WindowEquipSelectController:handleKeyDown()
    if not Input.isActionTriggered(Input.getCancelKeys(), false) then
        return false
    end
    self.model:onReturn()
    Input.isActionTriggered(Input.getCancelKeys(), true)
    return true
end

function WindowEquipSelectController:handleMouseButtonDown(kwargs)
    if kwargs.button ~= sf.Mouse.Button.Right then
        return false
    end
    self.model:onReturn()
    return true
end

function WindowEquipSelectController:open()
    self.model:setVisible(true)
    self.model:setActive(false)
end

function WindowEquipSelectController:close()
    self.model:setVisible(false)
    self.model:setActive(false)
end

function WindowEquipSelectController:onConfirmAction()
    if self.model.index == nil or self.model.index < 0 or self.model.index >= #self.model._equipList then
        return
    end
    local entry = self.model._equipList[self.model.index + 1]
    local slotKey = self.model._slotKey
    local currentEquipped = self.model._player:getEquipInfo(slotKey)
    ManagerFunctions.playSE(GameSystem.getEquipSE())
    if entry == self.UNEQUIP or entry == currentEquipped then
        if bool(currentEquipped) then
            self.model._player:unequip(slotKey)
        end
    else
        self.model._player:equip(entry)
    end
    if self.model._windowEquipSlot ~= nil then
        self.model._windowEquipSlot:refreshSlots()
    end
    self:refreshForSlot(slotKey)
    if self.model._onEquipCallback ~= nil then
        self.model._onEquipCallback()
    end
end

function WindowEquipSelectController:_updateLayout()
    local windowSize = self.model:getSize()
    local contentWidth = math.max(1, math.floor(windowSize.x - 32))
    local contentHeight = math.max(1, math.floor(windowSize.y - 32))
    self:resizeCanvas(self.model.content, contentWidth, contentHeight)
    self._logicalSize = sf.Vector2u.new(contentWidth, contentHeight)
    self._columns = self:getGridColumns(contentWidth)
    self.root:setColumns(self._columns)
end

local FinalWindowEquipSelectController = class(WindowEquipSelectController, ListViewController)

---@class Source.Windows.WindowEquipStatus: Source.Windows.Base.WindowBase
local WindowEquipStatus = {}

WindowEquipStatus.uiClass = WindowEquipStatusUI

function WindowEquipStatus:init(rect, player)
    super(WindowEquipStatus, self).init(rect)
    self._player = player
    self._slotKey = ""
    self._changeTexts = {}
    self._statusUI = self.uiClass.new(self)
    self._statusUI:attach()
    self:setActive(false)
    self:setVisible(false)
end

function WindowEquipStatus:setPlayer(player)
    self._player = player
    self._statusUI:setPlayer(player)
end

function WindowEquipStatus:openForSlot(slotKey)
    self._statusUI:openForSlot(slotKey)
end

function WindowEquipStatus:close()
    self._statusUI:close()
end

function WindowEquipStatus:refreshForEquip(slotKey, candidateEquipID, showUnequip)
    self._statusUI:refreshForEquip(slotKey, candidateEquipID, showUnequip)
end

function WindowEquipStatus:refreshForSlot(slotKey)
    self._statusUI:refreshForSlot(slotKey)
end

---@param currentAttrs   table
---@param candidateAttrs table
function WindowEquipStatus:_refreshChangeRows(currentAttrs, candidateAttrs)
    self._statusUI:refreshChangeRows(currentAttrs, candidateAttrs)
end

---@param attrKey  string
---@param delta    integer
---@param rowIndex integer
function WindowEquipStatus:_addChangeRow(attrKey, delta, rowIndex)
    self._statusUI:addChangeRow(attrKey, delta, rowIndex)
end

---@param candidateEquipID string | nil
---@param showUnequip      boolean
function WindowEquipStatus:_refreshDescription(candidateEquipID, showUnequip)
    self._statusUI:refreshDescription(candidateEquipID, showUnequip)
end

function WindowEquipStatus:_clearChangeTexts()
    self._statusUI:clearChangeTexts()
end

---@param nameY integer
---@param descY integer
function WindowEquipStatus:_setDescriptionPosition(nameY, descY)
    self._statusUI:setDescriptionPosition(nameY, descY)
end

---@param equipID string | nil
---@return table
function WindowEquipStatus:_getAttrPlus(equipID)
    return self._statusUI:getAttrPlus(equipID)
end

---@param firstAttrs  table
---@param secondAttrs table
---@return table
function WindowEquipStatus:_getAttrKeys(firstAttrs, secondAttrs)
    return self._statusUI:getAttrKeys(firstAttrs, secondAttrs)
end

---@param text Engine.PlainText
---@param y    number
function WindowEquipStatus:_setRightAligned(text, y)
    self._statusUI:setRightAligned(text, y)
end

local FinalWindowEquipStatus = class(WindowEquipStatus, WindowBase)

local WindowEquipSlot = {}

WindowEquipSlot.controllerClass = FinalWindowEquipSlotController

function WindowEquipSlot:init(rect, player, windowEquipSelect, windowEquipStatus, onClose)
    super(WindowEquipSlot, self).init(rect, nil, nil, _SLOT_ROW_HEIGHT)
    self:setHasReturnBtn(true)
    self._onCloseCallback = onClose
    self._player = player
    self._windowEquipSelect = windowEquipSelect
    self._windowEquipStatus = windowEquipStatus
    self._slotKeys = {}
    self._lastSlotIndex = nil
    self._slotController = self.controllerClass.new(self)
    self._slotController:attach()
    self:refreshSlots()
    self:setActive(false)
    self:setVisible(false)
end

function WindowEquipSlot:setEquipSelectWindow(windowEquipSelect)
    self._windowEquipSelect = windowEquipSelect
end

function WindowEquipSlot:setPlayer(player)
    self._player = player
end

function WindowEquipSlot:setEquipStatusWindow(windowEquipStatus)
    self._windowEquipStatus = windowEquipStatus
end

---@param slotKey string
---@return sf.Texture | nil, string
function WindowEquipSlot:_getSlotCellData(slotKey)
    local texture, label = self._slotController:getSlotCellData(slotKey)
    return texture, label
end

function WindowEquipSlot:refreshSlots()
    self._slotController:refreshSlots()
end

function WindowEquipSlot:refreshLocale()
    self._slotController:refreshLocale()
end

-- Force redraw while visible even when inactive.
function WindowEquipSlot:_redrawIfVisible()
    self._slotController:redrawIfVisible()
end

---@return string | nil
function WindowEquipSlot:_getCurrentSlotKey()
    return self._slotController:getCurrentSlotKey()
end

function WindowEquipSlot:_notifySlotChanged()
    self._slotController:notifySlotChanged()
end

function WindowEquipSlot:onTick(deltaTime)
    super(WindowEquipSlot, self).onTick(deltaTime)
    self._slotController:tick()
end

function WindowEquipSlot:_focusSelectWindow()
    self._slotController:focusSelectWindow()
end

function WindowEquipSlot:onKeyDown(kwargs)
    if self._slotController:handleKeyDown() then
        return
    end
    return super(WindowEquipSlot, self).onKeyDown(kwargs)
end

function WindowEquipSlot:onMouseButtonDown(kwargs)
    return self._slotController:handleMouseButtonDown(kwargs)
end

function WindowEquipSlot:open()
    self._slotController:open()
end

function WindowEquipSlot:_closeChildWindows()
    self._slotController:closeChildWindows()
end

function WindowEquipSlot:close()
    self._slotController:close()
end

function WindowEquipSlot:onReturn()
    self._slotController:closeByCancel()
end

local FinalWindowEquipSlot = class(WindowEquipSlot, WindowSelectable)

local WindowEquipSelect = {}

WindowEquipSelect.controllerClass = FinalWindowEquipSelectController

function WindowEquipSelect:init(rect, player, windowEquipSlot, windowEquipStatus, onEquip)
    super(WindowEquipSelect, self).init(rect, nil, _EQUIP_CELL_SIZE, _EQUIP_CELL_SIZE)
    self:setHasReturnBtn(true)
    self._player = player
    self._windowEquipSlot = windowEquipSlot
    self._windowEquipStatus = windowEquipStatus
    self._onEquipCallback = onEquip
    self._slotKey = ""
    self._equipList = {}
    self._equipCounts = {}
    self._lastStatusIndex = nil
    self._selectController = self.controllerClass.new(self)
    self._selectController:attach()
    self:setActive(false)
    self:setVisible(false)
end

function WindowEquipSelect:setEquipSlotWindow(windowEquipSlot)
    self._windowEquipSlot = windowEquipSlot
end

function WindowEquipSelect:setPlayer(player)
    self._player = player
end

function WindowEquipSelect:setEquipStatusWindow(windowEquipStatus)
    self._windowEquipStatus = windowEquipStatus
end

---@param target Engine.Canvas
---@param width  integer
---@param height integer
function WindowEquipSelect:_resizeCanvas(target, width, height)
    self._selectController:resizeCanvas(target, width, height)
end

function WindowEquipSelect:refreshForSlot(slotKey)
    self._selectController:refreshForSlot(slotKey)
end

function WindowEquipSelect:onTick(deltaTime)
    super(WindowEquipSelect, self).onTick(deltaTime)
    self._selectController:tick()
end

function WindowEquipSelect:updateStatus()
    self._selectController:updateStatus()
end

---@param contentWidth integer
---@return integer
function WindowEquipSelect:_getGridColumns(contentWidth)
    return self._selectController:getGridColumns(contentWidth)
end

function WindowEquipSelect:returnToSlotWindow(playSE)
    self._selectController:returnToSlotWindow(playSE)
end

function WindowEquipSelect:onReturn()
    self._selectController:closeByCancel()
end

function WindowEquipSelect:onKeyDown(kwargs)
    if self._selectController:handleKeyDown() then
        return
    end
    return super(WindowEquipSelect, self).onKeyDown(kwargs)
end

function WindowEquipSelect:onMouseButtonDown(kwargs)
    return self._selectController:handleMouseButtonDown(kwargs)
end

function WindowEquipSelect:open()
    self._selectController:open()
end

function WindowEquipSelect:close()
    self._selectController:close()
end

function WindowEquipSelect:_onConfirmAction()
    self._selectController:onConfirmAction()
end

local FinalWindowEquipSelect = class(WindowEquipSelect, WindowSelectable)

local WindowEquipExports = {
    WindowEquipStatus = FinalWindowEquipStatus,
    WindowEquipSlot = FinalWindowEquipSlot,
    WindowEquipSelect = FinalWindowEquipSelect
}

return WindowEquipExports
