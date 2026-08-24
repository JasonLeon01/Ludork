---@meta Source.Windows.WindowEquip.Slot

---@class Source.Windows.WindowEquipSlot: Source.Windows.Base.WindowSelectable
---@field _onCloseCallback function | nil
---@field new              fun(rect: sf.IntRect, player: Source.Player.Player, windowEquipSelect?: Source.Windows.WindowEquipSelect, windowEquipStatus?: Source.Windows.WindowEquipStatus, onClose?: function): Source.Windows.WindowEquipSlot
---@field _player          Source.Player.Player
local WindowEquipSlot = {}

---@brief Construct the equipped-slot window.
---
--- - @param rect The window rectangle.
--- - @param player The player instance.
--- - @param windowEquipSelect The available-equip window to refresh on slot change.
--- - @param windowEquipStatus The detail window to refresh on slot change.
--- - @param onClose Optional callback invoked when the window is closed.
---@param rect              sf.IntRect
---@param player            Source.Player.Player
---@param windowEquipSelect Source.Windows.WindowEquipSelect | nil
---@param windowEquipStatus Source.Windows.WindowEquipStatus | nil
---@param onClose           function | nil
function WindowEquipSlot:init(rect, player, windowEquipSelect, windowEquipStatus, onClose) end

---@brief Rebind the player whose equipment slots are displayed.
---@param player Source.Player.Player
function WindowEquipSlot:setPlayer(player) end

---@brief Set the available-equip window reference.
---
--- - @param windowEquipSelect The available-equip window.
---@param windowEquipSelect Source.Windows.WindowEquipSelect
function WindowEquipSlot:setEquipSelectWindow(windowEquipSelect) end

---@brief Set the equipment detail window reference.
---
--- - @param windowEquipStatus The equipment detail window.
---@param windowEquipStatus Source.Windows.WindowEquipStatus
function WindowEquipSlot:setEquipStatusWindow(windowEquipStatus) end

---@brief Rebuild the slot list from the player's class slot order.
function WindowEquipSlot:refreshSlots() end

---@brief Refresh localised slot and status text without changing the selected slot or equipment candidate.
function WindowEquipSlot:refreshLocale() end

---@brief Update slot window and notify slot change on index change.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function WindowEquipSlot:onTick(deltaTime) end

function WindowEquipSlot:onReturn() end

---@brief Open the slot window, refreshing slot list first.
function WindowEquipSlot:open() end

---@brief Close the slot window.
function WindowEquipSlot:close() end

---@brief Available-equip window with grid display filtered by slot.
---
--- Shows owned equips matching the selected slot with icons and counts.

return WindowEquipSlot
