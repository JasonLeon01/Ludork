---@meta

---@brief Non-focusable load/save tab window.
---@class Source.Windows.WindowSaveTabs.Controller: Source.UIBase.UiController
---@field host Source.Windows.WindowSaveTabs
---@field ui   Source.UI.Parts.WindowSaveLoad.WindowSaveTabs
local Controller = {}

---@param owner Source.Windows.WindowSaveLoad
function Controller:init(owner) end

---@param index integer
function Controller:onSelectedIndexChanged(index) end

---@return boolean
function Controller:handleNavigationInput() end

function Controller:dispose() end

function Controller:bind() end

function Controller:refresh() end
