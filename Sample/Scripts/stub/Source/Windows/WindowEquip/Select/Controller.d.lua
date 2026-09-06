---@meta Source.Windows.WindowEquip.Select.Controller

---@class Source.Windows.WindowEquip.Select.Controller: Source.UI.Helpers.ListView
---@field model        Source.Windows.WindowEquipSelect
---@field _logicalSize sf.Vector2u | nil
---@field _rowUIs      Source.UI.UiController[]
---@field CELL_SIZE    integer
---@field UNEQUIP      table
---@field new          fun(model: Source.Windows.WindowEquipSelect): Source.Windows.WindowEquip.Select.Controller
local WindowEquipSelectController = {}

---@param model Source.Windows.WindowEquipSelect
function WindowEquipSelectController:init(model) end

---@param listView Engine.ListView | nil
function WindowEquipSelectController:attach(listView) end

---@param target Engine.Canvas
---@param width  integer
---@param height integer
function WindowEquipSelectController:resizeCanvas(target, width, height) end

---@param slotKey string
function WindowEquipSelectController:refreshForSlot(slotKey) end

function WindowEquipSelectController:tick() end

function WindowEquipSelectController:updateStatus() end

---@param contentWidth integer
---@return integer
function WindowEquipSelectController:getGridColumns(contentWidth) end

---@param playSE boolean | nil
function WindowEquipSelectController:returnToSlotWindow(playSE) end

function WindowEquipSelectController:closeByCancel() end

function WindowEquipSelectController:open() end

function WindowEquipSelectController:close() end

function WindowEquipSelectController:onConfirmAction() end

function WindowEquipSelectController:_updateLayout() end

return WindowEquipSelectController
