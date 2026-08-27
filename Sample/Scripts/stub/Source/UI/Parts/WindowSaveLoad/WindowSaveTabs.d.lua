---@meta Source.UI.Parts.WindowSaveLoad.WindowSaveTabs

---@param model Source.Windows.WindowSaveTabs
---@param size  sf.Vector2i
function WindowSaveTabsUI:init(model, size) end

function WindowSaveTabsUI:bind() end

function WindowSaveTabsUI:refresh() end

---@return Engine.Canvas
function WindowSaveTabsUI:prepare() end

function WindowSaveTabsUI:attach() end

---@return Engine.Window
function WindowSaveTabsUI:getWindowFrame() end

---@return Engine.Canvas
function WindowSaveTabsUI:getContent() end

---@return Engine.TabView
function WindowSaveTabsUI:getTabView() end

function WindowSaveTabsUI:dispose() end

return WindowSaveTabsUI
