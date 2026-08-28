local ActorTree = {}

---@param root Engine.Actor
---@return Engine.Actor[]
function ActorTree.Collect(root)
    local actors = { root }
    local index = 1
    while index <= #actors do
        local actor = actors[index]
        ---@cast actor Engine.Actor
        for _, child in ipairs(actor:getChildren()) do
            actors[#actors + 1] = child
        end
        index = index + 1
    end
    return actors
end

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
