---@meta Source.NodeFunctions.Mota

--- @brief Open the current-map monster handbook.
function Mota.OpenMonsterBook() end

--- @brief Open the visited-floor teleporter preview window.
function Mota.OpenFloorTeleporter() end

--- @brief Get the current mota region.
---
--- - @return The current region, or an empty string when unavailable.
---@return string
function Mota.GetCurrentRegion() end

--- @brief Set the current mota region.
---
--- - @param region The region name to set.
---@param region string
function Mota.SetCurrentRegion(region) end

return Mota
