---@meta

---@class Source.Windows.WindowEquip.Controller: Source.UIBase.UiController
---@field host          Source.Windows.WindowEquip
---@field _slotWindow   Source.Windows.WindowEquipSlot
---@field _selectWindow Source.Windows.WindowEquipSelect
---@field _statusWindow Source.Windows.WindowEquipStatus
---@field ui            Source.UI.WindowEquip
---@field _transitionProfile string
local Controller = {}

---@param player Source.Player.Player
function Controller:init(player) end

---@param player Source.Player.Player
function Controller:setPlayer(player) end

---@param callback function | nil
function Controller:setOnCloseCallback(callback) end

---@param transitionProfile string | nil
function Controller:open(transitionProfile) end

---@param onHidden function | nil
function Controller:close(onHidden) end

function Controller:hideImmediate() end

function Controller:refreshLocale() end

---@return boolean
function Controller:getVisible() end

function Controller:requestSlotFocus() end

---@return Source.Windows.WindowEquipSlot
function Controller:getSlotFocusTarget() end

---@return Source.Windows.WindowEquipSlot, Source.Windows.WindowEquipSelect
function Controller:getFocusControls() end

---@return boolean
function Controller:returnSelectToSlot() end

function Controller:dispose() end
