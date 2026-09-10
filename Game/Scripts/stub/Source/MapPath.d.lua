---@meta Source.MapPath

---@param mapPath string | nil
---@return string
function MapPath.Normalise(mapPath) end

---@param mapPath string
---@return string
function MapPath.WithoutExtension(mapPath) end

---@param mapPath string
---@return string
function MapPath.BasenameWithoutExtension(mapPath) end

return MapPath
