---@meta Framework.Codec

---@alias LudorkServer.Key boolean | number | string
---@alias LudorkServer.Value nil | boolean | number | string | table<LudorkServer.Key, LudorkServer.Value>
---@alias LudorkServer.Table table<LudorkServer.Key, LudorkServer.Value>

---@alias LudorkServer.WireKey ["bool", boolean] | ["int", string] | ["float", string] | ["string", string]
---@alias LudorkServer.WireValue ["nil"] | LudorkServer.WireKey | ["table", [LudorkServer.WireKey, LudorkServer.WireValue][]]

local Codec = {}

--- Encode ordinary, acyclic Lua data as an ASCII JSON wire value.
--- Strings preserve arbitrary bytes; integers preserve the full signed 64-bit range.
--- Numbers must be finite. Tables cannot have metatables, cycles or table keys.
--- Shared tables are encoded as independent copies. Depth is limited to 64 and nodes to 65536.
---@param value LudorkServer.Value
---@return string
function Codec.Encode(value) end

--- Encode positional arguments as a JSON array of wire values, including every nil position.
--- The same depth and combined node limits apply to all arguments.
---@param ... LudorkServer.Value
---@return string
function Codec.EncodeArgs(...) end

--- Decode a host-validated wire value into ordinary Lua data.
---@param value LudorkServer.WireValue
---@return LudorkServer.Value
function Codec.Decode(value) end

return Codec
