local cjson = require("cjson")
local Engine = require("Engine")
local FileBatch = require("Global.Utils.FileBatch")
local Logging = require("Global.Utils.Logging")
local GeneralDataSchema = require("Source.Data.GeneralDataSchema")

local ResourceFileConstants = Engine.ResourceFileConstants
local Curve = Engine.Curve
local Vector2Curve = Engine.Vector2Curve
local Vector3Curve = Engine.Vector3Curve
local Vector4Curve = Engine.Vector4Curve

local categoryFields = {
    animations = "animationData",
    commonFunctions = "commonFunctionsData",
    tilesets = "tilesetData",
    autoTiles = "autoTileData",
    general = "generalData",
    curves = "curveData",
    textConfigs = "textConfigData"
}

---@class Source.Data.Loading
local DataLoading = {}

---@param data Source.Data.Cache
function DataLoading:init(data)
    self._state = data
end

---@param fileName string
---@return string, string
local function splitCompound(fileName)
    local name, extension = fileName:match("^(.-)(%..+)$")
    if name == nil then
        return fileName, ""
    end
    return name, extension or ""
end

---@param value Source.Data.JsonValue | userdata
---@param seen  table | nil
---@return Source.Data.JsonValue
function DataLoading:normaliseJsonNull(value, seen)
    if value == cjson.null then
        return nil
    end
    if not Class.isInstance(value, "table") then
        return value
    end
    ---@cast value table
    seen = seen or {}
    if seen[value] ~= nil then
        return seen[value]
    end
    local result = {}
    seen[value] = result
    local arrayValue = true
    for key in pairs(value) do
        if not Class.isInstance(key, "number") or key < 1 or key % 1 ~= 0 then
            arrayValue = false
            break
        end
    end
    if arrayValue then
        for index = 1, #value do
            local item = value[index]
            result[index] = item == cjson.null and cjson.null or self:normaliseJsonNull(item, seen)
        end
        return result
    end
    for key, item in pairs(value) do
        local normalised = self:normaliseJsonNull(item, seen)
        if normalised ~= nil then
            result[self:normaliseJsonNull(key, seen)] = normalised
        end
    end
    return result
end

