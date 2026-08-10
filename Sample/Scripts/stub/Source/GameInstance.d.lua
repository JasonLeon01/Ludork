---@meta Source.GameInstance

---@alias Source.GameInstance.TerrainTileID integer|string|nil
---@alias Source.GameInstance.RecordValue nil|boolean|number|string|table

---@class Source.GameInstance.AddedActorRecord
---@field bp              string
---@field layer           string
---@field position        sf.Vector2i
---@field tag             string
---@field classVarChanges table<string, Source.GameInstance.RecordValue> | nil

---@class Source.GameInstance.TerrainChangeRecord
---@field position sf.Vector2i
---@field tileID   Source.GameInstance.TerrainTileID

---@class Source.GameInstance.SavedAddedActorRecord
---@field bp              string
---@field layer           string
---@field position        integer[]
---@field tag             string
---@field classVarChanges table<string, Source.GameInstance.RecordValue> | nil

---@class Source.GameInstance.SavedTerrainChangeRecord
---@field position integer[]
---@field tileID   integer | string | lightuserdata

---@class Source.GameInstance.SaveData
---@field region           string
---@field players          Source.Player.SaveData[]
---@field variables        table<string, Source.GameInstance.RecordValue>
---@field map              string
---@field obtainedItems    table<string, boolean>
---@field addedActors      table<string, Source.GameInstance.SavedAddedActorRecord[]>
---@field actorPositions   table<string, table<string, integer[]>>
---@field destroyedActors  table<string, string[]>
---@field destroyedTerrain table<string, table<string, Source.GameInstance.SavedTerrainChangeRecord[]>>
---@field telepoints       table<string, integer[][]>
---@field screenshot       integer[] | nil

--- @brief Persistent game state container that survives across scene transitions.
---
--- Holds player data, game variables, cached map information,
--- and destroyed actor tracking.
---@class Source.GameInstance.GameInstance
---@field _cachedMap string | nil
local GameInstance = {}

---@return Source.GameInstance.GameInstance
function GameInstance.new(...) end

--- @brief Construct a new game instance with a default player.
---@param skipDefaultPlayer boolean | nil
function GameInstance:init(skipDefaultPlayer) end

--- @brief Serialize the game instance to a dictionary.
---
--- - @return A dictionary containing players, variables, map, destroyed actors, and screenshot.
---@return Source.GameInstance.SaveData
function GameInstance:asDict() end

--- @brief Deserialize a game instance from a dictionary.
---
--- - @param data The serialised game instance data.
--- - @return A restored GameInstance.
---@param data Source.GameInstance.SaveData
---@return Source.GameInstance.GameInstance
function GameInstance.FromDict(data) end

--- @brief Get the current region.
---
--- - @return The current region.
---@return string
function GameInstance:getCurrentRegion() end

--- @brief Set the current region.
---
--- - @param region The region to set.
---@param region string
function GameInstance:setCurrentRegion(region) end

--- @brief Set the captured screenshot bytes used for save thumbnails.
---
--- - @param screenshot Encoded image bytes (PNG) or nil to clear.
---@param screenshot integer[] | nil
function GameInstance:setScreenshot(screenshot) end

--- @brief Get the captured screenshot bytes.
---
--- - @return Encoded image bytes (PNG) or nil.
---@return integer[] | nil
function GameInstance:getScreenshot() end

--- @brief Get all game variables.
---
--- - @return A dictionary of all game variables.
---@return table<string, Source.GameInstance.RecordValue>
function GameInstance:getVariables() end

--- @brief Get a game variable by name.
---
--- - @param name The variable name.
--- - @return The variable value, or nil if not found.
---@param name string
---@return Source.GameInstance.RecordValue
function GameInstance:getVariable(name) end

--- @brief Set a game variable.
---
--- - @param name The variable name.
--- - @param value The value to set.
---@param name  string
---@param value Source.GameInstance.RecordValue
function GameInstance:setVariable(name, value) end

--- @brief Get the first (primary) player.
---
--- - @return The primary player.
---@return Source.Player.Player
function GameInstance:getPlayer() end

--- @brief Set the primary player.
---
--- - @param player The player to set as primary.
---@param player Source.Player.Player
function GameInstance:setPlayer(player) end

--- @brief Get all players.
---
--- - @return A list of all players.
---@return Source.Player.Player[]
function GameInstance:getPlayers() end

