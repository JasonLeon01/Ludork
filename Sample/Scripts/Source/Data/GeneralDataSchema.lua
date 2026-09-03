local Engine = require("Engine")

local scalarTypes = {
    any = true,
    bool = true,
    file = true,
    float = true,
    int = true,
    string = true,
    ["sf.Color"] = true,
    ["sf.IntRect"] = true,
    ["sf.Vector2f"] = true,
    ["sf.Vector2i"] = true,
    ["sf.Vector2u"] = true,
    ["sf.Vector3f"] = true,
    ["sf.Vector3i"] = true,
    ["sf.Vector3u"] = true
}
local valueTypes = deepcopy(scalarTypes)
valueTypes.list = true
valueTypes.dict = true

local GeneralDataSchema = {}

local function generalDataError(relativePath, context, message)
    error(string.format("Invalid General Data %s in %s: %s", context, relativePath, message))
end

local function isArray(value)
    if not Class.isInstance(value, "table") then
        return false
    end
    local count = 0
    local maximum = 0
    for key in pairs(value) do
        if not Class.isInstance(key, "number") or math.type(key) ~= "integer" or key < 1 then
            return false
        end
        ---@cast key integer
        count = count + 1
        maximum = math.max(maximum, key)
    end
    return count == maximum
end

local function isDictionary(value)
    if not Class.isInstance(value, "table") then
        return false
    end
    for key in pairs(value) do
        if not Class.isInstance(key, "string") then
            return false
        end
    end
    return true
end

local function canonicaliseScalar(value, typeName, relativePath, context)
    if typeName == "any" then
        return value
    end
    if typeName == "string" or typeName == "file" then
        if not Class.isInstance(value, "string") then
            generalDataError(relativePath, context, "expected " .. typeName)
        end
        return value
    end
    if typeName == "bool" then
        if not Class.isInstance(value, "boolean") then
            generalDataError(relativePath, context, "expected bool")
        end
        return value
    end
    if typeName == "int" then
        if not Class.isInstance(value, "number") or math.type(value) ~= "integer" then
            generalDataError(relativePath, context, "expected int")
        end
        return value
    end
    if typeName == "float" then
        if not Class.isInstance(value, "number") then
            generalDataError(relativePath, context, "expected float")
        end
        return value + 0.0
    end
    if rawget(scalarTypes, typeName) ~= true then
        generalDataError(relativePath, context, "unsupported type " .. tostring(typeName))
    end
    if not isArray(value) then
        generalDataError(relativePath, context, "expected JSON array for " .. typeName)
    end
    local result = Engine.resolveTypedDataValue(value, typeName)
    local sfType = sf[typeName:sub(4)]
    if sfType == nil then
        generalDataError(relativePath, context, "could not construct " .. typeName)
    end
    ---@cast sfType table
    if not Class.isInstance(result, sfType) then
        generalDataError(relativePath, context, "could not construct " .. typeName)
    end
    return result
end

local function canonicaliseValue(value, typeName, param, relativePath, context)
    if typeName == "list" then
        local itemType = param.itemType
        if not Class.isInstance(itemType, "string") or not bool(itemType) then
            generalDataError(relativePath, context, "list requires itemType")
        end
        if rawget(scalarTypes, itemType) ~= true then
            generalDataError(relativePath, context, "unsupported itemType " .. tostring(itemType))
        end
        if not isArray(value) then
            generalDataError(relativePath, context, "expected JSON array")
        end
        local result = {}
        for index, item in ipairs(value) do
            result[index] = canonicaliseScalar(item, itemType, relativePath, context .. "[" .. tostring(index) .. "]")
        end
        return result
    end
    if typeName == "dict" then
        local valueType = param.valueType
        if not Class.isInstance(valueType, "string") or not bool(valueType) then
            generalDataError(relativePath, context, "dict requires valueType")
        end
        if rawget(scalarTypes, valueType) ~= true then
            generalDataError(relativePath, context, "unsupported valueType " .. tostring(valueType))
        end
        if not isDictionary(value) then
            generalDataError(relativePath, context, "expected JSON object with string keys")
        end
        local result = {}
        for key, item in pairs(value) do
            result[key] = canonicaliseScalar(item, valueType, relativePath, context .. "." .. key)
        end
        return result
    end
    return canonicaliseScalar(value, typeName, relativePath, context)
