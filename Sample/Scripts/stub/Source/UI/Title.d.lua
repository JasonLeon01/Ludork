---@meta Source.UI.Title

---@class Source.UI.Title: Source.UI.UiController
---@field model Source.Scenes.SceneTitle
---@field new fun(model: Source.Scenes.SceneTitle): Source.UI.Title
local SceneTitleUI = {}

function SceneTitleUI:bind() end

function SceneTitleUI:refresh() end

---@return Source.Windows.WindowCommand
function SceneTitleUI:getCommandWindow() end

return SceneTitleUI
