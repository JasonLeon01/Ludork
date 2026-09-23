local Engine
local function loadEngine()
    if Engine == nil then
        Engine = require("Engine")
    end
    return Engine
end

local GlobalCore
local function loadGlobalCore()
    if GlobalCore == nil then
        GlobalCore = require("GlobalCore")
    end
    return GlobalCore
end

local Context = {}

---@param fn function
---@return GlobalFunctions.Context.RefLocal
function Context.GetRefLocal(fn)
    return loadEngine().Node.getRefLocal(fn) or {}
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
    return graph.parent
end

---@return Source.Scenes.SceneMap.SceneMap
function Context.RequireSceneMap()
    local scene = loadGlobalCore().SceneManager.requireScene()
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
