---@meta Source.NodeFunctions.Player

---@brief Get the primary player from the current scene.
---
--- - @return The primary Player instance, or nil if no active game scene.
---@return Source.Player.Player | nil
function Player.GetPlayer() end

---@brief Get the tile position in front of the player.
---
--- - @return The tile position in front of the player, or nil if no active player exists.
---@return sf.Vector2i | nil
function Player.GetPlayerFrontPosition() end

---@brief Add item(s) to the player's inventory.
---
--- - @param itemID Item identifier.
--- - @param count Number of items to add.
---@param itemID string
---@param count  integer
function Player.AddItem(itemID, count) end

---@brief Remove item(s) from the player's inventory.
---
--- - @param itemID Item identifier.
--- - @param count Number of items to remove.
--- - @return 0 on success, 1 if item count is insufficient.
---@param itemID string
---@param count  integer
---@return integer
function Player.RemoveItem(itemID, count) end

---@brief Check whether the player owns at least one of the specified item.
---
--- - @param itemID Item identifier.
--- - @return True if the player owns the item.
---@param itemID string
---@return boolean
function Player.HasItem(itemID) end

---@brief Get the count of a specific item in the player's inventory.
---
--- - @param itemID Item identifier.
--- - @return Number of items owned, or 0 if not found.
---@param itemID string
---@return integer
function Player.GetItemCount(itemID) end

---@brief Add equip(s) to the player's equipment bag.
---
--- - @param equipID Equip identifier.
--- - @param count Number of equips to add.
---@param equipID string
---@param count   integer
function Player.AddEquip(equipID, count) end

---@brief Remove equip(s) from the player's equipment bag.
---
--- - @param equipID Equip identifier.
--- - @param count Number of equips to remove.
--- - @return 0 on success, 1 if equip count is insufficient.
---@param equipID string
---@param count   integer
---@return integer
function Player.RemoveEquip(equipID, count) end

---@brief Check whether the player owns at least one of the specified equip.
---
--- - @param equipID Equip identifier.
--- - @return True if the player owns the equip.
---@param equipID string
---@return boolean
function Player.HasEquip(equipID) end

---@brief Equip a piece of equipment onto the player.
---
--- - @param equipID Equip identifier.
---@param equipID string
function Player.EquipItem(equipID) end

---@brief Unequip the item occupying the given equipment slot.
---
--- - @param slotID Equipment slot identifier.
---@param slotID string
function Player.UnequipSlot(slotID) end

---@brief Get the equip ID currently occupying the given slot.
---
--- - @param slotID Equipment slot identifier.
--- - @return The equip ID, or an empty string if the slot is empty.
---@param slotID string
---@return string
function Player.GetEquipInSlot(slotID) end

---@brief Get a named attribute from the player (e.g. HP, MAXHP, ATK, DEF, GOLD, EXP, LEVEL).
---
--- - @param attrName The attribute name.
--- - @return The attribute value, or nil if not found.
---@generic T
---@param attrName string
---@return T | nil
function Player.GetPlayerAttr(attrName) end

---@brief Set a named attribute on the player (e.g. HP, MAXHP, ATK, DEF, GOLD, EXP, LEVEL).
---
--- - @param attrName The attribute name.
--- - @param value The new value.
---@generic T
---@param attrName string
---@param value    T
function Player.SetPlayerAttr(attrName, value) end

---@brief Get a readable/writable reference to a player attribute.
---
--- - @param attrName The attribute name (e.g. HP, ATK, DEF, GOLD).
--- - @return An attribute reference wrapper, or nil if no active player.
---@generic T
---@param attrName string
---@return Source.NodeFunctions.Utils.NodeReference<T> | nil
function Player.GetPlayerAttrRef(attrName) end

---@brief Restore HP to the player, capped at MAXHP.
---
--- - @param amount Amount of HP to restore.
---@param amount integer
function Player.HealPlayer(amount) end

---@brief Deal damage to the player, floored at 0.
---
--- - @param amount Amount of HP to subtract.
---@param amount integer
function Player.DamagePlayer(amount) end

---@brief Add HP to the player.
---
--- - @param amount Amount of HP to add (can be negative to subtract).
---@param amount integer
function Player.AddHP(amount) end

---@brief Add gold to the player.
---
--- - @param amount Amount of gold to add (can be negative to subtract).
---@param amount integer
function Player.AddGold(amount) end

---@brief Add ATK to the player.
---
--- - @param amount Amount of ATK to add (can be negative to subtract).
---@param amount integer
function Player.AddATK(amount) end

---@brief Add DEF to the player.
---
--- - @param amount Amount of DEF to add (can be negative to subtract).
---@param amount integer
function Player.AddDEF(amount) end

---@brief Add experience points to the player.
---
--- - @param amount Amount of EXP to add.
---@param amount integer
function Player.AddEXP(amount) end

---@param actors Engine.Actor[]
---@return Source.Player.Player | nil
function Player.MeetPlayer(actors) end

return Player
