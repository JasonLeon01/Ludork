---@meta Source.NodeFunctions.Context

---@class Source.NodeFunctions.Context.RefLocal
---@field __graph__ Engine.Graph | nil
---@field __key__   string | nil

---@param fn function
---@return Source.NodeFunctions.Context.RefLocal
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
