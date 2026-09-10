---@meta Global.ActorTree

---@class Global.ActorTree.Module
local ActorTree = {}

---@generic T: Engine.Actor
---@param actors T[]
---@return table<T, boolean>
function ActorTree.ToSet(actors) end

return ActorTree
