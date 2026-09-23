---@meta Source.Windows.WindowAttrShop.AttrShopRow.Controller

---@class Source.Windows.WindowAttrShop.AttrShopRow.Controller: Internal.UIBase.UiController
---@field ui           Internal.UI.Parts.WindowAttrShop.AttrShopRow
---@field new          fun(model: table): Source.Windows.WindowAttrShop.AttrShopRow.Controller
---@field model        { text: string, available: boolean }
---@field root         Engine.Canvas
---@field _labelColour sf.Color
local AttrShopRowController = {}

function AttrShopRowController:bind() end

function AttrShopRowController:refresh() end

---@param logicalSize sf.Vector2u
---@return Engine.Canvas
function AttrShopRowController:prepare(logicalSize) end

return AttrShopRowController
