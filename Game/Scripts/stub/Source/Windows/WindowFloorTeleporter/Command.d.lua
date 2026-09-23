---@meta

---@brief Command list displaying visited maps in the current region.
---@class Source.Windows.WindowFloorMapCommand.Controller: Internal.UIBase.UiController
---@field host      Source.Windows.WindowFloorMapCommand
---@field _owner    Source.Windows.WindowFloorTeleporter
---@field _mapKeys  string[]
---@field ui        Internal.UI.Parts.WindowFloorTeleporter.WindowFloorMapCommand
---@field _commands Internal.UIBase.UiCollection<Internal.UIBase.CommandRow.Controller>
local Controller = {}

---@brief Construct the floor map command list.
---
--- - @param owner The parent floor teleporter coordinator.
---@param owner Source.Windows.WindowFloorTeleporter
function Controller:init(owner) end

---@brief Rebuild the list from map key/name pairs.
---
--- - @param entries Region map entries to display.
---@param entries table
function Controller:refreshMaps(entries) end

---@brief Get the selected region map key.
---
--- - @return The selected map key, or nil when no map is selected.
---@return string | nil
function Controller:getCurrentMapKey() end

---@param deltaTime number
function Controller:onTick(deltaTime) end

function Controller:onReturn() end

function Controller:activateTelepointSelector() end

---@param index integer | nil
function Controller:notifyMapIndexMaybeChanged(index) end

function Controller:refreshRows() end

function Controller:afterTick() end
