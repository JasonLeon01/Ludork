---@meta

---@class Source.Windows.WindowEquipSelect.Controller: Source.UIBase.UiController
---@field host               Source.Windows.WindowEquipSelect
---@field ui                 Source.UI.Parts.WindowEquip.WindowEquipSelect
---@field _logicalSize       sf.Vector2u
---@field CELL_SIZE          integer
---@field UNEQUIP            table
---@field _player            Source.Player.Player
---@field _windowEquipSlot   Source.Windows.WindowEquipSlot | nil
---@field _windowEquipStatus Source.Windows.WindowEquipStatus | nil
---@field _onEquipCallback   function | nil
---@field _slotKey           string
---@field _equipList         (string | table)[]
---@field _equipCounts       table<string, integer>
---@field _lastStatusIndex   integer | nil
---@field _columns           integer
---@field _rows              Source.UIBase.UiCollection<Source.Windows.WindowEquip.EquipItemRow.Controller>
local Controller = {}

---@brief Construct the available-equip window.
---
--- - @param player The player instance.
--- - @param windowEquipSlot The equipped-slot window for focus switching and refresh.
--- - @param windowEquipStatus The detail window for stat changes and description.
--- - @param onEquip Optional callback invoked after equipping an item.
---@param player            Source.Player.Player
---@param windowEquipSlot   Source.Windows.WindowEquipSlot | nil
---@param windowEquipStatus Source.Windows.WindowEquipStatus | nil
---@param onEquip           function | nil
function Controller:init(player, windowEquipSlot, windowEquipStatus, onEquip) end

function Controller:ready() end

---@brief Rebind the player whose available equipment is displayed.
---@param player Source.Player.Player
function Controller:setPlayer(player) end

---@brief Set the equipped-slot window reference.
---
--- - @param windowEquipSlot The equipped-slot window.
---@param windowEquipSlot Source.Windows.WindowEquipSlot
function Controller:setEquipSlotWindow(windowEquipSlot) end

---@brief Set the equipment detail window reference.
---
--- - @param windowEquipStatus The equipment detail window.
---@param windowEquipStatus Source.Windows.WindowEquipStatus
function Controller:setEquipStatusWindow(windowEquipStatus) end

---@brief Rebuild the equip list for the given slot and select its first entry.
---
--- - @param slotKey The equipment slot identifier to filter by.
---@param slotKey string
function Controller:refreshForSlot(slotKey) end

---@brief Update equip window and refresh description on index change.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function Controller:onTick(deltaTime) end

---@brief Refresh the detail window from the current selected equipment.
function Controller:updateStatus() end

---@brief Return focus to the slot list while keeping this window visible.
---
--- - @param playSE Whether to play the cancel sound effect.
---@param playSE boolean | nil
function Controller:returnToSlotWindow(playSE) end

function Controller:onReturn() end

---@brief Open the available-equip window without taking focus.
function Controller:open() end

---@brief Close the available-equip window.
function Controller:close() end

---@param contentWidth integer
---@return integer
function Controller:getGridColumns(contentWidth) end

function Controller:onConfirmAction() end

function Controller:_updateLayout() end

function Controller:refreshListLayout() end
