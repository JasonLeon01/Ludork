---@meta

---@brief Available-equip window with grid display filtered by slot.
---
--- Shows owned equips matching the selected slot with icons and counts.

---@class Source.Windows.WindowEquipSlot.Controller: Source.UIBase.UiController
---@field host               Source.Windows.WindowEquipSlot
---@field ui                 Source.UI.Parts.WindowEquip.WindowEquipSlot
---@field _player            Source.Player.Player
---@field _windowEquipSelect Source.Windows.WindowEquipSelect | nil
---@field _windowEquipStatus Source.Windows.WindowEquipStatus | nil
---@field _onCloseCallback   function | nil
---@field _slotKeys          string[]
---@field _lastSlotIndex     integer | nil
---@field _rows              Source.UIBase.UiCollection<Source.Windows.WindowEquip.EquipSlotRow.Controller>
local Controller = {}

---@brief Construct the equipped-slot window.
---
--- - @param player The player instance.
--- - @param windowEquipSelect The available-equip window to refresh on slot change.
--- - @param windowEquipStatus The detail window to refresh on slot change.
--- - @param onClose Optional callback invoked when the window is closed.
---@param player            Source.Player.Player
---@param windowEquipSelect Source.Windows.WindowEquipSelect | nil
---@param windowEquipStatus Source.Windows.WindowEquipStatus | nil
---@param onClose           function | nil
function Controller:init(player, windowEquipSelect, windowEquipStatus, onClose) end

function Controller:ready() end

---@brief Rebind the player whose equipment slots are displayed.
---@param player Source.Player.Player
function Controller:setPlayer(player) end

---@brief Set the available-equip window reference.
---
--- - @param windowEquipSelect The available-equip window.
---@param windowEquipSelect Source.Windows.WindowEquipSelect
function Controller:setEquipSelectWindow(windowEquipSelect) end

---@brief Set the equipment detail window reference.
---
--- - @param windowEquipStatus The equipment detail window.
---@param windowEquipStatus Source.Windows.WindowEquipStatus
function Controller:setEquipStatusWindow(windowEquipStatus) end

---@brief Rebuild the slot list from the player's class slot order.
function Controller:refreshSlots() end

---@brief Refresh localised slot and status text without changing the selected slot or equipment candidate.
function Controller:refreshLocale() end

---@brief Update slot window and notify slot change on index change.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function Controller:onTick(deltaTime) end

function Controller:onReturn() end

---@brief Open the slot window with the first slot and its first equipment candidate selected.
function Controller:open() end

---@brief Close the slot window.
function Controller:close() end

---@param callback function | nil
function Controller:setOnCloseCallback(callback) end

function Controller:redrawIfVisible() end

---@param slotKey string
---@return sf.Texture | nil, string
function Controller:getSlotCellData(slotKey) end

---@return string | nil
function Controller:getCurrentSlotKey() end

function Controller:notifySlotChanged() end

function Controller:focusSelectWindow() end

function Controller:closeChildWindows() end
