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
    if type(value) ~= "table" then
        return false
    end
    local count = 0
    local maximum = 0
    for key in pairs(value) do
        if type(key) ~= "number" or math.type(key) ~= "integer" or key < 1 then
            return false
        end
        ---@cast key integer
        count = count + 1
        maximum = math.max(maximum, key)
    end
    return count == maximum
end

local function isDictionary(value)
    if type(value) ~= "table" then
        return false
    end
    for key in pairs(value) do
        if type(key) ~= "string" then
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
        if type(value) ~= "string" then
            generalDataError(relativePath, context, "expected " .. typeName)
        end
        return value
    end
    if typeName == "bool" then
        if type(value) ~= "boolean" then
            generalDataError(relativePath, context, "expected bool")
        end
        return value
    end
    if typeName == "int" then
        if type(value) ~= "number" or math.type(value) ~= "integer" then
            generalDataError(relativePath, context, "expected int")
        end
        return value
    end
    if typeName == "float" then
        if type(value) ~= "number" then
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
        if type(itemType) ~= "string" or not bool(itemType) then
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
        if type(valueType) ~= "string" or not bool(valueType) then
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

function GeneralDataSchema.Canonicalise(payload, relativePath)
    local params = payload.params
    if not isDictionary(params) then
        generalDataError(relativePath, "schema", "params must be a JSON object")
    end
    local members = payload.members
    if not isDictionary(members) then
        generalDataError(relativePath, "schema", "members must be a JSON object")
    end
    for fieldName, param in pairs(params) do
        if type(param) ~= "table" then
            generalDataError(relativePath, "parameter " .. fieldName, "definition must be a JSON object")
        end
        local typeName = param.type
        if type(typeName) ~= "string" or not valueTypes[typeName] then
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
    end
end

return GeneralDataSchema
