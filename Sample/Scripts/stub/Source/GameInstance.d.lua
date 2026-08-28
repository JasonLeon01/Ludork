---@meta Source.GameInstance

---@alias Source.GameInstance.RecordValue nil | boolean | number | string | table

---@class Source.GameInstance.AddedActorRecord
---@field bp              string
---@field layer           string
---@field position        sf.Vector2i
---@field tag             string                                               Non-empty map-placement tag returned by `actor:getMapTag()`.
---@field classVarChanges table<string, Source.GameInstance.RecordValue> | nil

---@class Source.GameInstance.WorldMovedActorRecord: Source.GameInstance.AddedActorRecord
---@field definitionRegion string Canonical child-map path that authored the root Actor.
---@field currentRegion    string Canonical owning child-map path, or an empty string while the Actor is in a legal hole.

---@class Source.GameInstance.TerrainChangeRecord
---@field position sf.Vector2i
---@field tileID   Global.GameMap.TerrainTileID

---@class Source.GameInstance.SavedAddedActorRecord
---@field bp              string
---@field layer           string
---@field position        integer[]
---@field tag             string                                               Non-empty map-placement tag returned by `actor:getMapTag()`.
---@field classVarChanges table<string, Source.GameInstance.RecordValue> | nil

---@class Source.GameInstance.SavedWorldMovedActorRecord: Source.GameInstance.SavedAddedActorRecord
---@field definitionRegion string
---@field currentRegion    string

---@class Source.GameInstance.SavedTerrainChangeRecord
---@field position integer[]
---@field tileID   integer | string | lightuserdata

---@class Source.GameInstance.SaveData
---@field region           string
---@field playerKeys       string[]
---@field players          table<string, Source.Player.SaveData>
---@field variables        table<string, Source.GameInstance.RecordValue>
---@field map              string
---@field obtainedItems    table<string, boolean>
---@field addedActors      table<string, Source.GameInstance.SavedAddedActorRecord[]>
---@field actorPositions   table<string, table<string, integer[]>>
---@field worldMovedActors table<string, Source.GameInstance.SavedWorldMovedActorRecord[]> | nil        Required when `map` is a world manifest or the save retains moved-world state.
---@field destroyedActors  table<string, string[]>
---@field destroyedTerrain table<string, table<string, Source.GameInstance.SavedTerrainChangeRecord[]>>
---@field telepoints       table<string, integer[][]>
---@field screenshot       integer[] | nil

---@brief Persistent game state container that survives across scene transitions.
---
--- Holds player data, game variables, cached map information,
--- and destroyed actor tracking. Added, moved and destroyed Actor records use
--- the stable map-placement tag returned by `actor:getMapTag()`, not `actor.tag`.
---@class Source.GameInstance.GameInstance
---@field _playerKeys                 string[]
---@field _players                    table<string, Source.Player.Player>
---@field _cachedMap                  string | nil
---@field _cachedWorldMovedActors     table<string, Source.GameInstance.WorldMovedActorRecord[]>
---@field new                         fun(skipDefaultPlayer?: boolean): Source.GameInstance.GameInstance
---@field FromDict                    fun(data: Source.GameInstance.SaveData): Source.GameInstance.GameInstance
---@field getPlayer                   fun(self: Source.GameInstance.GameInstance): Source.Player.Player
---@field getVariables                fun(self: Source.GameInstance.GameInstance): table<string, Source.GameInstance.RecordValue>
---@field getVariable                 fun(self: Source.GameInstance.GameInstance, name: string): Source.GameInstance.RecordValue
---@field setVariable                 fun(self: Source.GameInstance.GameInstance, name: string, value: Source.GameInstance.RecordValue)
---@field getTerrainDestructions      fun(self: Source.GameInstance.GameInstance, mapPath: string): table<string, table<string, Source.GameInstance.TerrainChangeRecord>>
---@field getAddedActors              fun(self: Source.GameInstance.GameInstance, mapPath: string): Source.GameInstance.AddedActorRecord[]
---@field getActorPositions           fun(self: Source.GameInstance.GameInstance, mapPath: string): table<string, sf.Vector2i>
---@field getDestroyedActors          fun(self: Source.GameInstance.GameInstance, mapPath: string): string[]
---@field recordTerrainDestruction    fun(self: Source.GameInstance.GameInstance, mapPath: string, layerName: string, position: sf.Vector2i, tileID: Global.GameMap.TerrainTileID)
---@field recordTelepoint             fun(self: Source.GameInstance.GameInstance, mapPath: string, telepoint: sf.Vector2u)
---@field applyMapInfo                fun(self: Source.GameInstance.GameInstance, mapPath: string, position?: sf.Vector2i)
---@field recordAddedActor            fun(self: Source.GameInstance.GameInstance, mapPath: string, actor: Engine.Actor, layerName: string)
---@field recordAddedActorPosition    fun(self: Source.GameInstance.GameInstance, mapPath: string, actor: Engine.Actor, actorPosition?: sf.Vector2i)
---@field _buildWorldMovedActorRecord fun(self: Source.GameInstance.GameInstance, actor: Engine.Actor, definitionRegion: string, currentRegion: string, layerName: string, actorPosition: sf.Vector2i): Source.GameInstance.WorldMovedActorRecord | nil
---@field recordActorPosition         fun(self: Source.GameInstance.GameInstance, mapPath: string, actor: Engine.Actor, actorPosition?: sf.Vector2i)
---@field recordWorldMovedActor       fun(self: Source.GameInstance.GameInstance, worldPath: string, actor: Engine.Actor, definitionRegion: string, currentRegion: string, layerName: string, actorPosition?: sf.Vector2i)
---@field removeWorldMovedActor       fun(self: Source.GameInstance.GameInstance, worldPath: string, actorTag: string)
---@field getWorldMovedActors         fun(self: Source.GameInstance.GameInstance, worldPath: string): Source.GameInstance.WorldMovedActorRecord[]
---@field recordDestroyedActorTag     fun(self: Source.GameInstance.GameInstance, mapPath: string, actorTag: string)
---@field recordDestroyedActor        fun(self: Source.GameInstance.GameInstance, mapPath: string, actor: Engine.Actor)
---@field setCurrentRegion            fun(self: Source.GameInstance.GameInstance, region: string)
local GameInstance = {}

