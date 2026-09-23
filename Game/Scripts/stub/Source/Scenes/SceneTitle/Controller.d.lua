---@meta Source.Scenes.SceneTitle.Controller

---@class Source.Scenes.SceneTitle.Controller: Internal.UIBase.UiController
---@field ui             Internal.UI.Title
---@field model          Source.Scenes.SceneTitle
---@field refreshEvents  string[]
---@field _commandModels Internal.UIBase.CommandRow.Controller.Model[]
---@field _windowCommand Source.Windows.WindowCommand
---@field new            fun(model: Source.Scenes.SceneTitle): Source.Scenes.SceneTitle.Controller
local SceneTitleController = {}

function SceneTitleController:bind() end

function SceneTitleController:refresh() end

---@return Source.Windows.WindowCommand
function SceneTitleController:getCommandWindow() end

return SceneTitleController
