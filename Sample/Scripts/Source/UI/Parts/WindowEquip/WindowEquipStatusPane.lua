local Ui = require("Source.UI.Ui")

---@class Source.UI.Parts.WindowEquip.WindowEquipStatusPane
local WindowEquipStatusPaneUI = {}

function WindowEquipStatusPaneUI:init(model, instance)
    super(WindowEquipStatusPaneUI, self).init(model, instance)
    local logicalSize = sf.Vector2u.new(256, 352)
    ---@cast logicalSize sf.Vector2u
    self._logicalSize = logicalSize
end

function WindowEquipStatusPaneUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._statusAsset = self:requireAsset("StatusAsset")
    ---@cast self._windowFrame Engine.Window
    ---@cast self._content Engine.Canvas
end

function WindowEquipStatusPaneUI:attach()
    self:attachNestedWindowView(self.model, self._logicalSize)
end

function WindowEquipStatusPaneUI:getWindowFrame()
    return self._windowFrame
end

function WindowEquipStatusPaneUI:getContent()
    return self._content
end

function WindowEquipStatusPaneUI:getStatusAsset()
    return self._statusAsset
end

return Ui.Define("Parts/WindowEquip/WindowEquipStatusPane", WindowEquipStatusPaneUI)