---@brief Construct a new game instance with a default player.
---@param skipDefaultPlayer boolean | nil
function GameInstance:init(skipDefaultPlayer) end

---@brief Serialize the game instance to a dictionary.
---
--- - @return A dictionary containing ordered player keys, keyed players, variables, map, destroyed actors, and screenshot.
---@return Source.GameInstance.SaveData
function GameInstance:asDict() end

---@brief Deserialize a game instance from a dictionary.
---
--- - @param data The serialised game instance data.
--- - @return A restored GameInstance.
---@param data Source.GameInstance.SaveData
---@return Source.GameInstance.GameInstance
function GameInstance.FromDict(data) end

---@brief Get the current region.
---
--- - @return The current region.
---@return string
function GameInstance:getCurrentRegion() end

---@brief Set the current region.
---
--- - @param region The region to set.
---@param region string
function GameInstance:setCurrentRegion(region) end

---@brief Set the captured screenshot bytes used for save thumbnails.
---
--- - @param screenshot Encoded image bytes (PNG) or nil to clear.
---@param screenshot integer[] | nil
function GameInstance:setScreenshot(screenshot) end

---@brief Get the captured screenshot bytes.
---
--- - @return Encoded image bytes (PNG) or nil.
---@return integer[] | nil
function GameInstance:getScreenshot() end

---@brief Get all game variables.
---
--- - @return A dictionary of all game variables.
---@return table<string, Source.GameInstance.RecordValue>
function GameInstance:getVariables() end

---@brief Get a game variable by name.
---
--- - @param name The variable name.
--- - @return The variable value, or nil if not found.
---@param name string
---@return Source.GameInstance.RecordValue
function GameInstance:getVariable(name) end

---@brief Set a game variable.
---
--- - @param name The variable name.
--- - @param value The value to set.
---@param name  string
---@param value Source.GameInstance.RecordValue
function GameInstance:setVariable(name, value) end

---@brief Get the first (primary) player.
---
--- - @return The primary player.
---@return Source.Player.Player
function GameInstance:getPlayer() end

---@brief Make an existing keyed player the primary player.
---
--- Moves the key to the first position in the player order.
---
--- - @param playerKey The existing player key to make primary.
---@param playerKey string
function GameInstance:setPlayer(playerKey) end

---@brief Get all players.
---
--- - @return A dictionary keyed by Player General Data key.
---@return table<string, Source.Player.Player>
function GameInstance:getPlayers() end

---@brief Get player keys in their current order.
---
--- - @return The live ordered player-key list; the first key identifies the primary player.
---@return string[]
function GameInstance:getPlayerKeys() end

---@brief Get a player by index.
---
--- - @param index The zero-based index in the ordered player-key list.
--- - @return The player at the given index.
---@param index integer
---@return Source.Player.Player
function GameInstance:getPlayerByIndex(index) end

---@brief Find a player by tag.
---
--- - @param tag The player tag to search for.
--- - @return The matching player, or nil.
---@param tag string
---@return Source.Player.Player | nil
function GameInstance:getPlayerByTag(tag) end

---@brief Add a new player by class path.
---
--- - @param playerClass The class path for the player blueprint.
---@param playerClass string
function GameInstance:addPlayerByClass(playerClass) end

---@brief Remove a player by class path.
---
--- - @param playerClass The class path to remove.
---@param playerClass string
function GameInstance:removePlayerByClass(playerClass) end

---@brief Apply map information for scene transitions.
---
--- - @param mapPath The new map path to cache.
--- - @param pos The position to set the primary player to.
---@param mapPath  string
---@param position sf.Vector2i | nil
function GameInstance:applyMapInfo(mapPath, position) end

