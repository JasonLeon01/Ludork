---@meta Source.GameInstance.SaveCodec

local SaveCodec = {}

---@param instance Source.GameInstance.GameInstance
---@return Source.GameInstance.SaveData
function SaveCodec.Encode(instance) end

---@param instance Source.GameInstance.GameInstance
---@param data     Source.GameInstance.SaveData
function SaveCodec.DecodeInto(instance, data) end

return SaveCodec
