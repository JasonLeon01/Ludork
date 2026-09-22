---@meta Source.Windows.WindowShop.WindowShopCell.Controller

---@class Source.Windows.WindowShop.WindowShopCell.Controller.Model
---@field iconTexture sf.Texture | nil
---@field available   boolean
---@field showValue   boolean
---@field value       number | nil
---@field callback    function | nil

---@class Source.Windows.WindowShop.WindowShopCell.Controller: Source.UIBase.UiController
---@field ui           Source.UI.Parts.WindowShop.WindowShopCell
---@field model        Source.Windows.WindowShop.WindowShopCell.Controller.Model
---@field new          fun(model: Source.Windows.WindowShop.WindowShopCell.Controller.Model): Source.Windows.WindowShop.WindowShopCell.Controller
---@field _iconColour  sf.Color
---@field _valueColour sf.Color
local WindowShopCellController = {}

function WindowShopCellController:bind() end

function WindowShopCellController:refresh() end

return WindowShopCellController
