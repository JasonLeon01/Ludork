local ActorTree = {}

---@generic T: Engine.Actor
---@param actors T[]
---@return table<T, boolean>
function ActorTree.ToSet(actors)
    local result = {}
    for _, actor in ipairs(actors) do
        result[actor] = true
    end
    return result
end

return ActorTree
