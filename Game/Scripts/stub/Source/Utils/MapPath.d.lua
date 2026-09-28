---@meta Source.Utils.MapPath

---@param mapPath string | nil
---@return string
function MapPath.Normalise(mapPath) end

--- Return the normalised map path with only its final filename extension removed.
---@param mapPath string
---@return string
function MapPath.WithoutExtension(mapPath) end

--- Return the filename of the normalised map path without its final extension.
---@param mapPath string
---@return string
function MapPath.BasenameWithoutExtension(mapPath) end

return MapPath
