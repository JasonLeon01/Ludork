---@meta Source.Windows.WindowEquip

---@param model Source.Windows.WindowEquipSlot
function WindowEquipSlotController:init(model) end

function WindowEquipSlotController:attach() end

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

---@return boolean
function WindowEquipSlotController:handleKeyDown() end

---@param kwargs table
---@return boolean
function WindowEquipSlotController:handleMouseButtonDown(kwargs) end

function WindowEquipSlotController:open() end

function WindowEquipSlotController:closeChildWindows() end

function WindowEquipSlotController:close() end

function WindowEquipSlotController:closeByCancel() end

local WindowEquipSelectController = {}

---@param model Source.Windows.WindowEquipSelect
function WindowEquipSelectController:init(model) end

function WindowEquipSelectController:attach() end

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

---@return boolean
function WindowEquipSelectController:handleKeyDown() end

---@param kwargs table
---@return boolean
function WindowEquipSelectController:handleMouseButtonDown(kwargs) end

function WindowEquipSelectController:open() end

function WindowEquipSelectController:close() end

function WindowEquipSelectController:onConfirmAction() end

--- @brief Equipment detail window with stat delta preview and description.
---@class Source.Windows.WindowEquipStatus: Source.Windows.Base.WindowBase
---@field new fun(rect: sf.IntRect, player: Source.Player.Player): Source.Windows.WindowEquipStatus
---@field _player Source.Player.Player
---@field _slotKey string
---@field _changeTexts Engine.PlainText[]
---@field _descNameText Engine.PlainText
---@field _descText Engine.PlainText
---@field _statusUI Source.UI.Parts.WindowEquip.WindowEquipStatus
local WindowEquipStatus = {}

--- @brief Construct the equipment status window.
---
--- - @param rect The window rectangle.
--- - @param player The player instance.
---@param rect   sf.IntRect
---@param player Source.Player.Player
function WindowEquipStatus:init(rect, player) end

--- @brief Rebind the player instance used for equipment comparisons.
---
--- - @param player The player instance.
---@param player Source.Player.Player
function WindowEquipStatus:setPlayer(player) end

--- @brief Open the detail window for the current equipment slot.
---
--- - @param slotKey Equipment slot identifier.
---@param slotKey string
function WindowEquipStatus:openForSlot(slotKey) end

--- @brief Close the detail window.
function WindowEquipStatus:close() end

--- @brief Refresh stat changes and description for a selected equipment candidate.
---
--- - @param slotKey Equipment slot identifier.
--- - @param candidateEquipID Candidate equipment ID, or nil for no candidate.
--- - @param showUnequip Whether the candidate is the unequip command.
---@param slotKey          string
---@param candidateEquipID string | nil
---@param showUnequip      boolean | nil
function WindowEquipStatus:refreshForEquip(slotKey, candidateEquipID, showUnequip) end

--- @brief Refresh description for the current equipped item in a slot.
---
--- - @param slotKey Equipment slot identifier.
---@param slotKey string
function WindowEquipStatus:refreshForSlot(slotKey) end

--- @brief Equipped-slot list window ordered by class slot keys.
---
--- Shows currently equipped item names per slot, or unequipped placeholder text.
---@class Source.Windows.WindowEquipSlot: Source.Windows.Base.WindowSelectable
---@field _onCloseCallback function | nil
---@field new fun(rect: sf.IntRect, player: Source.Player.Player, windowEquipSelect?: Source.Windows.WindowEquipSelect, windowEquipStatus?: Source.Windows.WindowEquipStatus, onClose?: function): Source.Windows.WindowEquipSlot
---@field _player Source.Player.Player
local WindowEquipSlot = {}

--- @brief Construct the equipped-slot window.
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

--- @brief Set the available-equip window reference.
---
--- - @param windowEquipSelect The available-equip window.
---@param windowEquipSelect Source.Windows.WindowEquipSelect
function WindowEquipSlot:setEquipSelectWindow(windowEquipSelect) end

--- @brief Set the equipment detail window reference.
---
--- - @param windowEquipStatus The equipment detail window.
---@param windowEquipStatus Source.Windows.WindowEquipStatus
function WindowEquipSlot:setEquipStatusWindow(windowEquipStatus) end

