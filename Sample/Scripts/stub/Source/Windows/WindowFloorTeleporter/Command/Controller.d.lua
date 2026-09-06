---@meta Source.Windows.WindowFloorTeleporter.Command.Controller

---@class Source.Windows.WindowFloorTeleporter.Command.Controller: Source.Windows.WindowCommand.Controller
---@field model    Source.Windows.WindowFloorMapCommand
---@field _mapKeys string[]
---@field new      fun(model: Source.Windows.WindowFloorMapCommand, size: sf.Vector2u, rowHeight: integer, columns: integer): Source.Windows.WindowFloorTeleporter.Command.Controller
local WindowFloorMapCommandController = {}

---@param model     Source.Windows.WindowFloorMapCommand
---@param size      sf.Vector2u
---@param rowHeight integer
---@param columns   integer
function WindowFloorMapCommandController:init(model, size, rowHeight, columns) end

---@param entries { [1]: string, [2]: string } []
function WindowFloorMapCommandController:refreshMaps(entries) end

---@return string | nil
function WindowFloorMapCommandController:getCurrentMapKey() end

function WindowFloorMapCommandController:afterTick() end

return WindowFloorMapCommandController
