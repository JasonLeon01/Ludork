local Ui = require("Source.UI.Ui")

---@class Source.UI.WindowFloorTeleporter
local WindowFloorTeleporterUI = {}

function WindowFloorTeleporterUI:bind()
    self._previewAsset = self:requireAsset("PreviewAsset")
    self._commandAsset = self:requireAsset("CommandAsset")
end

function WindowFloorTeleporterUI:attach()
    local logicalSize = sf.Vector2u.new(416, 240)
    ---@cast logicalSize sf.Vector2u
    self:attachTo(self.model, logicalSize)
end

function WindowFloorTeleporterUI:getPreviewAsset()
    return self._previewAsset
end

function WindowFloorTeleporterUI:getCommandAsset()
    return self._commandAsset
end

return Ui.Define("WindowFloorTeleporter", WindowFloorTeleporterUI)
