---@meta

---@class Source.Windows.WindowShopTabs.Controller: Source.UIBase.UiController
---@field host   Source.Windows.WindowShopTabs
---@field _owner Source.Windows.WindowShop
---@field ui     Source.UI.Parts.WindowShop.WindowShopTabs
local Controller = {}

---@param owner Source.Windows.WindowShop
function Controller:init(owner) end

---@param index integer
function Controller:onSelectedIndexChanged(index) end

---@return boolean
function Controller:handleNavigationInput() end

function Controller:refresh() end

function Controller:dispose() end

function Controller:bind() end
