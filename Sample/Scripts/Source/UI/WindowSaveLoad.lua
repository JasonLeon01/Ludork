local Ui = require("Source.UI.Ui")

---@class Source.UI.WindowSaveLoad
local WindowSaveLoadUI = {}

function WindowSaveLoadUI:bind()
    self._tabsAsset = self:requireAsset("TabsAsset")
    self._slotAsset = self:requireAsset("SlotAsset")
    self._detailAsset = self:requireAsset("DetailAsset")
end

function WindowSaveLoadUI:attach()
    local logicalSize = sf.Vector2u.new(416, 320)
    ---@cast logicalSize sf.Vector2u
    self:attachTo(self.model, logicalSize)
end

function WindowSaveLoadUI:getTabsAsset()
    return self._tabsAsset
end

function WindowSaveLoadUI:getSlotAsset()
    return self._slotAsset
end

function WindowSaveLoadUI:getDetailAsset()
    return self._detailAsset
end

return Ui.Define("WindowSaveLoad", WindowSaveLoadUI)
