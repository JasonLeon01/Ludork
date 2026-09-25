---@meta Global.Tutorial

---@class Global.Tutorial.Handler
---@field show fun(key: string): fun(): boolean

local Tutorial = {}

--- Register a scene-owned tutorial callback without importing project business modules.
--- The returned function unregisters this registration only.
---@param scene GlobalCore.SceneBase
---@param show  fun(key: string): fun(): boolean
---@return fun()
function Tutorial.Register(scene, show) end

--- Request a tutorial from the active scene. The condition completes after confirmation or cancellation.
---@param key string
---@return fun(): boolean
function Tutorial.Show(key) end

return Tutorial
