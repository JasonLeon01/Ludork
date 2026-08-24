---@meta Source.Data.Validation

---@class Source.Data.Validation
local Validation = {}

---@brief Read a required named value and fail with the supplied message when it is absent.
---@generic T
---@param values  table<string, T>
---@param key     string
---@param message string
---@return T
function Validation.RequireNamedValue(values, key, message) end

return Validation
