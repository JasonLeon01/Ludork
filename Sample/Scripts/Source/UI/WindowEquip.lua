local Ui = require("Source.UI.Ui")

---@class Source.UI.WindowEquip
local WindowEquipUI = {}

function WindowEquipUI:bind()
    self._slotAsset = self:requireAsset("SlotAsset")
    self._selectAsset = self:requireAsset("SelectAsset")
    self._statusPaneAsset = self:requireAsset("StatusPaneAsset")
end

function WindowEquipUI:attach()
    local logicalSize = sf.Vector2u.new(448, 352)
    ---@cast logicalSize sf.Vector2u
    self:attachTo(self.model, logicalSize)
end

function WindowEquipUI:getSlotAsset()
    return self._slotAsset
end

function WindowEquipUI:getSelectAsset()
    return self._selectAsset
end

function WindowEquipUI:getStatusPaneAsset()
    return self._statusPaneAsset
end

return Ui.Define("WindowEquip", WindowEquipUI)
