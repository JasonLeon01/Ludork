---@meta Source.Scenes.SceneGameOver
---@class Source.Scenes.SceneGameOver: GlobalCore.SceneBase
---@field _ui            Source.Scenes.SceneGameOver.Controller
---@field _phase         Source.Scenes.SceneGameOver.Phase
---@field new            fun(): Source.Scenes.SceneGameOver
---@field private _PHASE { OPEN: Source.Scenes.SceneGameOver.Phase, ENTERING: Source.Scenes.SceneGameOver.Phase, EXITING: Source.Scenes.SceneGameOver.Phase }
local Scene = {}

---@enum Source.Scenes.SceneGameOver.Phase
Scene._PHASE = {
    OPEN = "open",
    ENTERING = "entering",
    EXITING = "exiting"
}

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