---@brief Record an added actor for persistence.
---
--- - @param mapPath The map path where the actor was added.
--- - @param actor The added actor, identified by its non-empty map-placement tag.
--- - @param layerName The actor layer name.
---@param mapPath   string
---@param actor     Engine.Actor
---@param layerName string
function GameInstance:recordAddedActor(mapPath, actor, layerName) end

---@param mapPath       string
---@param actor         Engine.Actor
---@param actorPosition sf.Vector2i | nil
function GameInstance:recordAddedActorPosition(mapPath, actor, actorPosition) end

---@brief Get added actor records for a map.
---
--- - @param mapPath The map path.
--- - @return A list of added actor records.
---@param mapPath string
---@return Source.GameInstance.AddedActorRecord[]
function GameInstance:getAddedActors(mapPath) end

---@brief Record an actor position change for persistence.
---
--- - @param mapPath The map path where the actor moved.
--- - @param actor The moved actor, identified by its non-empty map-placement tag.
---@param mapPath       string
---@param actor         Engine.Actor
---@param actorPosition sf.Vector2i | nil
function GameInstance:recordActorPosition(mapPath, actor, actorPosition) end

---@brief Get actor position records for a map.
---
--- - @param mapPath The map path.
--- - @return Map-placement-tag-indexed position records.
---@param mapPath string
---@return table<string, sf.Vector2i>
function GameInstance:getActorPositions(mapPath) end

---@brief Record or update the dedicated snapshot for an authored Actor root currently outside its definition child.
---
--- Passing the definition child as `currentRegion` removes the moved snapshot. An empty current region means that
--- the world position is in a legal hole.
---@param worldPath        string
---@param actor            Engine.Actor
---@param definitionRegion string
---@param currentRegion    string
---@param layerName        string
---@param actorPosition    sf.Vector2i | nil
function GameInstance:recordWorldMovedActor(worldPath, actor, definitionRegion, currentRegion, layerName, actorPosition) end

---@brief Remove one authored-root world movement snapshot by map-placement tag.
---@param worldPath string
---@param actorTag  string
function GameInstance:removeWorldMovedActor(worldPath, actorTag) end

---@brief Get authored-root movement snapshots stored under a world manifest identity.
---@param worldPath string
---@return Source.GameInstance.WorldMovedActorRecord[]
function GameInstance:getWorldMovedActors(worldPath) end

---@brief Record a destroyed Actor map-placement tag for persistence.
---@param mapPath  string
---@param actorTag string
function GameInstance:recordDestroyedActorTag(mapPath, actorTag) end

---@brief Record a destroyed actor for persistence.
---
--- - @param mapPath The map path where the actor was destroyed.
--- - @param actor The destroyed actor, identified by its non-empty map-placement tag.
---@param mapPath string
---@param actor   Engine.Actor
function GameInstance:recordDestroyedActor(mapPath, actor) end

---@brief Get destroyed Actor map-placement tags for a map.
---
--- - @param mapPath The map path.
--- - @return A list of destroyed Actor map-placement tags.
---@param mapPath string
---@return string[]
function GameInstance:getDestroyedActors(mapPath) end

---@brief Record a terrain tile replacement for persistence.
---
--- - @param mapPath The map path where the terrain was changed.
--- - @param layerName The tile layer name.
--- - @param position The tile coordinate.
--- - @param tileID The replacement tile ID, autotile key, or nil to clear the tile.
---@param mapPath   string
---@param layerName string
---@param position  sf.Vector2i
---@param tileID    Global.GameMap.TerrainTileID
function GameInstance:recordTerrainDestruction(mapPath, layerName, position, tileID) end

---@brief Get recorded terrain tile replacements for a map.
---
--- - @param mapPath The map path.
--- - @return Layer-indexed terrain replacement records.
---@param mapPath string
---@return table<string, table<string, Source.GameInstance.TerrainChangeRecord>>
function GameInstance:getTerrainDestructions(mapPath) end

---@brief Record a telepoint for persistence.
---
--- - @param mapPath The map path where the telepoint is located.
--- - @param telepoint The telepoint position.
---@param mapPath   string
---@param telepoint sf.Vector2u
function GameInstance:recordTelepoint(mapPath, telepoint) end

---@brief Get telepoint positions for a map.
---
--- - @param mapPath The map path.
--- - @return A list of telepoint positions.
---@param mapPath string
---@return sf.Vector2u[]
function GameInstance:getTelepoints(mapPath) end

---@brief Check whether an item or equip has been obtained before.
---
--- - @param itemID The item ID.
--- - @return True if the item or equip has already been obtained.
---@param itemID string
---@return boolean
function GameInstance:getCachedNewItem(itemID) end

---@brief Mark an item or equip as obtained before.
---
--- - @param itemID The item ID.
---@param itemID string
function GameInstance:setCachedNewItem(itemID) end

return GameInstance
