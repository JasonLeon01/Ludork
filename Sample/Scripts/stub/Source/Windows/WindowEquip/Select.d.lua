---@meta Source.Windows.WindowEquip.Select

---@class Source.Windows.WindowEquipSelect: Source.Windows.Base.WindowSelectable
---@field new     fun(rect: sf.IntRect, player: Source.Player.Player, windowEquipSlot?: Source.Windows.WindowEquipSlot, windowEquipStatus?: Source.Windows.WindowEquipStatus, onEquip?: function): Source.Windows.WindowEquipSelect
---@field _player Source.Player.Player
local WindowEquipSelect = {}

---@brief Construct the available-equip window.
---
--- - @param rect The window rectangle.
--- - @param player The player instance.
--- - @param windowEquipSlot The equipped-slot window for focus switching and refresh.
--- - @param windowEquipStatus The detail window for stat changes and description.
--- - @param onEquip Optional callback invoked after equipping an item.
---@param rect              sf.IntRect
---@param player            Source.Player.Player
---@param windowEquipSlot   Source.Windows.WindowEquipSlot | nil
---@param windowEquipStatus Source.Windows.WindowEquipStatus | nil
---@param onEquip           function | nil
function WindowEquipSelect:init(rect, player, windowEquipSlot, windowEquipStatus, onEquip) end

---@brief Rebind the player whose available equipment is displayed.
---@param player Source.Player.Player
function WindowEquipSelect:setPlayer(player) end

---@brief Set the equipped-slot window reference.
---
--- - @param windowEquipSlot The equipped-slot window.
---@param windowEquipSlot Source.Windows.WindowEquipSlot
function WindowEquipSelect:setEquipSlotWindow(windowEquipSlot) end

---@brief Set the equipment detail window reference.
---
--- - @param windowEquipStatus The equipment detail window.
---@param windowEquipStatus Source.Windows.WindowEquipStatus
function WindowEquipSelect:setEquipStatusWindow(windowEquipStatus) end

---@brief Rebuild the equip list for the given slot and select its first entry.
---
--- - @param slotKey The equipment slot identifier to filter by.
---@param slotKey string
function WindowEquipSelect:refreshForSlot(slotKey) end

---@brief Update equip window and refresh description on index change.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function WindowEquipSelect:onTick(deltaTime) end

---@brief Refresh the detail window from the current selected equipment.
function WindowEquipSelect:updateStatus() end

---@brief Return focus to the slot list while keeping this window visible.
---
--- - @param playSE Whether to play the cancel sound effect.
---@param playSE boolean | nil
function WindowEquipSelect:returnToSlotWindow(playSE) end

function WindowEquipSelect:onReturn() end

---@brief Open the available-equip window without taking focus.
function WindowEquipSelect:open() end

---@brief Close the available-equip window.
function WindowEquipSelect:close() end

return WindowEquipSelect
