---@meta Source.Windows.WindowEquip.Select.Controller

---@class Source.Windows.WindowEquip.Select.Controller: Source.UI.Helpers.ListView
---@field model              Source.Windows.WindowEquipSelect
---@field _logicalSize       sf.Vector2u | nil
---@field _rowUIs            Source.UI.UiController[]
---@field CELL_SIZE          integer
---@field UNEQUIP            table
---@field new                fun(model: Source.Windows.WindowEquipSelect, player: Source.Player.Player, windowEquipSlot: Source.Windows.WindowEquipSlot | nil, windowEquipStatus: Source.Windows.WindowEquipStatus | nil, onEquip: function | nil): Source.Windows.WindowEquip.Select.Controller
---@field _player            Source.Player.Player
---@field _windowEquipSlot   Source.Windows.WindowEquipSlot | nil
---@field _windowEquipStatus Source.Windows.WindowEquipStatus | nil
---@field _onEquipCallback   function | nil
---@field _slotKey           string
---@field _equipList         (string | table)[]
---@field _equipCounts       table<string, integer>
---@field _lastStatusIndex   integer | nil
local WindowEquipSelectController = {}

---@param model             Source.Windows.WindowEquipSelect
---@param player            Source.Player.Player
---@param windowEquipSlot   Source.Windows.WindowEquipSlot | nil
---@param windowEquipStatus Source.Windows.WindowEquipStatus | nil
---@param onEquip           function | nil
function WindowEquipSelectController:init(model, player, windowEquipSlot, windowEquipStatus, onEquip) end

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

---@param player Source.Player.Player
function WindowEquipSelectController:setPlayer(player) end

---@param windowEquipStatus Source.Windows.WindowEquipStatus | nil
function WindowEquipSelectController:setEquipStatusWindow(windowEquipStatus) end

---@param windowEquipSlot Source.Windows.WindowEquipSlot | nil
function WindowEquipSelectController:setEquipSlotWindow(windowEquipSlot) end

return WindowEquipSelectController
