---@meta Internal.LiveDebug.Tiles

---@class Internal.LiveDebug.Tiles.Module
local Tiles = {}

---@return Global.GameMap.TerrainChanges.Owner | nil
function Tiles.GetOwner() end

--- Release the active terrain subscription and queued updates.
function Tiles.Reset() end

---@param owner Global.GameMap.TerrainChanges.Owner
function Tiles.Bind(owner) end

--- Consume bounded cell updates, requiring a full map for unobserved layer replacement or structural changes.
---@param view         Internal.LiveDebug.View
---@param forceFullMap boolean
---@return boolean, Internal.LiveDebug.TileChange[] | nil
function Tiles.Collect(view, forceFullMap) end

return Tiles
