local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local StateInfo = require("Source.Infos.StateInfo")

local Node = Engine.Node

local Context = {}

---@param fn function
---@return Source.NodeFunctions.Context.RefLocal
function Context._getRefLocal(fn)
    return Node.getRefLocal(fn) or {}
end

---@param fn function
---@return any
function Context._requireGraphParent(fn)
    local graph = Context._getRefLocal(fn).__graph__
    assert(graph ~= nil, "Node function requires a blueprint graph context")
    return graph.parent
end

---@param fn function
---@return any
function Context._getGraphOwner(fn)
    local graph = Context._getRefLocal(fn).__graph__
    if graph == nil or graph.parent == nil then
        return nil
    end
    local parent = graph.parent
    if Class.isInstance(parent, StateInfo) then
        ---@cast parent StateInfo
        return parent:getOwner()
    end
    return parent
end

---@return Source.Scenes.SceneMap.SceneMap
function Context.requireSceneMap()
    local scene = GlobalCore.System.requireScene()
    ---@cast scene Source.Scenes.SceneMap.SceneMap
    return scene
end

---@return Source.GameInstance.GameInstance
function Context.requireGameInstance()
    local instance = Context.requireSceneMap().inst
    assert(instance ~= nil, "Node functions require an active game instance")
    return instance
end

return Context
