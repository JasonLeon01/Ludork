local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local StateInfo = require("Source.Infos.StateInfo")

local Node = Engine.Node

local Context = {}

---@param fn function
---@return Source.NodeFunctions.Context.RefLocal
function Context.GetRefLocal(fn)
    return Node.getRefLocal(fn) or {}
end

---@param fn function
---@return unknown
function Context.RequireGraphParent(fn)
    local graph = Context.GetRefLocal(fn).__graph__
    assert(graph ~= nil, "Node function requires a blueprint graph context")
    return graph.parent
end

---@param fn function
---@return unknown
function Context.GetGraphOwner(fn)
    local graph = Context.GetRefLocal(fn).__graph__
    if graph == nil or graph.parent == nil then
        return nil
    end
    local parent = graph.parent
    if Class.isInstance(parent, StateInfo) then
        ---@cast parent Source.Infos.StateInfo
        return parent:getOwner()
    end
    return parent
end

---@return Source.Scenes.SceneMap.SceneMap
function Context.RequireSceneMap()
    local scene = GlobalCore.System.requireScene()
    ---@cast scene Source.Scenes.SceneMap.SceneMap
    return scene
end

---@return Source.GameInstance.GameInstance
function Context.RequireGameInstance()
    local instance = Context.RequireSceneMap().inst
    assert(instance ~= nil, "Node functions require an active game instance")
    return instance
end

return Context
