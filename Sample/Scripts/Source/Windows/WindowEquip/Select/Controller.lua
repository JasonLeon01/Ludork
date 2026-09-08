local GlobalCore = require("GlobalCore")
local Data = require("Source.Data")
local GameSystem = require("Source.System")
local IconTexture = require("Source.UI.IconTexture")
local EquipItemRowUI = require("Source.UI.Parts.WindowEquip.EquipItemRow")
local ListViewController = require("Source.UI.Helpers.ListView")

local AudioManager = GlobalCore.AudioManager

---@class (partial) Source.Windows.WindowEquip.Select.Controller
local WindowEquipSelectController = {}

WindowEquipSelectController.CELL_SIZE = 32

WindowEquipSelectController.UNEQUIP = {}

function WindowEquipSelectController:init(model, player, windowEquipSlot, windowEquipStatus, onEquip)
    self._player = player
    self._windowEquipSlot = windowEquipSlot
    self._windowEquipStatus = windowEquipStatus
    self._onEquipCallback = onEquip
    self._slotKey = ""
    self._equipList = {}
    self._equipCounts = {}
    self._lastStatusIndex = nil
    self._logicalSize = nil
    self._rowUIs = {}
    super
        (WindowEquipSelectController, self)
        .init(model, sf.Vector2u.new(1, 1), WindowEquipSelectController.CELL_SIZE, true, 1)
end

function WindowEquipSelectController:attach(listView)
    self:_updateLayout()
    if listView ~= nil then
        self.root = listView
        self.root:clearChildren()
    end
    local root = self:prepare(self._logicalSize)
    self.model:setListView(root)
end

---@diagnostic disable-next-line: unused
function WindowEquipSelectController:resizeCanvas(target, width, height)
    local logicalSize = sf.Vector2u.new(width, height)
    ---@cast logicalSize sf.Vector2u
    target:resize(logicalSize)
    target:setView(target:getDefaultView())
end

function WindowEquipSelectController:refreshForSlot(slotKey)
    self._slotKey = slotKey
    self:_updateLayout()
    self.root:clearChildren()
    self._rowUIs = {}
    local equipData = Data.GetAllGeneralEquipData()
    local playerEquips = self._player:getEquips()
    local currentEquipped = self._player:getEquipInfo(slotKey)
    local orderedEquips = {}
    if bool(currentEquipped) then
        orderedEquips[#orderedEquips + 1] = self.UNEQUIP
    end
    self._equipCounts = {}
    for _, equipID in ipairs(table.orderedStringKeys(equipData)) do
        local equip = equipData[equipID] or {}
        if playerEquips[equipID] ~= nil and equip.slot == slotKey then
            orderedEquips[#orderedEquips + 1] = equipID
            self._equipCounts[equipID] = playerEquips[equipID]
        end
    end
    self._equipList = orderedEquips
    for _, entry in ipairs(orderedEquips) do
        local iconTexture = nil
        local count = 0
        if entry ~= self.UNEQUIP then
            local member = equipData[entry] or {}
            iconTexture = IconTexture.Load(member.icon or "")
            count = self._equipCounts[entry] or 1
        end
        local rowUI = EquipItemRowUI.new({
            iconTexture = iconTexture,
            count = count
        })
        local cell = rowUI:prepare(
            sf.Vector2u.new(WindowEquipSelectController.CELL_SIZE, WindowEquipSelectController.CELL_SIZE)
        )
        cell:addConfirmCallback(function ()
            self:onConfirmAction()
        end)
        self._rowUIs[#self._rowUIs + 1] = rowUI
        self.model:applyItem(cell)
        self.root:addChild(cell)
    end
    self:prepare(self._logicalSize)
    self.model:setListView(self.root)
    self.model:resetSelection()
    self._lastStatusIndex = nil
    if self.model:getActive() then
        self:updateStatus()
    end
end

function WindowEquipSelectController:tick()
    if self._lastStatusIndex == self.model.index then
        return
    end
    self._lastStatusIndex = self.model.index
    if self.model:getActive() then
        self:updateStatus()
    end
end

function WindowEquipSelectController:updateStatus()
    if self._windowEquipStatus == nil then
        return
    end
    if self.model.index == nil or self.model.index < 0 or self.model.index >= #self._equipList then
        self._windowEquipStatus:refreshForEquip(self._slotKey, nil)
        return
    end
    local showUnequip = self._equipList[self.model.index + 1] == self.UNEQUIP
    if showUnequip then
        self._windowEquipStatus:refreshForEquip(self._slotKey, nil, true)
    else
        self._windowEquipStatus:refreshForEquip(self._slotKey, self._equipList[self.model.index + 1], false)
    end
end

---@diagnostic disable-next-line: unused
function WindowEquipSelectController:getGridColumns(contentWidth)
    return math.max(1, math.floor(contentWidth / WindowEquipSelectController.CELL_SIZE))
end

function WindowEquipSelectController:returnToSlotWindow(playSE)
    if playSE == nil then
        playSE = true
    end
    if playSE then
        AudioManager.playSound(GameSystem.GetCancelSE())
    end
    self.model:setActive(false)
    self.model:setVisible(true)
    if self._windowEquipSlot ~= nil then
        self._windowEquipSlot:setActive(true)
        self._windowEquipSlot:requestKeyboardFocus()
    end
    if self._windowEquipStatus ~= nil and bool(self._slotKey) then
        self._windowEquipStatus:refreshForSlot(self._slotKey)
    end
end

function WindowEquipSelectController:closeByCancel()
    self:returnToSlotWindow()
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
    if self.model.index == nil or self.model.index < 0 or self.model.index >= #self._equipList then
        return
    end
    local equipID = assert(self._equipList[self.model.index + 1])
    local currentEquipped = self._player:getEquipInfo(self._slotKey)
    AudioManager.playSound(GameSystem.GetEquipSE())
    if equipID == self.UNEQUIP or equipID == currentEquipped then
        if bool(currentEquipped) then
            self._player:unequip(self._slotKey)
        end
    else
        ---@cast equipID string
        self._player:equip(equipID)
    end
    if self._windowEquipSlot ~= nil then
        self._windowEquipSlot:refreshSlots()
    end
    self:refreshForSlot(self._slotKey)
    if self._onEquipCallback ~= nil then
        self._onEquipCallback()
    end
end

function WindowEquipSelectController:_updateLayout()
    local windowSize = self.model:getSize()
    local contentWidth = math.max(1, math.floor(windowSize.x - 32))
    local contentHeight = math.max(1, math.floor(windowSize.y - 32))
    self:resizeCanvas(self.model.content, contentWidth, contentHeight)
    local logicalSize = sf.Vector2u.new(contentWidth, contentHeight)
    ---@cast logicalSize sf.Vector2u
    self._logicalSize = logicalSize
    self._columns = self:getGridColumns(contentWidth)
    self.root:setColumns(self._columns)
end

function WindowEquipSelectController:setPlayer(player)
    self._player = player
end

function WindowEquipSelectController:setEquipStatusWindow(windowEquipStatus)
    self._windowEquipStatus = windowEquipStatus
end

function WindowEquipSelectController:setEquipSlotWindow(windowEquipSlot)
    self._windowEquipSlot = windowEquipSlot
end

return class(WindowEquipSelectController, ListViewController)