---@param relativePath string
---@return string
local function animationNameFromRelativePath(relativePath)
    local suffix = ResourceFileConstants.ANIMATION_CACHE_SUFFIX
    assert(string.endsWith(relativePath, suffix), "Invalid compressed animation file name: " .. relativePath)
    local name = relativePath:sub(1, -#suffix - 1)
    assert(bool(name), "Compressed animation name must not be empty")
    return name
end

---@param specs  FileBatchSpec[]
---@param onItem fun(item: FileBatchItem, snapshot: FileBatchSnapshot)
---@return integer
---@diagnostic disable-next-line: unused
function DataLoading:drainFileBatch(specs, onItem)
    assert(bool(asyncio), "asyncio is not initialised")
    local async = asyncio
    local job = async.start_file_batch(specs)
    while true do
        local snapshot = async.poll_file_batch(job, 64)
        if snapshot.state == "failed" then
            error(FileBatch.FormatError(snapshot.error))
        end
        if snapshot.state == "cancelled" then
            error("File batch was cancelled")
        end
        for _, item in ipairs(snapshot.items or {}) do
            onItem(item, snapshot)
        end
        if snapshot.state == "completed" and snapshot.drained then
            return snapshot.total
        end
    end
end

---@param spec         FileBatchSpec
---@param onFileLoaded fun() | nil
function DataLoading:_loadOneCategory(spec, onFileLoaded)
    local stage = self:beginInitialLoad()
    self:drainFileBatch({ spec }, function (item)
        self:applyInitialLoadItem(stage, item)
        if onFileLoaded ~= nil then
            onFileLoaded()
        end
    end)
    local field = assert(categoryFields[spec.category])
    if spec.category == "animations" then
        self._state.animationData = stage.animationData
    elseif spec.category == "curves" then
        self._state.curveData = stage.curveData
        self._state.curveTypes = stage.curveTypes
    else
        self._state[field] = stage[field]
    end
end

---@diagnostic disable-next-line: unused
function DataLoading:beginInitialLoad()
    return {
        animationData = {},
        curveData = {},
        curveTypes = {},
        textConfigData = {},
        commonFunctionsData = {},
        tilesetData = {},
        autoTileData = {},
        generalData = {},
        aborted = false,
        committed = false
    }
end

function DataLoading:applyInitialLoadItem(stage, item)
    assert(not stage.aborted, "Initial data stage is unavailable")
    assert(not stage.committed, "Initial data stage is already committed")
    local relativePath = item.relativePath
    local category = item.category
    Logging.debug("Loading %s: %s", category, relativePath)
    local payload = self:normaliseJsonNull(cjson.decode(item.content))
    ---@cast payload table<string, Source.Data.JsonValue>
    local name
    if category == "animations" then
        assert(payload.type == "compressedAnimation", "Invalid compressed animation type: " .. relativePath)
        assert(
            payload.frameEncoding == "base64+zlib", "Unsupported compressed animation frame encoding: " .. relativePath
        )
        name = animationNameFromRelativePath(relativePath)
        payload.type = nil
        stage.animationData[name] = Engine.AnimationData.new(payload)
    elseif category == "commonFunctions" then
        payload.type = nil
        name = splitCompound(relativePath)
        stage.commonFunctionsData[name] = payload
    elseif category == "tilesets" then
        payload.type = nil
        name = splitCompound(relativePath)
        ---@cast payload table<string, string | boolean[] | table<string, boolean | number>[] | boolean[][]>
        stage.tilesetData[name] = Engine.Tileset.fromData(payload)
    elseif category == "autoTiles" then
        payload.type = nil
        name = splitCompound(relativePath)
        ---@cast payload table<string, string | boolean | table<string, boolean | number>>
        stage.autoTileData[name] = Engine.AutoTile.fromData(payload)
    elseif category == "general" then
        payload.type = nil
        name = splitCompound(relativePath)
        GeneralDataSchema.Canonicalise(payload, relativePath)
        stage.generalData[name] = payload
    elseif category == "curves" then
        local curveType = payload.type
        payload.type = nil
        name = splitCompound(relativePath)
        if curveType == "curve" then
            local curveData = payload
            ---@cast curveData Engine.CurveData
            stage.curveData[name] = Curve.fromData(curveData)
        elseif curveType == "vector2Curve" then
            local curveData = payload
            ---@cast curveData Engine.Vector2CurveData
            stage.curveData[name] = Vector2Curve.fromData(curveData)
        elseif curveType == "vector3Curve" then
            local curveData = payload
            ---@cast curveData Engine.Vector3CurveData
            stage.curveData[name] = Vector3Curve.fromData(curveData)
        elseif curveType == "vector4Curve" then
            local curveData = payload
            ---@cast curveData Engine.Vector4CurveData
            stage.curveData[name] = Vector4Curve.fromData(curveData)
        else
            error("Invalid curve type " .. tostring(curveType) .. ": " .. relativePath)
        end
        stage.curveTypes[name] = curveType
    elseif category == "textConfigs" then
        assert(
            payload.type == "plainTextConfig" or payload.type == "richTextConfig",
            "Invalid text config type: " .. relativePath
        )
        name = splitCompound(relativePath)
        stage.textConfigData[name] = payload
    else
        error("Unknown initial data category: " .. tostring(category))
    end
    return name
end

function DataLoading:commitInitialLoad(stage, blueprints)
    assert(not stage.aborted, "Initial data stage is unavailable")
    assert(not stage.committed, "Initial data stage is already committed")
    self._state.animationData = stage.animationData
    self._state.curveData = stage.curveData
    self._state.curveTypes = stage.curveTypes
    self._state.textConfigData = stage.textConfigData
    self._state.plainTextConfigs = {}
    self._state.richTextConfigs = {}
    self._state.commonFunctionsData = stage.commonFunctionsData
    self._state.tilesetData = stage.tilesetData
    self._state.autoTileData = stage.autoTileData
    self._state.generalData = stage.generalData
    blueprints:clearGraphTemplates()
    stage.committed = true
end

---@diagnostic disable-next-line: unused
function DataLoading:abortInitialLoad(stage)
    if not stage.committed then
        stage.aborted = true
    end
end

function DataLoading:countLoadableFiles(dataRoot, needExt, recursive)
    if needExt ~= nil and not string.endsWith(tostring(needExt), "json") then
        return 0
    end
    local suffix = needExt or ResourceFileConstants.DATA_EXTENSION
    local total = 0
    self:drainFileBatch({
        {
            category = "count",
            root = dataRoot,
            suffix = suffix,
            recursive = recursive == true,
            required = false
        }
    },
        function ()
            total = total + 1
        end)
    return total
end

function DataLoading:loadAnimations(onFileLoaded)
    self:_loadOneCategory({
        category = "animations",
        root = Engine.getAnimationCacheRoot(),
        suffix = ResourceFileConstants.ANIMATION_CACHE_SUFFIX,
        recursive = true,
        required = false
    }, onFileLoaded)
end

function DataLoading:loadCommonFunctions(onFileLoaded)
    self:_loadOneCategory({
        category = "commonFunctions",
        root = "./Data/CommonFunctions",
        suffix = ResourceFileConstants.DATA_EXTENSION,
        recursive = true,
        required = false
    }, onFileLoaded)
end

function DataLoading:loadTilesets(onFileLoaded)
    self:_loadOneCategory({
        category = "tilesets",
        root = "./Data/Tilesets",
        suffix = ResourceFileConstants.DATA_EXTENSION,
        recursive = false,
        required = true
    }, onFileLoaded)
end

function DataLoading:loadAutoTiles(onFileLoaded)
    self:_loadOneCategory({
        category = "autoTiles",
        root = "./Data/AutoTiles",
        suffix = ResourceFileConstants.DATA_EXTENSION,
        recursive = true,
        required = false
    }, onFileLoaded)
end

function DataLoading:loadGeneralData(onFileLoaded)
    self:_loadOneCategory({
        category = "general",
        root = "./Data/General",
        suffix = ResourceFileConstants.DATA_EXTENSION,
        recursive = false,
        required = true
    }, onFileLoaded)
end

function DataLoading:loadCurves(onFileLoaded)
    self:_loadOneCategory({
        category = "curves",
        root = "./Data/Curves",
        suffix = ResourceFileConstants.DATA_EXTENSION,
        recursive = true,
        required = false
    }, onFileLoaded)
end

function DataLoading:loadTextConfigs(onFileLoaded)
    self:_loadOneCategory({
        category = "textConfigs",
        root = "./Data/TextConfigs",
        suffix = ResourceFileConstants.DATA_EXTENSION,
        recursive = true,
        required = true
    }, onFileLoaded)
end

return class(DataLoading)
