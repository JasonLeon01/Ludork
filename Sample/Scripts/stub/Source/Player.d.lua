---@meta Source.Player
---@class Source.Player.SaveData
---@field playerClass string
---@field tag         string
---@field position    integer[]
---@field attr        table<string, integer | string>
---@field items       table<string, integer>
---@field equips      table<string, integer>
---@field equipInfo   table<string, string>
---@field states      table<string, integer>

---@class Source.Player.Player: Engine.Character, Source.Battler.Battler
---@field attributes                Source.Configs.GeneralDataTypes.PlayerAttributeSet
---@field _loading                  boolean
---@field _items                    table<string, integer>
---@field _equips                   table<string, integer>
---@field _equipInfo                table<string, string>
---@field _equipEffectHandles       table<string, integer>
---@field _classPath                string
---@field _forbiddenMoving          boolean
---@field _wasMovingOnLastFixedTick boolean
---@field _movementSpecialPath      sf.Vector2i[]
---@field new                       fun(texture?: sf.Texture, tag?: string): Source.Player.Player
local Player = {}

---@param texture sf.Texture | nil
---@param tag     string
function Player:init(texture, tag) end

---@brief Take and clear the map cells arrived at during the current move.
---
--- - @return Arrived map cells in order, excluding the movement start cell.
---@return sf.Vector2i[]
function Player:consumeMovementSpecialPath() end

---@brief Get the blueprint class path used to create this player.
---
--- - @return Player class path string.
---@return string
function Player:getClassPath() end

---@brief Set the blueprint class path for this player.
---
--- - @param classPath Player class path string.
---@param classPath string
function Player:setClassPath(classPath) end

---@param fixedDelta number
function Player:onFixedTick(fixedDelta) end

---
---@brief Serialize player information for serialization.
---
--- - @return A dictionary containing player class path, tag, position, attributes, and inventory.
---
---@return Source.Player.SaveData
function Player:asDict() end

---
---@brief Initialize a player character from a class path.
---
--- - @param playerPath  Path to the player class.
--- - @param applyInitialEquipment Whether Class defaults should be equipped; defaults to true.
---
--- - @return A new `Player` instance initialized with the provided class path.
---
---@param playerPath             string
---@param applyInitialEquipment? boolean
---@return Source.Player.Player
function Player.InitPlayer(playerPath, applyInitialEquipment) end

---
---@brief Deserialize player attributes and inventory from a dictionary.
---
--- - @param data  A dictionary containing player attributes and inventory.
---
--- - @return A new `Player` instance initialized with the provided data.
---
---@param data Source.Player.SaveData
---@return Source.Player.Player
function Player.FromDict(data) end

---@brief Add item(s) to the player's inventory.
---
--- - `itemID` - Item identifier.
--- - `count` - Number of items to add, default is 1.
---@param itemID string
---@param count? integer
function Player:addItem(itemID, count) end

---@brief Remove item(s) from the player's inventory.
---
--- - `itemID` - Item identifier.
--- - `count` - Number of items to remove, default is 1.
---
--- - @return `True` if removal succeeded, `False` otherwise.
---@param itemID string
---@param count? integer
---@return boolean
function Player:removeItem(itemID, count) end

---@param itemID string
---@return Global.Gameplay.GameplayAbilityResult
function Player:activateItem(itemID) end

---@brief Get the count of a specific item in the player's inventory.
---
--- - `itemID` - Item identifier.
---
--- - @return Number of items owned, or 0 if not found.
---@param itemID string
---@return integer
function Player:getItemCount(itemID) end

---@brief Check whether the player owns at least one of the specified item.
---
--- - `itemID` - Item identifier.
---
--- - @return `True` if the item is owned and count > 0, `False` otherwise.
---@param itemID string
---@return boolean
function Player:hasItem(itemID) end

---@brief Add equip(s) to the player's equipment.
---
--- - `equipID` - Equip identifier.
--- - `count` - Number of equips to add, default is 1.
---@param equipID string
---@param count?  integer
function Player:addEquip(equipID, count) end

---@brief Remove equip(s) from the player's equipment.
---
--- - `equipID` - Equip identifier.
--- - `count` - Number of equips to remove, default is 1.
---
--- - @return `True` if removal succeeded, `False` otherwise.
---@param equipID string
---@param count?  integer
---@return boolean
function Player:removeEquip(equipID, count) end

---@brief Equip a specific equip to the player's equipment.
---
--- - `equipID` - Equip identifier.
---@param equipID string
function Player:equip(equipID) end

---@brief Unequip a specific equip from the player's equipment.
---
--- - `slotID` - Slot identifier.
---@param slotID string
function Player:unequip(slotID) end

---@brief Get the count of a specific equip in the player's equipment.
---
--- - `equipID` - Equip identifier.
---
--- - @return Number of equips owned, or 0 if not found.
---@param equipID string
---@return integer
function Player:getEquipCount(equipID) end

---@brief Check whether the player owns at least one of the specified equip.
---
--- - `equipID` - Equip identifier.
---
--- - @return `True` if the equip is owned and count > 0, `False` otherwise.
---@param equipID string
---@return boolean
function Player:hasEquip(equipID) end

---@brief Get the info of a specific equip in the player's equipment.
---
--- - `slotID` - Slot identifier.
---
--- - @return The info of the equip, or empty string if not found.
---@param slotID string
---@return string
function Player:getEquipInfo(slotID) end

---@return boolean
function Player:getForbiddenMoving() end

---@param value boolean
function Player:setForbiddenMoving(value) end

return Player
