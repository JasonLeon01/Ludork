local cjson = require("cjson")
local Engine = require("Engine")
local NodeCompiler = require("Global.Utils.NodeCompiler")
local GlobalCore = require("GlobalCore")
local Logging = require("Global.Utils.Logging")
---@type Global.Utils.Path.Module
local Path = require("Global.Utils.Path")
local BlueprintActorOverrides = require("Source.Data.BlueprintActorOverrides")

local ResourceFileConstants = Engine.ResourceFileConstants
local RuntimeProviders = Engine.RuntimeProviders
local TextureManager = GlobalCore.TextureManager

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

---@class Source.Data.Blueprints
local DataBlueprints = {}

---@param data      Source.Data.Cache
---@param classDict Engine.ClassDict
function DataBlueprints:init(data, loading, classDict)
    self._state = data
    self._classDict = classDict
    self._loading = loading
    self._graphTemplates = {}
end

function DataBlueprints:clearGraphTemplates()
    self._graphTemplates = {}
end

---@return string[]
function DataBlueprints:_loadBlueprintClassPaths()
    if self._state.blueprintClassPaths ~= nil then
        return self._state.blueprintClassPaths
    end
    local paths = {}
    self._loading:drainFileBatch({
        {
            category = "blueprints",
            root = "./Data/Blueprints",
            suffix = ResourceFileConstants.DATA_EXTENSION,
            recursive = true,
            required = false
        }
    },
        function (item)
            local relative = Path.NormaliseSeparators(tostring(item.relativePath))
            relative = relative:gsub("%.json$", "")
            local classPath = "Data.Blueprints." .. relative:gsub("/", ".")
            paths[#paths + 1] = classPath
            self._state.blueprintClassData[classPath] = item.content
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
    self._state.blueprintClassPaths = paths
    self._state.blueprintClassPathIndex = index
    return paths
end

function DataBlueprints:resolveClassPath(className)
    if not Class.isInstance(className, "string") then
        return ""
    end
    className = className:match("^%s*(.-)%s*$")
    if not bool(className) then
        return ""
    end
    if self._classDict:containsCached(className) then
        return className
    end
    local cachedPath = self._classDict:findCachedPathByName(className)
    if cachedPath ~= nil then
        return cachedPath
    end
    self:_loadBlueprintClassPaths()
    local blueprintPath = assert(self._state.blueprintClassPathIndex)[className]
    if blueprintPath ~= nil then
        return blueprintPath
    end
    return className
end

function DataBlueprints:getCommonFunction(name)
    if self._state.commonFunctionsData[name] == nil then
        local path = "./Data/CommonFunctions/" .. tostring(name) .. ResourceFileConstants.DATA_EXTENSION
        assert(Engine.jsonExists(path), "Common function not found: " .. tostring(name))
        local loadedData = self._loading:normaliseJsonNull(Engine.getJSONData(path))
        ---@cast loadedData Source.Data.GraphData
        self._state.commonFunctionsData[name] = loadedData
    end
    return self:genGraphFromData(self._state.commonFunctionsData[name])
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
    local classModel = self._classDict:get(classPath)
    if classModel == nil then
        return nil
    end
    local texturePath = classModel.texturePath or ""
    local defaultRect = classModel.defaultRect
    local texture = bool(texturePath) and TextureManager.load(texturePath) or nil
    local actor = classModel.GenActor(classModel, texture, defaultRect, tag)
    actor:setMapTag(tag == nil and "" or tostring(tag))
    actor.texturePath = texturePath
    BlueprintActorOverrides.ApplyGeneration(actor)
    local graph = self._classDict:instantiateGraph(classPath, actor)
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
    actor:setMapPosition(position)
    return actor
end

function DataBlueprints:resolveBlueprintData(classPath)
    if self._state.blueprintClassPaths == nil then
        self:_loadBlueprintClassPaths()
    end
    if Class.isInstance(self._state.blueprintClassData[classPath], "string") then
        local loadedData = self._loading:normaliseJsonNull(
            cjson.decode(tostring(self._state.blueprintClassData[classPath]))
        )
        ---@cast loadedData table<string, Source.Data.JsonValue>
        self._state.blueprintClassData[classPath] = loadedData
    end
    if self._state.blueprintClassData[classPath] ~= nil then
        return self._state.blueprintClassData[classPath]
    end
    local relative = classPath:match("^Data%.Blueprints%.(.+)$")
    if relative == nil then
        return nil
    end
    local path = "./Data/Blueprints/" .. relative:gsub("%.", "/") .. ResourceFileConstants.DATA_EXTENSION
    if not Engine.jsonExists(path) then
        return nil
    end
    local loadedData = self._loading:normaliseJsonNull(Engine.getJSONData(path))
    ---@cast loadedData table<string, Source.Data.JsonValue>
    self._state.blueprintClassData[classPath] = loadedData
    return loadedData
end

function DataBlueprints:installRuntimeProviders()
    RuntimeProviders.installBlueprint(function (classPath)
        return self:resolveBlueprintData(classPath)
    end,
        function (graphData, parentClass)
            return self:compileGraphTemplate(graphData, parentClass)
        end, function (template, parent)
            return template:instantiate(parent)
        end)
end

return class(DataBlueprints)
