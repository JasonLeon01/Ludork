---@meta Global.Tutorial

---@class Global.Tutorial.Handler
---@field show fun(key: string): Engine.AsyncOperation

local Tutorial = {}

--- Register a scene-owned tutorial callback without importing project business modules.
--- The returned function unregisters this registration only.
---@param scene GlobalCore.SceneBase
---@param show  fun(key: string): Engine.AsyncOperation
---@return fun()
function Tutorial.Register(scene, show) end

--- Request a tutorial from the active scene. The operation completes after confirmation and is cancelled when the scene leaves.
---@param key string
---@return Engine.AsyncOperation
function Tutorial.Show(key) end

return Tutorial
