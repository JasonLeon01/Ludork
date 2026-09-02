---@meta Source.UI.WindowShop

---@class Source.UI.WindowShop: Source.UI.UiController
---@field new fun(model: Source.Windows.WindowShop): Source.UI.WindowShop
local WindowShopUI = {}

function WindowShopUI:attach() end

---@return Engine.AssetInstance
function WindowShopUI:getTabsAsset() end

---@return Engine.AssetInstance
function WindowShopUI:getItemAsset() end

---@return Engine.AssetInstance
function WindowShopUI:getDetailAsset() end

return WindowShopUI
