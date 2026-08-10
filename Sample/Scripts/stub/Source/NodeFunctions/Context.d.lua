---@meta Source.NodeFunctions.Context

local Context = {}

---@class Source.NodeFunctions.Context.RefLocal
---@field __graph__ Engine.Graph | nil
---@field __key__ string | nil

---@param fn function
---@return Source.NodeFunctions.Context.RefLocal
function Context._getRefLocal(fn) end

---@param fn function
---@return any
function Context._requireGraphParent(fn) end

---@param fn function
---@return any
function Context._getGraphOwner(fn) end

---@return Source.Scenes.SceneMap.SceneMap
function Context.requireSceneMap() end

---@return Source.GameInstance.GameInstance
function Context.requireGameInstance() end

return Context
