---@meta Source.Windows.WindowFloorTeleporter.Command

---@brief Command list displaying visited maps in the current region.
---@class Source.Windows.WindowFloorMapCommand: Source.Windows.WindowCommand
---@field new            fun(rect: sf.IntRect, owner: Source.Windows.WindowFloorTeleporter, instance?: Engine.AssetInstance): Source.Windows.WindowFloorMapCommand
---@field _owner         Source.Windows.WindowFloorTeleporter
---@field _mapController Source.Windows.WindowFloorMapCommandController
---@field _mapKeys       string[]
---@field new            fun(rect: sf.IntRect, owner: Source.Windows.WindowFloorTeleporter): Source.Windows.WindowFloorMapCommand
local WindowFloorMapCommand = {}

---@brief Construct the floor map command list.
---
--- - @param rect The command list window rectangle.
--- - @param owner The parent floor teleporter coordinator.
---@param rect  sf.IntRect
---@param owner Source.Windows.WindowFloorTeleporter
function WindowFloorMapCommand:init(rect, owner, instance) end

---@brief Rebuild the list from map key/name pairs.
---
--- - @param entries Region map entries to display.
---@param entries table
function WindowFloorMapCommand:refreshMaps(entries) end

---@brief Get the selected region map key.
---
--- - @return The selected map key, or nil when no map is selected.
---@return string | nil
function WindowFloorMapCommand:getCurrentMapKey() end

---@param deltaTime number
function WindowFloorMapCommand:onTick(deltaTime) end

function WindowFloorMapCommand:onReturn() end

return WindowFloorMapCommand
