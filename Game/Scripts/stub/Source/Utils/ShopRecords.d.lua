---@meta Source.Utils.ShopRecords

local ShopRecords = {}

--- Construct the recorded map placement with its Blueprint overrides, without attaching it or dispatching onCreate.
--- World placements keep their world coordinates. Missing maps or tags return nil.
---@param mapPath string
---@param tag     string
---@return Engine.Actor | nil
function ShopRecords.CreateActor(mapPath, tag) end

return ShopRecords
