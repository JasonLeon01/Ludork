---@meta Source.GameInstance.Records

local Records = {}

---@param player Source.Player.Player
---@return string
function Records.RequirePlayerKey(player) end

---@param players    table<string, Source.Player.Player>
---@param playerKeys string[]
---@param player     Source.Player.Player
function Records.AppendPlayer(players, playerKeys, player) end

---@param records Source.GameInstance.AddedActorRecord[]
---@param record  Source.GameInstance.AddedActorRecord
function Records.UpsertTaggedRecord(records, record) end

---@param points   Source.GameInstance.TelepointRecord[]
---@param position sf.Vector2u
---@param tag      string
function Records.AppendUniqueTelepoint(points, position, tag) end

---@param changes table<string, Source.Data.ClassVarValue> | nil
---@return table<string, Source.GameInstance.RecordValue>
function Records.NormaliseClassVarChanges(changes) end

---@param actor     Engine.Actor
---@param layerName string
---@return Source.GameInstance.AddedActorRecord | nil
function Records.BuildAddedActorRecord(actor, layerName) end

---@param actor            Engine.Actor
---@param definitionRegion string
---@param currentRegion    string
---@param layerName        string
---@param actorPosition    sf.Vector2i
---@return Source.GameInstance.WorldMovedActorRecord | nil
function Records.BuildWorldMovedActorRecord(actor, definitionRegion, currentRegion, layerName, actorPosition) end

---@param terrainDestructions table<string, table<string, table<string, Source.GameInstance.TerrainChangeRecord>>>
---@param mapPath             string
---@param layerName           string
---@param position            sf.Vector2i
---@param tileID              Global.GameMap.TerrainTileID
function Records.StoreTerrainChange(terrainDestructions, mapPath, layerName, position, tileID) end

return Records
