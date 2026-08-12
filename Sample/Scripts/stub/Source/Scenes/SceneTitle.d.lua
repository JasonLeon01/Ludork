---@meta Source.Scenes.SceneTitle
---@class Source.Scenes.SceneTitle: GlobalCore.SceneBase
---@field new fun(): Source.Scenes.SceneTitle
---@field _ui Source.UI.Title
---@field _windowCommand Source.Windows.WindowCommand
---@field _windowSaveLoad Source.Windows.WindowSaveLoad
---@field _configWindow Source.Windows.ConfigWindow
local Scene = {}

--- @brief Start with a blind transition effect.
function Scene:onEnter() end

--- @brief Create background, command window, and load UI elements.
function Scene:onCreate() end

--- @brief Stop title BGM when leaving this scene.
function Scene:onQuit() end

--- @brief Ensure title BGM is stopped when scene is destroyed.
function Scene:onDestroy() end

return Scene
