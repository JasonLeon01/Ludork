---@meta Source.Scenes.SceneInit
---@class Source.Scenes.SceneInit.FrameAsset
---@field name string
---@field path string

---@brief Initial loading scene that bootstraps game data.
---@class Source.Scenes.SceneInit.SceneInit: GlobalCore.SceneBase
---@field _ui                  Source.UI.Init.SceneInitUI
---@field _bg                  Engine.Image
---@field progressValue        number
---@field _displayProgress     number
---@field progressTotal        integer
---@field processedCount       integer
---@field progressDone         boolean
---@field hasSwitched          boolean
---@field _loadCancelled       boolean
---@field _loadStage           Source.Data.InitialLoadStage | nil
---@field _activeBatch         userdata | nil
---@field _animationSourceKeys table<string, boolean>
---@field _loadTask            table | nil
---@field new                  fun(): Source.Scenes.SceneInit.SceneInit
local Scene = {}

---@brief Create progress bar UI and start asset preparation thread.
function Scene:onCreate() end

---@brief Transition after the displayed progress completes.
---
--- - @param deltaTime Elapsed time in seconds.
---@param _ number
function Scene:onTick(_) end

---@brief Publish pending progress on the render thread.
---
--- - @param deltaTime Elapsed time in seconds.
---@param _ number
function Scene:onLateTick(_) end

function Scene:onQuit() end

---@brief Split a compound filename into name and extension.
---
--- - @param fileName The compound filename.
---
--- - @return A tuple of (name, extension).
---@brief Load all independent game data phases and update the progress bar.
function Scene:loadGameData() end

---@brief Background thread entry point: compress animations then load all data.
function Scene:prepareAssets() end

function Scene:onDestroy() end

---@brief Compress animation data files if source or referenced images are newer than cached copies.
function Scene:compressAnimations() end

return Scene
