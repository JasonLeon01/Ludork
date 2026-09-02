---@meta Source.Windows.WindowSaveLoad.Tabs

---@brief Non-focusable load/save tab window.
---@class Source.Windows.WindowSaveTabs: Source.Windows.Base.WindowBase
---@field new fun(rect: sf.IntRect, owner: Source.Windows.WindowSaveLoad, instance?: Engine.AssetInstance): Source.Windows.WindowSaveTabs
---@field new fun(rect: sf.IntRect, owner: Source.Windows.WindowSaveLoad): Source.Windows.WindowSaveTabs
local WindowSaveTabs = {}

---@param rect  sf.IntRect
---@param owner Source.Windows.WindowSaveLoad
function WindowSaveTabs:init(rect, owner, instance) end

---@return Engine.TabView
function WindowSaveTabs:getTabView() end

---@param index integer
function WindowSaveTabs:onSelectedIndexChanged(index) end

---@return boolean
function WindowSaveTabs:handleNavigationInput() end

function WindowSaveTabs:dispose() end

return WindowSaveTabs
