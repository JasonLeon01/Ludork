local Ui = require("Source.UI.Ui")

---@class Source.UI.WindowShop
local WindowShopUI = {}

function WindowShopUI:bind()
    self._tabsAsset = self:requireAsset("TabsAsset")
    self._itemAsset = self:requireAsset("ItemAsset")
    self._detailAsset = self:requireAsset("DetailAsset")
end

function WindowShopUI:attach()
    local logicalSize = sf.Vector2u.new(352, 416)
    ---@cast logicalSize sf.Vector2u
    self:attachTo(self.model, logicalSize)
end

function WindowShopUI:getTabsAsset()
    return self._tabsAsset
end

function WindowShopUI:getItemAsset()
    return self._itemAsset
end

function WindowShopUI:getDetailAsset()
    return self._detailAsset
end

return Ui.Define("WindowShop", WindowShopUI)
