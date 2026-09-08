---@meta Source.Windows.WindowEquip.Slot.Controller

---@class Source.Windows.WindowEquip.Slot.Controller: Source.UI.Helpers.ListView
---@field model              Source.Windows.WindowEquipSlot
---@field _logicalSize       sf.Vector2u | nil
---@field _rowUIs            Source.UI.UiController[]
---@field ROW_HEIGHT         integer
---@field new                fun(model: Source.Windows.WindowEquipSlot, player: Source.Player.Player, windowEquipSelect: Source.Windows.WindowEquipSelect | nil, windowEquipStatus: Source.Windows.WindowEquipStatus | nil, onClose: function | nil): Source.Windows.WindowEquip.Slot.Controller
---@field _player            Source.Player.Player
---@field _windowEquipSelect Source.Windows.WindowEquipSelect | nil
---@field _windowEquipStatus Source.Windows.WindowEquipStatus | nil
---@field _onCloseCallback   function | nil
---@field _slotKeys          string[]
---@field _lastSlotIndex     integer | nil
local WindowEquipSlotController = {}

---@param model             Source.Windows.WindowEquipSlot
---@param player            Source.Player.Player
---@param windowEquipSelect Source.Windows.WindowEquipSelect | nil
---@param windowEquipStatus Source.Windows.WindowEquipStatus | nil
---@param onClose           function | nil
function WindowEquipSlotController:init(model, player, windowEquipSelect, windowEquipStatus, onClose) end

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

---@param player Source.Player.Player
function WindowEquipSlotController:setPlayer(player) end

---@param windowEquipStatus Source.Windows.WindowEquipStatus | nil
function WindowEquipSlotController:setEquipStatusWindow(windowEquipStatus) end

---@param windowEquipSelect Source.Windows.WindowEquipSelect | nil
function WindowEquipSlotController:setEquipSelectWindow(windowEquipSelect) end

---@param callback function | nil
function WindowEquipSlotController:setOnCloseCallback(callback) end

return WindowEquipSlotController
