local Engine = require("Engine")
local DataLoading = require("Global.Data.Loading")
local DataTextConfigs = require("Global.Data.TextConfigs")
local DataBlueprints = require("Global.Data.Blueprints")
local Validation = require("Global.Data.Validation")

local requireNamedValue = Validation.RequireNamedValue
local RuntimeProviders = Engine.RuntimeProviders

---@class (partial) Global.Data
local Data = {
    dataKinds = 7
}

---@type Global.Data.Cache
local dataState = {
    animationData = {},
    curveData = {},
    curveTypes = {},
    textConfigData = {},
    plainTextConfigs = {},
    richTextConfigs = {},
    commonFunctionsData = {},
    tilesetData = {},
    autoTileData = {},
    generalData = {},
    blueprintClassPaths = nil,
    blueprintClassPathIndex = nil,
    blueprintClassData = {}
}

---@type Engine.ClassDict | nil
local classDict

---@type Global.Data.Loading
local dataLoading
---@type Global.Data.TextConfigs
local dataTextConfigs
---@type Global.Data.Blueprints
local dataBlueprints

---@param options Global.Data.RuntimeOptions
function Data.InitializeRuntime(options)
    assert(classDict == nil, "Data runtime is already initialized")
    assert(Class.isInstance(options.configResolver, "function"), "Data configResolver must be a function")
    assert(Class.isInstance(options.nodeModuleCandidates, "function"), "Data nodeModuleCandidates must be a function")
    assert(
        Class.isSubclass(options.conditionalActorBase, Engine.Actor),
        "Data conditionalActorBase must derive from Engine.Actor"
    )
    assert(
        Class.isSubclass(options.childActorComponentBase, Engine.Component),
        "Data childActorComponentBase must derive from Engine.Component"
    )
    classDict = Engine.ClassDict.new()
    dataLoading = DataLoading.new(dataState)
    dataTextConfigs = DataTextConfigs.new(dataState)
    dataBlueprints = DataBlueprints.new(dataState, dataLoading, classDict, options)
    RuntimeProviders.installData(function (name)
        return Data.GetCurve(name)
    end,
        function (name)
            return Data.GetPlainTextConfig(name)
        end)
    dataBlueprints:installRuntimeProviders()
end

function Data.BeginInitialLoad()
    return dataLoading:beginInitialLoad()
end

function Data.ApplyInitialLoadItem(stage, item)
    return dataLoading:applyInitialLoadItem(stage, item)
end

function Data.CommitInitialLoad(stage)
    dataLoading:commitInitialLoad(stage, dataBlueprints)
end

function Data.AbortInitialLoad(stage)
    dataLoading:abortInitialLoad(stage)
end

function Data.GetDataKinds()
    return Data.dataKinds
end

function Data.CountLoadableFiles(dataRoot, needExt, _defaultType, recursive)
    return dataLoading:countLoadableFiles(dataRoot, needExt, recursive)
end

function Data.LoadAnimations(onFileLoaded)
    dataLoading:loadAnimations(onFileLoaded)
end

function Data.LoadCommonFunctions(onFileLoaded)
    dataLoading:loadCommonFunctions(onFileLoaded)
end

function Data.LoadTilesets(onFileLoaded)
    dataLoading:loadTilesets(onFileLoaded)
end

function Data.LoadAutoTiles(onFileLoaded)
    dataLoading:loadAutoTiles(onFileLoaded)
end

function Data.LoadGeneralData(onFileLoaded)
    dataLoading:loadGeneralData(onFileLoaded)
end

function Data.LoadCurves(onFileLoaded)
    dataLoading:loadCurves(onFileLoaded)
end

function Data.LoadTextConfigs(onFileLoaded)
    dataLoading:loadTextConfigs(onFileLoaded)
end

function Data.GetAnimation(name)
    local animation = requireNamedValue(dataState.animationData, name, "Animation data not found: " .. tostring(name))
    return copy(animation)
end

function Data.GetCurve(name)
    assert(dataState.curveTypes[name] == "curve", "Float curve data not found: " .. tostring(name))
    return requireNamedValue(dataState.curveData, name, "Float curve data not found: " .. tostring(name))
end

function Data.GetVector2Curve(name)
    assert(dataState.curveTypes[name] == "vector2Curve", "Vector2 curve data not found: " .. tostring(name))
    return requireNamedValue(dataState.curveData, name, "Vector2 curve data not found: " .. tostring(name))
end

function Data.GetVector3Curve(name)
    assert(dataState.curveTypes[name] == "vector3Curve", "Vector3 curve data not found: " .. tostring(name))
    return requireNamedValue(dataState.curveData, name, "Vector3 curve data not found: " .. tostring(name))
end

function Data.GetVector4Curve(name)
    assert(dataState.curveTypes[name] == "vector4Curve", "Vector4 curve data not found: " .. tostring(name))
    return requireNamedValue(dataState.curveData, name, "Vector4 curve data not found: " .. tostring(name))
end

function Data.GetPlainTextConfig(name)
    return dataTextConfigs:getPlainTextConfig(name)
end

function Data.GetRichTextConfig(name)
    return dataTextConfigs:getRichTextConfig(name)
end

function Data.GetTileset(name)
    return requireNamedValue(dataState.tilesetData, name, "Tileset data not found: " .. tostring(name))
end

function Data.GetAutoTile(name)
    return requireNamedValue(dataState.autoTileData, name, "AutoTile data not found: " .. tostring(name))
end

function Data.HasAutoTile(name)
    return dataState.autoTileData[name] ~= nil
end

function Data.GetGeneralData(name)
    return requireNamedValue(dataState.generalData, name, "General data not found: " .. tostring(name))
end

function Data.GetClass(classPath)
    return assert(classDict, "Data runtime is not initialized"):get(classPath)
end

function Data.GetClassData(classPath)
    return assert(classDict, "Data runtime is not initialized"):getData(classPath)
end

function Data.ResolveClassPath(className)
    return dataBlueprints:resolveClassPath(className)
end

function Data.GetCommonFunction(name)
    return dataBlueprints:getCommonFunction(name)
end

function Data.GenGraphFromData(data, parent, parentClass)
    return dataBlueprints:genGraphFromData(data, parent, parentClass)
end

---@param classVarChanges table<string, Global.Data.ClassVarValue> | nil
function Data.GenActorFromClassPath(classPath, tag, classVarChanges)
    return dataBlueprints:genActorFromClassPath(classPath, tag, classVarChanges)
end

function Data.GetActorPreviewData(classPath, classVarChanges)
    return dataBlueprints:getActorPreviewData(classPath, classVarChanges)
end

function Data.ClearActorPreviewCache()
    dataBlueprints:clearActorPreviewCache()
end

function Data.GenActorFromClassName(className, tag)
    return dataBlueprints:genActorFromClassName(className, tag)
end

---@param classVarChanges table<string, Global.Data.ClassVarValue> | nil
function Data.GenActorFromData(actorData, layerName, classVarChanges)
    return dataBlueprints:genActorFromData(actorData, layerName, classVarChanges)
end

return Data