--- @brief Rebuild the slot list from the player's class slot order.
function WindowEquipSlot:refreshSlots() end

--- @brief Refresh localised slot and status text without changing the selected slot or equipment candidate.
function WindowEquipSlot:refreshLocale() end

--- @brief Update slot window and notify slot change on index change.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function WindowEquipSlot:onTick(deltaTime) end

--- @brief Handle cancel, confirm, and focus-switch keys.
---
--- - @param kwargs Event data.
---@param kwargs table
function WindowEquipSlot:onKeyDown(kwargs) end

--- @brief Handle mouse cancel to close the slot window.
---@param kwargs table
---@return boolean
function WindowEquipSlot:onMouseButtonDown(kwargs) end

--- @brief Open the slot window, refreshing slot list first.
function WindowEquipSlot:open() end

--- @brief Close the slot window.
function WindowEquipSlot:close() end

--- @brief Available-equip window with grid display filtered by slot.
---
--- Shows owned equips matching the selected slot with icons and counts.
---@class Source.Windows.WindowEquipSelect: Source.Windows.Base.WindowSelectable
---@field new fun(rect: sf.IntRect, player: Source.Player.Player, windowEquipSlot?: Source.Windows.WindowEquipSlot, windowEquipStatus?: Source.Windows.WindowEquipStatus, onEquip?: function): Source.Windows.WindowEquipSelect
---@field _player Source.Player.Player
local WindowEquipSelect = {}

--- @brief Construct the available-equip window.
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

--- @brief Set the equipped-slot window reference.
---
--- - @param windowEquipSlot The equipped-slot window.
---@param windowEquipSlot Source.Windows.WindowEquipSlot
function WindowEquipSelect:setEquipSlotWindow(windowEquipSlot) end

--- @brief Set the equipment detail window reference.
---
--- - @param windowEquipStatus The equipment detail window.
---@param windowEquipStatus Source.Windows.WindowEquipStatus
function WindowEquipSelect:setEquipStatusWindow(windowEquipStatus) end

--- @brief Rebuild the equip list for the given slot.
---
--- - @param slotKey The equipment slot identifier to filter by.
---@param slotKey string
function WindowEquipSelect:refreshForSlot(slotKey) end

--- @brief Update equip window and refresh description on index change.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function WindowEquipSelect:onTick(deltaTime) end

--- @brief Refresh the detail window from the current selected equipment.
function WindowEquipSelect:updateStatus() end

--- @brief Return focus to the slot list while keeping this window visible.
---
--- - @param playSE Whether to play the cancel sound effect.
---@param playSE boolean | nil
function WindowEquipSelect:returnToSlotWindow(playSE) end

--- @brief Handle cancel, confirm, and focus-switch keys.
---
--- - @param kwargs Event data.
---@param kwargs table
function WindowEquipSelect:onKeyDown(kwargs) end

--- @brief Handle mouse cancel to close this window.
---@param kwargs table
---@return boolean
function WindowEquipSelect:onMouseButtonDown(kwargs) end

--- @brief Open the available-equip window without taking focus.
function WindowEquipSelect:open() end

--- @brief Close the available-equip window.
function WindowEquipSelect:close() end

---@class Source.Windows.WindowEquipExports
---@field WindowEquipStatus Source.Windows.WindowEquipStatus & { new: fun(rect: sf.IntRect, player: Source.Player.Player): Source.Windows.WindowEquipStatus }
---@field WindowEquipSlot   Source.Windows.WindowEquipSlot & { new: fun(rect: sf.IntRect, player: Source.Player.Player, windowEquipSelect?: Source.Windows.WindowEquipSelect, windowEquipStatus?: Source.Windows.WindowEquipStatus, onClose?: function): Source.Windows.WindowEquipSlot }
---@field WindowEquipSelect Source.Windows.WindowEquipSelect & { new: fun(rect: sf.IntRect, player: Source.Player.Player, windowEquipSlot?: Source.Windows.WindowEquipSlot, windowEquipStatus?: Source.Windows.WindowEquipStatus, onEquip?: function): Source.Windows.WindowEquipSelect }
local WindowEquipExports = {}

return WindowEquipExports
