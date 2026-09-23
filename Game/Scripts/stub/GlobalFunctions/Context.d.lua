---@meta GlobalFunctions.Context

---@class GlobalFunctions.Context.RefLocal
---@field __graph__ Engine.Graph | nil
---@field __key__   string | nil

local Context = {}

---@param fn function
---@return GlobalFunctions.Context.RefLocal
function Context.GetRefLocal(fn) end

---@param fn function
---@return unknown
function Context.RequireGraphParent(fn) end

---@param fn function
---@return unknown
function Context.GetGraphOwner(fn) end

---@return Source.Scenes.SceneMap.SceneMap
function Context.RequireSceneMap() end

---@return Source.GameInstance.GameInstance
function Context.RequireGameInstance() end

return Context
