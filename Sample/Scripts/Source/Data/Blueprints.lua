local cjson = require("cjson")
local Engine = require("Engine")
local NodeCompiler = require("Global.Utils.NodeCompiler")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Logging = require("Global.Utils.Logging")
---@type Global.Utils.Path.Module
local Path = require("Global.Utils.Path")
local BlueprintActorOverrides = require("Source.Data.BlueprintActorOverrides")

local ManagerFunctions = GlobalFunctions.Manager

local nilGraphParentClass = {}
local nodeCompilerContext = {
    moduleCandidates = function (prefix)
        return { "Source." .. prefix, "Global." .. prefix }
    end,
    roots = {
        {
            name = "Engine",
            value = Engine
        },
        {
            name = "GlobalCore",
            value = GlobalCore
        }
    }
}

local DataBlueprints = {}

function DataBlueprints:init(data, loading)
    self._data = data
    self._loading = loading
    self._graphTemplates = {}
end

function DataBlueprints:clearGraphTemplates()
    self._graphTemplates = {}
end

---@return string[]
function DataBlueprints:_loadBlueprintClassPaths()
    if self._data._blueprintClassPaths ~= nil then
        return self._data._blueprintClassPaths
    end
    local paths = {}
    self._loading:drainFileBatch({
        {
            category = "blueprints",
            root = "./Data/Blueprints",
            suffix = ".json",
            recursive = true,
            required = false
        }
    },
        function (item)
            local relative = Path.NormaliseSeparators(tostring(item.relativePath))
            relative = relative:gsub("%.json$", "")
            local classPath = "Data.Blueprints." .. relative:gsub("/", ".")
            paths[#paths + 1] = classPath
            self._data._blueprintClassData[classPath] = item.content
        end)
    table.sort(paths)
    local index = {}
    local leafMatches = {}
    for _, classPath in ipairs(paths) do
        index[classPath:gsub("%.", "_")] = classPath
        local leaf = classPath:match("([^%.]+)$")
        local matches = leafMatches[leaf]
        if matches == nil then
            matches = {}
            leafMatches[leaf] = matches
        end
        matches[#matches + 1] = classPath
    end
    for leaf, matches in pairs(leafMatches) do
        if #matches == 1 then
            index[leaf] = matches[1]
        end
    end
    self._data._blueprintClassPaths = paths
    self._data._blueprintClassPathIndex = index
    return paths
end

function DataBlueprints:resolveClassPath(className)
    if type(className) ~= "string" then
        return ""
    end
    className = className:match("^%s*(.-)%s*$")
    if not bool(className) then
        return ""
    end
    if self._data._classDict:containsCached(className) then
        return className
    end
    local cachedPath = self._data._classDict:findCachedPathByName(className)
    if cachedPath ~= nil then
        return cachedPath
    end
    self:_loadBlueprintClassPaths()
    local blueprintPath = assert(self._data._blueprintClassPathIndex)[className]
    if blueprintPath ~= nil then
        return blueprintPath
    end
    return className
end

function DataBlueprints:getCommonFunction(name)
    if self._data._commonFunctionsData[name] == nil then
        local path = "./Data/CommonFunctions/" .. tostring(name) .. ".json"
        assert(Engine.jsonExists(path), "Common function not found: " .. tostring(name))
        local loadedData = self._loading:normaliseJsonNull(Engine.getJSONData(path))
        ---@cast loadedData Source.Data.GraphData
        self._data._commonFunctionsData[name] = loadedData
    end
    return self:genGraphFromData(self._data._commonFunctionsData[name])
end

---@param data        Source.Data.GraphData
---@param parentClass Class.ClassType<any> | nil
---@return Engine.Graph
function DataBlueprints:compileGraphTemplate(data, parentClass)
    if self._graphTemplates[data] == nil then
        self._graphTemplates[data] = {}
    end
    local parentKey = parentClass or nilGraphParentClass
    if self._graphTemplates[data][parentKey] ~= nil then
        return self._graphTemplates[data][parentKey]
    end
    local nodes = {}
    local links = {}
    local eventParams = deepcopy(data.eventParams or {})
    local startNodes = deepcopy(data.startNodes or {})
    for key, valueDict in pairs(data.nodeGraph) do
        nodes[key] = {}
        for _, node in ipairs(valueDict.nodes or {}) do
            local nodeData = deepcopy(node)
            nodeData.pos = nil
            local resolvedDefinition = NodeCompiler.Compile(nodeData.nodeFunction, parentClass, nodeCompilerContext)
            assert(
                resolvedDefinition ~= nil,
                "Function " .. tostring(nodeData.nodeFunction) .. " not found while compiling graph"
            )
            nodes[key][#nodes[key] + 1] = Engine.DataNode.new(
                nodeData.nodeFunction, nodeData.params, resolvedDefinition
            )
        end
        links[key] = deepcopy(valueDict.links or {})
        if eventParams[key] == nil and (#nodes[key] > 0 or startNodes[key] ~= nil) then
            local eventDefinition = NodeCompiler.Compile(key, parentClass, nodeCompilerContext)
                or NodeCompiler.Compile("self." .. key, parentClass, nodeCompilerContext)
            local paramNames = eventDefinition ~= nil and eventDefinition.paramNames or nil
            if bool(paramNames) then
                eventParams[key] = deepcopy(paramNames)
            end
        end
    end
    local template = Engine.Graph.new(
        data.parent or "NOT_WRITTEN", parentClass, nil, nodes, links, nil, startNodes, eventParams
    )
    self._graphTemplates[data][parentKey] = template
    return template
end

function DataBlueprints:genGraphFromData(data, parent, parentClass)
    return self:compileGraphTemplate(data, parentClass):instantiate(parent)
end

---@param classVarChanges table<string, Source.Data.ClassVarValue> | nil
function DataBlueprints:genActorFromClassPath(classPath, tag, classVarChanges)
    if not bool(classPath) then
        return nil
    end
    local classModel = self._data.GetClass(classPath)
    if classModel == nil then
        return nil
    end
    local texturePath = classModel.texturePath or ""
    local defaultRect = classModel.defaultRect
    local texture = bool(texturePath) and ManagerFunctions.loadCharacter(texturePath) or nil
    local actor = classModel.GenActor(classModel, texture, defaultRect, tag)
    actor:setMapTag(tag == nil and "" or tostring(tag))
    actor.texturePath = texturePath
    BlueprintActorOverrides.ApplyGeneration(actor)
    local graph = self._data._classDict:instantiateGraph(classPath, actor)
    if graph ~= nil then
        actor:setGraph(graph)
    end
    if classVarChanges ~= nil then
        BlueprintActorOverrides.ApplyChanges(actor, classVarChanges)
        BlueprintActorOverrides.ApplyGeneration(actor)
    end
    return actor
end

function DataBlueprints:genActorFromClassName(className, tag)
    return self:genActorFromClassPath(self:resolveClassPath(className), tag)
end

---@param classVarChanges table<string, Source.Data.ClassVarValue> | nil
function DataBlueprints:genActorFromData(actorData, layerName, classVarChanges)
    local tag = actorData.tag
    local position = actorData.position
    local blueprint = actorData.bp
    if blueprint == nil then
        Logging.warning("Actor %s in layer %s has no bp", tostring(tag), tostring(layerName))
        return nil
    end
    blueprint = self:resolveClassPath(blueprint)
    local actor = self:genActorFromClassPath(blueprint, tag, classVarChanges)
    if actor == nil then
        return nil
    end
    BlueprintActorOverrides.ApplyGeneration(actor)
    actor:setMapPosition(position)
    return actor
end

function DataBlueprints:registerServices()
    Class.registerService("blueprint.classGraphData", function (className)
        local classPath = self:resolveClassPath(className)
        local classData = self._data.GetClassData(classPath)
        return classData ~= nil and classData.graph or nil
    end)

    Class.registerService("blueprint.classDataByPath", function (classPath)
        if self._data._blueprintClassPaths == nil then
            self:_loadBlueprintClassPaths()
        end
        if type(self._data._blueprintClassData[classPath]) == "string" then
            local loadedData = self._loading:normaliseJsonNull(cjson.decode(self._data._blueprintClassData[classPath]))
            ---@cast loadedData table<string, Source.Data.JsonValue>
            self._data._blueprintClassData[classPath] = loadedData
        end
        if self._data._blueprintClassData[classPath] ~= nil then
            return self._data._blueprintClassData[classPath]
        end
        local relative = classPath:match("^Data%.Blueprints%.(.+)$")
        if relative == nil then
            return nil
        end
        local path = "./Data/Blueprints/" .. relative:gsub("%.", "/") .. ".json"
        if not Engine.jsonExists(path) then
            return nil
        end
        local loadedData = self._loading:normaliseJsonNull(Engine.getJSONData(path))
        ---@cast loadedData table<string, Source.Data.JsonValue>
        self._data._blueprintClassData[classPath] = loadedData
        return loadedData
    end)

    Class.registerService("blueprint.invalidateClassData", function (classPath)
        if type(self._data._blueprintClassData[classPath]) == "table"
            and self._data._blueprintClassData[classPath].graph ~= nil then
            self._graphTemplates[self._data._blueprintClassData[classPath].graph] = nil
        end
        self._data._blueprintClassData[classPath] = nil
    end)

    Class.registerService("blueprint.compileGraph", function (graphData, parentClass)
        return self:compileGraphTemplate(graphData, parentClass)
    end)

    Class.registerService("blueprint.instantiateGraphTemplate", function (template, parent)
        return template:instantiate(parent)
    end)

    Class.registerService("blueprint.createGraph", function (graphData, parent, parentClass)
        return self:genGraphFromData(graphData, parent, parentClass)
    end)
end

return class(DataBlueprints)
