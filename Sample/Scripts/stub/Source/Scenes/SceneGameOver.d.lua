---@meta Source.Scenes.SceneGameOver
---@class Source.Scenes.SceneGameOver: GlobalCore.SceneBase
---@field _ui    Source.UI.GameOver
---@field _phase string
---@field new    fun(): Source.Scenes.SceneGameOver
local Scene = {}

---@brief Disable the scene mask so the UI Timeline owns the Game Over transition.
function Scene:onEnter() end

---@brief Create the black background and centred game over text.
function Scene:onCreate() end

---@brief Return to the title scene after a confirm action.
---@param _ number
function Scene:onTick(_) end

function Scene:onDestroy() end

function Scene:_backToTitle() end

return Scene
