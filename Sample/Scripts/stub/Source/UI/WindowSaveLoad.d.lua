---@meta Source.UI.WindowSaveLoad

---@class Source.UI.WindowSaveLoad: Source.UI.UiController
---@field new fun(model: Source.Windows.WindowSaveLoad): Source.UI.WindowSaveLoad
local WindowSaveLoadUI = {}

function WindowSaveLoadUI:attach() end

---@return Engine.AssetInstance
function WindowSaveLoadUI:getTabsAsset() end

---@return Engine.AssetInstance
function WindowSaveLoadUI:getSlotAsset() end

---@return Engine.AssetInstance
function WindowSaveLoadUI:getDetailAsset() end

return WindowSaveLoadUI
