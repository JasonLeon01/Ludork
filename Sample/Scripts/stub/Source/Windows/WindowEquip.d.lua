---@meta Source.Windows.WindowEquip

---@class Source.Windows.WindowEquip: Engine.Canvas
---@field _slotWindow   Source.Windows.WindowEquipSlot
---@field _selectWindow Source.Windows.WindowEquipSelect
---@field _statusWindow Source.Windows.WindowEquipStatus
---@field new           fun(player: Source.Player.Player): Source.Windows.WindowEquip
local WindowEquip = {}

---@param player Source.Player.Player
function WindowEquip:init(player) end

---@param player Source.Player.Player
function WindowEquip:setPlayer(player) end

---@param callback function | nil
function WindowEquip:setOnCloseCallback(callback) end

function WindowEquip:open() end

---@param onHidden function | nil
function WindowEquip:close(onHidden) end

function WindowEquip:hideImmediate() end

function WindowEquip:refreshLocale() end

---@return boolean
function WindowEquip:getVisible() end

function WindowEquip:requestSlotFocus() end

---@return Source.Windows.WindowEquipSlot
function WindowEquip:getSlotFocusTarget() end

---@return Source.Windows.WindowEquipSlot, Source.Windows.WindowEquipSelect
function WindowEquip:getFocusControls() end

---@return boolean
function WindowEquip:returnSelectToSlot() end

return WindowEquip
