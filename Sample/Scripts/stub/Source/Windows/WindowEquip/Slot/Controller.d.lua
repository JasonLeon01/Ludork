---@meta Source.Windows.WindowEquip.Slot.Controller

---@class Source.Windows.WindowEquip.Slot.Controller: Source.UI.Helpers.ListView
---@field model        Source.Windows.WindowEquipSlot
---@field _logicalSize sf.Vector2u | nil
---@field _rowUIs      Source.UI.UiController[]
---@field ROW_HEIGHT   integer
---@field new          fun(model: Source.Windows.WindowEquipSlot): Source.Windows.WindowEquip.Slot.Controller
local WindowEquipSlotController = {}

---@param model Source.Windows.WindowEquipSlot
function WindowEquipSlotController:init(model) end

---@param listView Engine.ListView | nil
function WindowEquipSlotController:attach(listView) end

---@param slotKey string
---@return sf.Texture | nil, string
function WindowEquipSlotController:getSlotCellData(slotKey) end

function WindowEquipSlotController:refreshSlots() end

function WindowEquipSlotController:refreshLocale() end

function WindowEquipSlotController:redrawIfVisible() end

---@return string | nil
function WindowEquipSlotController:getCurrentSlotKey() end

function WindowEquipSlotController:notifySlotChanged() end

function WindowEquipSlotController:tick() end

function WindowEquipSlotController:focusSelectWindow() end

function WindowEquipSlotController:open() end

function WindowEquipSlotController:closeChildWindows() end

function WindowEquipSlotController:close() end

function WindowEquipSlotController:closeByCancel() end

function WindowEquipSlotController:_refreshLogicalSize() end

return WindowEquipSlotController
