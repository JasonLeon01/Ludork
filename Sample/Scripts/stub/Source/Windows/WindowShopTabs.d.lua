---@meta Source.Windows.WindowShopTabs

---@class Source.Windows.WindowShopTabs: Source.Windows.Base.WindowBase
---@field new      fun(rect: sf.IntRect, owner: Source.Windows.WindowShop, instance?: Engine.AssetInstance): Source.Windows.WindowShopTabs
---@field _owner   Source.Windows.WindowShop
---@field _ui      Source.UI.Parts.WindowShop.WindowShopTabs.WindowShopTabsUI
---@field _tabView Engine.TabView
local WindowShopTabs = {}

---@param rect  sf.IntRect
---@param owner Source.Windows.WindowShop
---@return Source.Windows.WindowShopTabs
function WindowShopTabs.new(rect, owner) end

---@param rect  sf.IntRect
---@param owner Source.Windows.WindowShop
function WindowShopTabs:init(rect, owner, instance) end

---@return Engine.TabView
function WindowShopTabs:getTabView() end

---@param index integer
function WindowShopTabs:onSelectedIndexChanged(index) end

---@return boolean
function WindowShopTabs:handleNavigationInput() end

function WindowShopTabs:refresh() end

function WindowShopTabs:dispose() end

return WindowShopTabs
