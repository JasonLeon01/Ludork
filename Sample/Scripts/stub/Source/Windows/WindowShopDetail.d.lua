---@meta

---@class Source.Windows.WindowShopDetail.Controller: Source.UIBase.UiController
---@field host      Source.Windows.WindowShopDetail
---@field ui        Source.UI.Parts.WindowShop.WindowShopDetail
---@field _itemInfo Source.Data.GeneralItemData | nil
---@field _price    integer | nil
local Controller = {}

function Controller:init() end

---@param itemInfo Source.Data.GeneralItemData | nil
---@param price    integer | nil
function Controller:setItem(itemInfo, price) end

function Controller:refresh() end

function Controller:dispose() end
