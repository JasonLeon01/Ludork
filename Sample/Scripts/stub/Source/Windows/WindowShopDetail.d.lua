---@meta Source.Windows.WindowShopDetail

---@class Source.Windows.WindowShopDetail: Source.Windows.Base.WindowBase
---@field new       fun(rect: sf.IntRect, instance?: Engine.AssetInstance): Source.Windows.WindowShopDetail
---@field _ui       Source.UI.Parts.WindowShop.WindowShopDetail.WindowShopDetailUI
---@field _itemInfo Source.Data.GeneralItemData | nil
---@field _price    integer | nil
local WindowShopDetail = {}

---@param rect sf.IntRect
---@return Source.Windows.WindowShopDetail
function WindowShopDetail.new(rect) end

---@param rect sf.IntRect
function WindowShopDetail:init(rect, instance) end

---@param itemInfo Source.Data.GeneralItemData | nil
---@param price    integer | nil
function WindowShopDetail:setItem(itemInfo, price) end

function WindowShopDetail:refresh() end

function WindowShopDetail:dispose() end

return WindowShopDetail
