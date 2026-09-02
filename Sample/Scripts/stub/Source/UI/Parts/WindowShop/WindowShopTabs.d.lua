---@meta Source.UI.Parts.WindowShop.WindowShopTabs

---@class Source.UI.Parts.WindowShop.WindowShopTabs.WindowShopTabsUI: Source.UI.UiController
local WindowShopTabsUI = {}

---@param model    Source.Windows.WindowShopTabs
---@param size     sf.Vector2i
---@param instance Engine.AssetInstance | nil
---@return Source.UI.Parts.WindowShop.WindowShopTabs.WindowShopTabsUI
function WindowShopTabsUI.new(model, size, instance) end

---@param model Source.Windows.WindowShopTabs
---@param size  sf.Vector2i
function WindowShopTabsUI:init(model, size, instance) end

function WindowShopTabsUI:bind() end

function WindowShopTabsUI:refresh() end

---@return Engine.Canvas
function WindowShopTabsUI:prepare() end

function WindowShopTabsUI:attach(nested) end

---@return Engine.Window
function WindowShopTabsUI:getWindowFrame() end

---@return Engine.Canvas
function WindowShopTabsUI:getContent() end

---@return Engine.TabView
function WindowShopTabsUI:getTabView() end

function WindowShopTabsUI:dispose() end

return WindowShopTabsUI
