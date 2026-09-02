---@meta Source.UI.WindowFloorTeleporter

---@class Source.UI.WindowFloorTeleporter: Source.UI.UiController
---@field new fun(model: Source.Windows.WindowFloorTeleporter): Source.UI.WindowFloorTeleporter
local WindowFloorTeleporterUI = {}

function WindowFloorTeleporterUI:attach() end

---@return Engine.AssetInstance
function WindowFloorTeleporterUI:getPreviewAsset() end

---@return Engine.AssetInstance
function WindowFloorTeleporterUI:getCommandAsset() end

return WindowFloorTeleporterUI
