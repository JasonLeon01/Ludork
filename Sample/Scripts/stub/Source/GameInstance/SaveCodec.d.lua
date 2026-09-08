---@meta Source.GameInstance.SaveCodec

---@class Source.GameInstance.SaveState
---@field playerKeys          string[]
---@field players             table<string, Source.Player.Player>
---@field currentRegion       string
---@field variables           table<string, Source.GameInstance.RecordValue>
---@field currentMap          string | nil
---@field addedActors         table<string, Source.GameInstance.AddedActorRecord[]>
---@field actorPositions      table<string, table<string, sf.Vector2i>>
---@field worldMovedActors    table<string, Source.GameInstance.WorldMovedActorRecord[]>
---@field destroyedActors     table<string, string[]>
---@field terrainDestructions table<string, table<string, table<string, Source.GameInstance.TerrainChangeRecord>>>
---@field obtainedItems       table<string, boolean>
---@field telepoints          table<string, Source.GameInstance.TelepointRecord[]>
---@field screenshot          integer[] | nil

local SaveCodec = {}

---@param state Source.GameInstance.SaveState
---@return Source.GameInstance.SaveData
function SaveCodec.Encode(state) end

---@param data Source.GameInstance.SaveData
---@return Source.GameInstance.SaveState
function SaveCodec.Decode(data) end

return SaveCodec
