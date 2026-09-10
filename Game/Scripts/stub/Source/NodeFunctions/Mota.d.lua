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

--- @brief Teleport the player to the centre-symmetric tile on the current map.
---
--- The Failed execution branch displays the localised ``FLY_FAIL`` common tip.
---
--- - @return 0 on success, 1 on failure.
---@return integer
function Mota.CenterSymmetricTeleport() end

--- @brief Teleport the player to the same coordinates on the floor above.
---
--- The Failed execution branch displays the localised ``FLY_FAIL`` common tip.
---
--- - @return 0 on success, 1 on failure.
---@return integer
function Mota.GoUpstairsSamePos() end

--- @brief Teleport the player to the same coordinates on the floor below.
---
--- The Failed execution branch displays the localised ``FLY_FAIL`` common tip.
---
--- - @return 0 on success, 1 on failure.
---@return integer
function Mota.GoDownstairsSamePos() end

return Mota
