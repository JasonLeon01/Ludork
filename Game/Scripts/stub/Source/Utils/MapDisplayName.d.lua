---@meta Source.Utils.MapDisplayName

local MapDisplayName = {}

--- Resolve and localise a map's display name, using the localised map key when the map is absent or unnamed.
---@param mapKey         string
---@param currentMapPath string | nil
---@return string
function MapDisplayName.Resolve(mapKey, currentMapPath) end

return MapDisplayName
