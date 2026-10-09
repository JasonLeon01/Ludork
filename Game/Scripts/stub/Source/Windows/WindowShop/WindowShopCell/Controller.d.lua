---@meta Source.Windows.WindowShop.WindowShopCell.Controller

---@class Source.Windows.WindowShop.WindowShopCell.Controller.Model
---@field iconTexture sf.Texture | nil
---@field available   boolean
---@field showValue   boolean
---@field value       number | nil
---@field callback    function | nil

---@class Source.Windows.WindowShop.WindowShopCell.Controller: Internal.UIBase.UiController
---@field ui           Internal.UI.Parts.WindowShop.WindowShopCell
---@field root         Engine.Canvas
---@field model        Source.Windows.WindowShop.WindowShopCell.Controller.Model
---@field new          fun(model: Source.Windows.WindowShop.WindowShopCell.Controller.Model): Source.Windows.WindowShop.WindowShopCell.Controller
---@field _iconColour  sf.Color
---@field _valueColour sf.Color
local WindowShopCellController = {}

---@param model Source.Windows.WindowShop.WindowShopCell.Controller.Model
function WindowShopCellController:init(model) end

function WindowShopCellController:bind() end

function WindowShopCellController:refresh() end

return WindowShopCellController
