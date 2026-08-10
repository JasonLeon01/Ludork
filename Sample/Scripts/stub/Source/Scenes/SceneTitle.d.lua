---@meta Source.Scenes.SceneTitle
local Scene = {}

---@return any
function Scene.new(...) end

--- @brief Start with a blind transition effect.
function Scene:onEnter() end

--- @brief Create background, command window, and load UI elements.
function Scene:onCreate() end

--- @brief Stop title BGM when leaving this scene.
function Scene:onQuit() end

--- @brief Ensure title BGM is stopped when scene is destroyed.
function Scene:onDestroy() end

return Scene