end

local function validateGraph(member, memberName, declaredEvents, relativePath)
    local graph = member._graph
    if graph == nil then
        return
    end
    if not isDictionary(graph) then
        generalDataError(relativePath, "member " .. memberName .. "._graph", "must be a JSON object")
    end
    local nodeGraph = graph.nodeGraph
    local startNodes = graph.startNodes
    if not isDictionary(nodeGraph) then
        generalDataError(relativePath, "member " .. memberName .. "._graph", "nodeGraph must be a JSON object")
    end
    if not isDictionary(startNodes) then
        generalDataError(relativePath, "member " .. memberName .. "._graph", "startNodes must be a JSON object")
    end
    for eventName, eventGraph in pairs(nodeGraph) do
        if declaredEvents[eventName] ~= true then
            generalDataError(
                relativePath, "member " .. memberName .. "._graph", "undeclared graph event " .. tostring(eventName)
            )
        end
        if not isDictionary(eventGraph) or not isArray(eventGraph.nodes) or not isArray(eventGraph.links) then
            generalDataError(
                relativePath, "member " .. memberName .. "._graph." .. eventName, "nodes and links must be arrays"
            )
        end
    end
    for eventName in pairs(startNodes) do
        if declaredEvents[eventName] ~= true then
            generalDataError(
                relativePath, "member " .. memberName .. "._graph", "undeclared start event " .. tostring(eventName)
            )
        end
        if nodeGraph[eventName] == nil then
            generalDataError(
                relativePath, "member " .. memberName .. "._graph", "start event has no matching graph " .. eventName
            )
        end
    end
end

function GeneralDataSchema.Canonicalise(payload, relativePath)
    if payload.linkedType ~= nil then
        generalDataError(relativePath, "schema", "linkedType is not supported")
    end
    local params = payload.params
    if not isDictionary(params) then
        generalDataError(relativePath, "schema", "params must be a JSON object")
    end
    local members = payload.members
    if not isDictionary(members) then
        generalDataError(relativePath, "schema", "members must be a JSON object")
    end
    local declaredEvents = {}
    if payload.events ~= nil then
        if not isArray(payload.events) then
            generalDataError(relativePath, "schema", "events must be a JSON array")
        end
        if #payload.events == 0 then
            generalDataError(relativePath, "schema", "events must not be empty")
        end
        for index, eventName in ipairs(payload.events) do
            if not Class.isInstance(eventName, "string") or not bool(eventName) then
                generalDataError(relativePath, "schema", "events[" .. tostring(index) .. "] must be a non-empty string")
            end
            if declaredEvents[eventName] then
                generalDataError(relativePath, "schema", "duplicate event " .. eventName)
            end
            declaredEvents[eventName] = true
        end
    end
    for fieldName, param in pairs(params) do
        if not Class.isInstance(param, "table") then
            generalDataError(relativePath, "parameter " .. fieldName, "definition must be a JSON object")
        end
        local typeName = param.type
        if not Class.isInstance(typeName, "string") or not valueTypes[typeName] then
            generalDataError(relativePath, "parameter " .. fieldName, "unsupported type " .. tostring(typeName))
        end
        if rawget(param, "defaultValue") == nil then
            generalDataError(relativePath, "parameter " .. fieldName, "defaultValue is required")
        end
        canonicaliseValue(
            param.defaultValue, typeName, param, relativePath, "parameter " .. fieldName .. ".defaultValue"
        )
    end
    for memberName, member in pairs(members) do
        if not isDictionary(member) then
            generalDataError(relativePath, "member " .. memberName, "must be a JSON object")
        end
        for fieldName in pairs(member) do
            if fieldName:sub(1, 1) ~= "_" and params[fieldName] == nil then
                generalDataError(relativePath, "member " .. memberName, "unknown field " .. fieldName)
            end
        end
        for fieldName, param in pairs(params) do
            local value = rawget(member, fieldName)
            if value == nil then
                generalDataError(relativePath, "member " .. memberName, "missing field " .. fieldName)
            end
            member[fieldName] = canonicaliseValue(
                value, param.type, param, relativePath, "member " .. memberName .. "." .. fieldName
            )
        end
        validateGraph(member, memberName, declaredEvents, relativePath)
    end
end

return GeneralDataSchema
