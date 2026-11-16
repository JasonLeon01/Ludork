---@meta Source.Scenes.SceneInit
---@brief Initial loading scene that bootstraps game data.
---@class Source.Scenes.SceneInit.SceneInit: GlobalCore.SceneBase
---@field _ui                                          Source.Scenes.SceneInit.Controller
---@field _bg                                          Engine.Image
---@field phaseKey                                     string
---@field _displayPhaseKey                             string
---@field progressValue                                number
---@field _displayProgress                             number
---@field progressTotal                                integer
---@field processedCount                               integer
---@field progressDone                                 boolean
---@field _loadCancelled                               boolean
---@field _loadStage                                   Global.Data.InitialLoadStage | nil
---@field _activeBatch                                 FileBatchJob | nil
---@field _cacheRequest                                Engine.MaterialCacheRequest | Engine.ShaderCacheRequest | nil
---@field _animationSourceKeys                         table<string, boolean>
---@field _loadTask                                    AsyncioTask | nil
---@field new                                          fun(): Source.Scenes.SceneInit.SceneInit
---@field private _DISPLAY_PROGRESS_COMPLETE_THRESHOLD number
local Scene = {}

---@brief Show initialization directly, before transition shaders are prepared.
function Scene:onEnter() end

---@brief Create progress bar UI and start the required loading and title scene task.
function Scene:onCreate() end

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

---@brief Prepare material and shader programs and animation caches, then load game data.
function Scene:prepareAssets() end

function Scene:onDestroy() end

---@brief Compress animation data files if source or referenced images are newer than cached copies.
function Scene:compressAnimations() end

---@brief Prepare the native material cache and publish its progress.
function Scene:prepareMaterials() end

---@brief Translate and prepare the native shader cache after material generation.
function Scene:prepareShaders() end

---@param request Engine.MaterialCacheRequest | Engine.ShaderCacheRequest
---@param offset  number
function Scene:_prepareCache(request, offset) end

return Scene
