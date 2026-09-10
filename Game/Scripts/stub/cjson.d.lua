---@meta cjson
---@module 'cjson'

---@class cjson
local cjson = {}

---@alias cjson.JsonValue nil|boolean|number|string|lightuserdata|table

---@type lightuserdata
cjson.null = nil

---@type string
cjson._NAME = "cjson"

---@type string
cjson._VERSION = "2.1.0"

--- Serialise a Lua value into a JSON string.
---@param value cjson.JsonValue
---@return string
function cjson.encode(value) end

--- Deserialise a UTF-8 JSON string into a Lua value.
---@param json_text string
---@return cjson.JsonValue
function cjson.decode(json_text) end

--- Create an independent cjson module instance with its own configuration.
---@return cjson
function cjson.new() end

--- Get and/or set sparse-array encoding behaviour.
---@param convert boolean|nil
---@param ratio integer|nil
---@param safe integer|nil
---@return boolean, integer, integer
function cjson.encode_sparse_array(convert, ratio, safe) end

--- Get and/or set the maximum nesting depth when encoding.
---@param depth integer|nil
---@return integer
function cjson.encode_max_depth(depth) end

--- Get and/or set the maximum nesting depth when decoding.
---@param depth integer|nil
---@return integer
function cjson.decode_max_depth(depth) end

--- Get and/or set the number of significant digits used when encoding numbers.
---@param precision integer|nil
---@return integer
function cjson.encode_number_precision(precision) end

--- Get and/or set whether encode reuses an internal buffer.
---@param keep boolean|nil
---@return boolean
function cjson.encode_keep_buffer(keep) end

--- Get and/or set how invalid numbers are encoded (false / true / "null").
---@param setting boolean|string|nil
---@return boolean|string
function cjson.encode_invalid_numbers(setting) end

--- Get and/or set whether invalid numbers are accepted when decoding.
---@param setting boolean|nil
---@return boolean
function cjson.decode_invalid_numbers(setting) end

return cjson
