---@meta Source.UI.Parts.WindowSaveLoad.WindowSaveTabs

---@class Source.UI.Parts.WindowSaveLoad.WindowSaveTabs: Source.UI.UiController
---@field new fun(model: table, size: sf.Vector2i, instance?: Engine.AssetInstance): Source.UI.Parts.WindowSaveLoad.WindowSaveTabs
local WindowSaveTabsUI = {}

---@param model Source.Windows.WindowSaveTabs
---@param size  sf.Vector2i
function WindowSaveTabsUI:init(model, size, instance) end

function WindowSaveTabsUI:bind() end

function WindowSaveTabsUI:refresh() end

---@return Engine.Canvas
function WindowSaveTabsUI:prepare() end

function WindowSaveTabsUI:attach(nested) end

---@return Engine.Window
function WindowSaveTabsUI:getWindowFrame() end

---@return Engine.Canvas
function WindowSaveTabsUI:getContent() end

---@return Engine.TabView
function WindowSaveTabsUI:getTabView() end

function WindowSaveTabsUI:dispose() end

return WindowSaveTabsUI
