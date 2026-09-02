---@meta Source.Windows.WindowEquip.Status

---@class Source.Windows.WindowEquipStatus: Source.Windows.Base.WindowBase
---@field new           fun(rect: sf.IntRect, player: Source.Player.Player, instance?: Engine.AssetInstance): Source.Windows.WindowEquipStatus
---@field _player       Source.Player.Player
---@field _slotKey      string
---@field _changeTexts  Engine.PlainText[]
---@field _descNameText Engine.PlainText
---@field _descText     Engine.PlainText
---@field _statusUI     Source.UI.Parts.WindowEquip.WindowEquipStatus
local WindowEquipStatus = {}

---@brief Construct the equipment status window.
---
--- - @param rect The window rectangle.
--- - @param player The player instance.
---@param rect   sf.IntRect
---@param player Source.Player.Player
function WindowEquipStatus:init(rect, player, instance) end

---@brief Rebind the player instance used for equipment comparisons.
---
--- - @param player The player instance.
---@param player Source.Player.Player
function WindowEquipStatus:setPlayer(player) end

---@brief Open the detail window for the current equipment slot.
---
--- - @param slotKey Equipment slot identifier.
---@param slotKey string
function WindowEquipStatus:openForSlot(slotKey) end

---@brief Close the detail window.
function WindowEquipStatus:close() end

---@brief Refresh stat changes and description for a selected equipment candidate.
---
--- - @param slotKey Equipment slot identifier.
--- - @param candidateEquipID Candidate equipment ID, or nil for no candidate.
--- - @param showUnequip Whether the candidate is the unequip command.
---@param slotKey          string
---@param candidateEquipID string | nil
---@param showUnequip      boolean | nil
function WindowEquipStatus:refreshForEquip(slotKey, candidateEquipID, showUnequip) end

---@brief Refresh description for the current equipped item in a slot.
---
--- - @param slotKey Equipment slot identifier.
---@param slotKey string
function WindowEquipStatus:refreshForSlot(slotKey) end

---@brief Equipped-slot list window ordered by class slot keys.
---
--- Shows currently equipped item names per slot, or unequipped placeholder text.

return WindowEquipStatus
