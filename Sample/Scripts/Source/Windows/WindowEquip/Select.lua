local WindowEquipSelectUI = require("Source.UI.Parts.WindowEquip.WindowEquipSelect")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local WindowEquipSelectController = require("Source.Windows.WindowEquip.Select.Controller")

---@class (partial) Source.Windows.WindowEquipSelect
local WindowEquipSelect = {}

WindowEquipSelect.controllerClass = WindowEquipSelectController

function WindowEquipSelect:init(rect, player, windowEquipSlot, windowEquipStatus, onEquip, instance)
    super
        (WindowEquipSelect, self)
        .init(rect, nil, WindowEquipSelectController.CELL_SIZE, WindowEquipSelectController.CELL_SIZE, nil, nil, nil, nil, instance
            ~= nil)
    self:setHasReturnBtn(true)
    self._player = player
    self._windowEquipSlot = windowEquipSlot
    self._windowEquipStatus = windowEquipStatus
    self._onEquipCallback = onEquip
    self._slotKey = ""
    self._equipList = {}
    self._equipCounts = {}
    self._lastStatusIndex = nil
    if instance ~= nil then
        local ui = WindowEquipSelectUI.new(self, instance)
        self._ui = ui
        ui:attach()
        self:setScrollBox(ui:getScrollBox())
        self:setListView(ui:getListView())
    end
    self._selectController = self.controllerClass.new(self)
    self._selectController:attach(self:getListView())
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

function WindowEquipSelect:open()
    self._selectController:open()
end

function WindowEquipSelect:close()
    self._selectController:close()
end

function WindowEquipSelect:_onConfirmAction()
    self._selectController:onConfirmAction()
end

return class(WindowEquipSelect, WindowSelectable)
