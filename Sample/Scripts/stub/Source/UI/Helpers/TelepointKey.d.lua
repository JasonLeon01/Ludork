---@meta Source.UI.Helpers.TelepointKey

local TelepointKey = {}

---@param point sf.Vector2u | nil
---@return tuple<any>
function TelepointKey.FromPoint(point) end

---@param entries table
---@return tuple<any>
function TelepointKey.FromEntries(entries) end

return TelepointKey
