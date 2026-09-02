---@meta Source.UI.Parts.WindowShop.WindowShopDetail

---@class Source.UI.Parts.WindowShop.WindowShopDetail.WindowShopDetailUI: Source.UI.UiController
local WindowShopDetailUI = {}

---@param model    Source.Windows.WindowShopDetail
---@param size     sf.Vector2i
---@param instance Engine.AssetInstance | nil
---@return Source.UI.Parts.WindowShop.WindowShopDetail.WindowShopDetailUI
function WindowShopDetailUI.new(model, size, instance) end

---@param model Source.Windows.WindowShopDetail
---@param size  sf.Vector2i
function WindowShopDetailUI:init(model, size, instance) end

function WindowShopDetailUI:bind() end

function WindowShopDetailUI:refresh() end

---@return Engine.Canvas
function WindowShopDetailUI:prepare() end

function WindowShopDetailUI:attach(nested) end

---@return Engine.Window
function WindowShopDetailUI:getWindowFrame() end

---@return Engine.Canvas
function WindowShopDetailUI:getContent() end

---@param itemInfo Source.Data.GeneralItemData | nil
---@param price    integer | nil
function WindowShopDetailUI:setItem(itemInfo, price) end

return WindowShopDetailUI