--- @brief Get a player by index.
---
--- - @param index The player index.
--- - @return The player at the given index.
---@param index integer
---@return Source.Player.Player
function GameInstance:getPlayerByIndex(index) end

--- @brief Find a player by tag.
---
--- - @param tag The player tag to search for.
--- - @return The matching player, or nil.
---@param tag string
---@return Source.Player.Player | nil
function GameInstance:getPlayerByTag(tag) end

--- @brief Add a new player by class path.
---
--- - @param playerClass The class path for the player blueprint.
---@param playerClass string
function GameInstance:addPlayerByClass(playerClass) end

--- @brief Remove a player by class path.
---
--- - @param playerClass The class path to remove.
---@param playerClass string
function GameInstance:removePlayerByClass(playerClass) end

--- @brief Apply map information for scene transitions.
---
--- - @param mapPath The new map path to cache.
--- - @param pos The position to set the primary player to.
---@param mapPath  string
---@param position sf.Vector2i | sf.Vector2u | nil
function GameInstance:applyMapInfo(mapPath, position) end

--- @brief Record an added actor for persistence.
---
--- - @param mapPath The map path where the actor was added.
--- - @param actor The added actor.
--- - @param layerName The actor layer name.
---@param mapPath   string
---@param actor     Engine.Actor
---@param layerName string
function GameInstance:recordAddedActor(mapPath, actor, layerName) end

--- @brief Get added actor records for a map.
---
--- - @param mapPath The map path.
--- - @return A list of added actor records.
---@param mapPath string
---@return Source.GameInstance.AddedActorRecord[]
function GameInstance:getAddedActors(mapPath) end

--- @brief Record an actor position change for persistence.
---
--- - @param mapPath The map path where the actor moved.
--- - @param actor The moved actor.
---@param mapPath       string
---@param actor         Engine.Actor
---@param actorPosition sf.Vector2i | nil
function GameInstance:recordActorPosition(mapPath, actor, actorPosition) end

--- @brief Get actor position records for a map.
---
--- - @param mapPath The map path.
--- - @return Actor-tag-indexed position records.
---@param mapPath string
---@return table<string, sf.Vector2i>
function GameInstance:getActorPositions(mapPath) end

--- @brief Record a destroyed actor for persistence.
---
--- - @param mapPath The map path where the actor was destroyed.
--- - @param actor The destroyed actor.
---@param mapPath string
---@param actor   Engine.Actor
function GameInstance:recordDestroyedActor(mapPath, actor) end

--- @brief Get destroyed actor tags for a map.
---
--- - @param mapPath The map path.
--- - @return A list of destroyed actor tags.
---@param mapPath string
---@return string[]
function GameInstance:getDestroyedActors(mapPath) end

--- @brief Record a terrain tile replacement for persistence.
---
--- - @param mapPath The map path where the terrain was changed.
--- - @param layerName The tile layer name.
--- - @param position The tile coordinate.
--- - @param tileID The replacement tile ID, autotile key, or nil to clear the tile.
---@param mapPath   string
---@param layerName string
---@param position  sf.Vector2i
---@param tileID    integer | string | nil
function GameInstance:recordTerrainDestruction(mapPath, layerName, position, tileID) end

--- @brief Get recorded terrain tile replacements for a map.
---
--- - @param mapPath The map path.
--- - @return Layer-indexed terrain replacement records.
---@param mapPath string
---@return table<string, table<string, Source.GameInstance.TerrainChangeRecord>>
function GameInstance:getTerrainDestructions(mapPath) end

--- @brief Record a telepoint for persistence.
---
--- - @param mapPath The map path where the telepoint is located.
--- - @param telepoint The telepoint position.
---@param mapPath   string
---@param telepoint sf.Vector2u
function GameInstance:recordTelepoint(mapPath, telepoint) end

--- @brief Get telepoint positions for a map.
---
--- - @param mapPath The map path.
--- - @return A list of telepoint positions.
---@param mapPath string
---@return sf.Vector2u[]
function GameInstance:getTelepoints(mapPath) end

--- @brief Check whether an item or equip has been obtained before.
---
--- - @param itemID The item ID.
--- - @return True if the item or equip has already been obtained.
---@param itemID string
---@return boolean
function GameInstance:getCachedNewItem(itemID) end

--- @brief Mark an item or equip as obtained before.
---
--- - @param itemID The item ID.
---@param itemID string
function GameInstance:setCachedNewItem(itemID) end

return GameInstance
