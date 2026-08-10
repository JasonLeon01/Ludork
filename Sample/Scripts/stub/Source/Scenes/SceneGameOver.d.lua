---@meta Source.Scenes.SceneGameOver
local Scene = {}

---@return any
function Scene.new(...) end

--- @brief Fade in the game over screen.
function Scene:onEnter() end

--- @brief Create the black background and centred game over text.
function Scene:onCreate() end

--- @brief Return to the title scene after a confirm action.
---@param _ number
function Scene:onTick(_) end

function Scene:onDestroy() end

return Scene
