---@meta Global.ActorTree

---@class Global.ActorTree.Module
local ActorTree = {}

---@param root Engine.Actor
---@return Engine.Actor[]
function ActorTree.Collect(root) end

---@generic T: Engine.Actor
---@param actors T[]
---@return table<T, boolean>
function ActorTree.ToSet(actors) end

return ActorTree
