---@meta Source.UI.Parts.WindowAttrShop.AttrShopRow

---@class Source.UI.Parts.WindowAttrShop.AttrShopRow: Source.UI.UiController
---@field new    fun(model: table): Source.UI.Parts.WindowAttrShop.AttrShopRow
---@field model  { text: string, available: boolean }
---@field root   Engine.Canvas
---@field _label Engine.PlainText
local AttrShopRowUI = {}

function AttrShopRowUI:bind() end

function AttrShopRowUI:refresh() end

---@param logicalSize sf.Vector2u
---@return Engine.Canvas
function AttrShopRowUI:prepare(logicalSize) end

return AttrShopRowUI
