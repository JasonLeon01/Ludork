---@meta

---@brief Equipped-slot list window ordered by class slot keys.
---
--- Shows currently equipped item names per slot, or unequipped placeholder text.

---@class Source.Windows.WindowEquipStatus.Controller: Source.UIBase.UiController
---@field host             Source.Windows.WindowEquipStatus
---@field ui               Source.UI.Parts.WindowEquip.WindowEquipStatusPane
---@field _player          Source.Player.Player
---@field _changeRows      Source.UIBase.UiCollection<Source.Windows.WindowEquip.EquipStatusRow.Controller>
---@field _showComparison  boolean
---@field _descriptionName string
---@field _descriptionText string
local Controller = {}

---@brief Construct the equipment status window.
---
--- - @param player The player instance.
---@param player Source.Player.Player
function Controller:init(player) end

---@brief Rebind the player instance used for equipment comparisons.
---
--- - @param player The player instance.
---@param player Source.Player.Player
function Controller:setPlayer(player) end

---@brief Open the detail window for the current equipment slot.
---
--- - @param slotKey Equipment slot identifier.
---@param slotKey string
function Controller:openForSlot(slotKey) end

---@brief Close the detail window.
function Controller:close() end

---@brief Refresh stat changes and description for a selected equipment candidate.
---
--- - @param slotKey Equipment slot identifier.
--- - @param candidateEquipID Candidate equipment ID, or nil for no candidate.
--- - @param showUnequip Whether the candidate is the unequip command.
---@param slotKey          string
---@param candidateEquipID string | nil
---@param showUnequip      boolean | nil
function Controller:refreshForEquip(slotKey, candidateEquipID, showUnequip) end

---@brief Refresh description for the current equipped item in a slot.
---
--- - @param slotKey Equipment slot identifier.
---@param slotKey string
function Controller:refreshForSlot(slotKey) end

function Controller:refresh() end

---@param currentAttrs   table<string, integer>
---@param candidateAttrs table<string, integer>
function Controller:refreshChangeRows(currentAttrs, candidateAttrs) end

---@param candidateEquipID string | nil
---@param showUnequip      boolean
function Controller:refreshDescription(candidateEquipID, showUnequip) end

function Controller:clearChangeTexts() end

---@param equipID string | nil
---@return table<string, integer>
function Controller:getAttrPlus(equipID) end

---@param firstAttrs  table
---@param secondAttrs table
---@return table
function Controller:getAttrKeys(firstAttrs, secondAttrs) end
