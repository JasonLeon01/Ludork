local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local ConditionalActor = require("Source.ConditionalActor")
local cjson = require("cjson")

local Values = {}

local arrayMetatable = getmetatable(list():toTable())

local identityFields = {
    ID = true,
    tag = true,
    mapTag = true,
    runtimeId = true,
    parent = true,
    children = true,
    classVarChanges = true,
    scriptMixin = true,
    scriptPath = true
}

local dataValueTypes = {
    ["Engine.Material"] = Engine.Material,
    ["Engine.AutoSoundParams"] = Engine.AutoSoundParams,
    ["Engine.SoundFilter"] = Engine.SoundFilter,
    ["Engine.MusicFilter"] = Engine.MusicFilter
}

---@type table<string, boolean>
local standardTypeNames = {
    any = true,
    ["nil"] = true,
    bool = true,
    int = true,
    float = true,
    number = true,
    double = true,
    string = true,
    file = true,
    table = true,
    Pair = true,
    pair = true,
    List = true,
    Array = true,
    Dict = true,
    Dictionary = true,
    Map = true,
    Tuple = true,
    Union = true
}

---@type table<string, boolean>
local sfValueTypeNames = {
    ["sf.IntRect"] = true,
    ["sf.FloatRect"] = true,
    ["sf.Color"] = true,
    ["sf.Angle"] = true,
    ["sf.Vector2i"] = true,
    ["sf.Vector2u"] = true,
    ["sf.Vector2f"] = true,
    ["sf.Vector2b"] = true,
    ["sf.Vector3i"] = true,
    ["sf.Vector3u"] = true,
    ["sf.Vector3f"] = true,
    ["sf.Vector3b"] = true
}

---@param valueType       any
---@param declaringModule string | nil
---@return boolean
local function supportsType(valueType, declaringModule)
    local name = Engine.metadataTypeName(valueType)
    for token in name:gmatch("[%a_][%w_%.]*") do
        local qualified = token:find("%.") and token or (declaringModule or "") .. "." .. token
        if not standardTypeNames[token] and not sfValueTypeNames[token] and dataValueTypes[qualified] == nil then
            return false
        end
    end
    return bool(name)
end

---@param value any
---@param state { visited: table<any, boolean>, remaining: integer, bytes: integer }
---@param depth integer
---@return any, boolean
local function encode(value, state, depth)
    state.remaining = state.remaining - 1
    if state.remaining < 0 or depth > 16 then
        return nil, false
    end
    if value == nil or value == cjson.null then
        return cjson.null, true
    end
    if Class.isInstance(value, "string") then
        state.bytes = state.bytes - #value
        return value, state.bytes >= 0
    end
    if Class.isInstance(value, "boolean") then
        return value, true
    end
    if Class.isInstance(value, "number") then
        return value, math.isFinite(value)
    end
    if Class.isInstance(value, sf.IntRect) or Class.isInstance(value, sf.FloatRect) then
        return encode({ { value.position.x, value.position.y }, { value.size.x, value.size.y } }, state, depth + 1)
    end
    if Class.isInstance(value, sf.Vector2i) or Class.isInstance(value, sf.Vector2u)
        or Class.isInstance(value, sf.Vector2f) or Class.isInstance(value, sf.Vector2b) then
        return encode({ value.x, value.y }, state, depth + 1)
    end
    if Class.isInstance(value, sf.Vector3i) or Class.isInstance(value, sf.Vector3u)
        or Class.isInstance(value, sf.Vector3f) or Class.isInstance(value, sf.Vector3b) then
        return encode({ value.x, value.y, value.z }, state, depth + 1)
    end
    if Class.isInstance(value, sf.Color) then
        return encode({ value.r, value.g, value.b, value.a }, state, depth + 1)
    end
    if Class.isInstance(value, sf.Angle) then
        return encode(value:asDegrees(), state, depth + 1)
    end
    if state.visited[value] then
        return nil, false
    end
    local sequence = Class.isInstance(value, list) or Class.isInstance(value, tuple)
    local dictionary = Class.isInstance(value, dict)
    local plainTable = Class.isInstance(value, "table")
        and (getmetatable(value) == nil or getmetatable(value) == arrayMetatable)
    local dataType = Class.type(value)
    local knownDataType = false
    for _, candidate in pairs(dataValueTypes) do
        if dataType == candidate then
            knownDataType = true
            break
        end
    end
    if not sequence and not dictionary and not plainTable and not knownDataType then
        return nil, false
    end
    state.visited[value] = true
    local result = {}
    local supported = true
    local count = 0
    local maximumIndex = 0
    local hasStringKeys = false
    local hasNumberKeys = false
    local function append(key, item)
        if Class.isInstance(key, "string") then
            state.bytes = state.bytes - #key
            if state.bytes < 0 then
                return false
            end
            hasStringKeys = true
        elseif not dictionary and Class.isInstance(key, "number")
            and math.type(key) == "integer" and key > 0
            and key <= 1024 then
            hasNumberKeys = true
            maximumIndex = math.max(maximumIndex, key)
        else
            return false
        end
        if hasStringKeys and hasNumberKeys then
            return false
        end
        local encoded, valid = encode(item, state, depth + 1)
        if valid then
            result[key] = encoded
            count = count + 1
        end
        return valid
    end
    if sequence then
        setmetatable(result, arrayMetatable)
        for index, item in ipairs(value) do
            if not append(index, item) then
                supported = false
                break
            end
        end
    elseif dictionary then
        for key, item in value:items() do
            if not append(key, item) then
                supported = false
                break
            end
        end
    elseif plainTable then
        local packedLength = rawget(value, "n")
        local packed = Class.isInstance(packedLength, "number") and math.type(packedLength) == "integer"
            and packedLength >= 0 and packedLength <= 1024
        if packed then
            local keys = 0
            for key in pairs(value) do
                keys = keys + 1
                if keys > 1025
                    or (key ~= "n"
                        and (not Class.isInstance(key, "number") or math.type(key) ~= "integer"
                            or key < 1 or key > packedLength)) then
                    packed = false
                    break
                end
            end
        end
        if packed then
            setmetatable(result, arrayMetatable)
            for index = 1, packedLength do
                if not append(index, value[index]) then
                    supported = false
                    break
                end
            end
        else
            if getmetatable(value) == arrayMetatable then
                setmetatable(result, arrayMetatable)
            end
            for key, item in pairs(value) do
                if not append(key, item) then
                    supported = false
                    break
                end
            end
        end
    else
        for name, descriptor in pairs(Engine.getAttrMetadata(dataType)) do
            if descriptor.component or not supportsType(descriptor.type, descriptor.module)
                or not append(name, value[name]) then
                supported = false
                break
            end
        end
    end
    state.visited[value] = nil
    return result, supported and (not hasNumberKeys or maximumIndex == count)
end

---@param name string
---@return string[]
local function typeArguments(name)
    local result = {}
    local start = assert(name:find("[", 1, true)) + 1
    local depth = 0
    for index = start, #name - 1 do
        local character = name:sub(index, index)
        if character == "[" then
            depth = depth + 1
        elseif character == "]" then
            depth = depth - 1
        elseif character == "," and depth == 0 then
            result[#result + 1] = name:sub(start, index - 1):match("^%s*(.-)%s*$")
            start = index + 1
        end
    end
    result[#result + 1] = name:sub(start, #name - 1):match("^%s*(.-)%s*$")
    return result
end

---@param name string
---@return any
local function typeSchema(name)
    local kind = name:match("^(%a+)%[")
    if kind == nil then
        return name
    end
    local arguments = typeArguments(name)
    if kind == "List" or kind == "Dict" or kind == "Optional" then
        return { [kind:lower()] = typeSchema(assert(arguments[kind == "Dict" and 2 or 1])) }
    end
    local entries = {}
    for _, argument in ipairs(arguments) do
        entries[#entries + 1] = typeSchema(argument)
    end
    return { [kind:lower()] = entries }
end

---@param value           any
---@param encoded         any
---@param valueType       any
---@param declaringModule string | nil
---@return any
local function typedSnapshot(value, encoded, valueType, declaringModule)
    if value == cjson.null then
        value = nil
    end
    local name = Engine.metadataTypeName(valueType)
    if name:match("^Optional%[") then
        return typedSnapshot(value, encoded, typeArguments(name)[1], declaringModule)
    end
    if name:match("^Union%[") then
        for _, branch in ipairs(typeArguments(name)) do
            local valid = value == nil and (branch == "nil" or branch == "any")
            if value ~= nil then
                valid = pcall(Engine.resolveRuntimeTypedValue, value, branch, declaringModule)
            end
            if valid then
                return {
                    ["$type"] = typeSchema(branch),
                    ["$value"] = typedSnapshot(value, encoded, branch, declaringModule)
                }
            end
        end
        error("Runtime value does not match its declared union")
    end
    if value == nil then
        return encoded
    end
    if name:match("^List%[") or name:match("^Tuple%[") or name:match("^Dict%[") then
        if not name:match("^Dict%[") then
            setmetatable(encoded, arrayMetatable)
        end
        local arguments = typeArguments(name)
        for key, item in pairs(encoded) do
            local itemType = arguments[1]
            if name:match("^Dict%[") then
                itemType = arguments[2]
            elseif name:match("^Tuple%[") then
                itemType = arguments[key]
            end
            encoded[key] = typedSnapshot(value[key], item, itemType, declaringModule)
        end
    end
    local qualified = name:find("%.") and name or (declaringModule or "") .. "." .. name
    local dataType = dataValueTypes[qualified]
    if dataType ~= nil then
        local descriptors = Engine.getAttrMetadata(dataType)
        for field, item in pairs(encoded) do
            local descriptor = descriptors[field]
            encoded[field] = typedSnapshot(value[field], item, descriptor.type, descriptor.module)
        end
    end
    return encoded
end

---@param value           any
---@param valueType       any
---@param declaringModule string | nil
---@return any, boolean
local function encodeField(value, valueType, declaringModule)
    if not supportsType(valueType, declaringModule) then
        return nil, false
    end
    return encode(value, { visited = {}, remaining = 1024, bytes = 65536 }, 0)
end

---@param value           any
---@param valueType       any
---@param declaringModule string | nil
---@return boolean
local function isLiteralInput(value, valueType, declaringModule)
    if value == nil or value == cjson.null then
        return true
    end
    local name = Engine.metadataTypeName(valueType)
    if name:match("^Optional%[") then
        return isLiteralInput(value, typeArguments(name)[1], declaringModule)
    end
    if Class.isInstance(value, "string") then
        return name == "any" or name == "string" or name == "file"
    end
    if name:match("^Union%[") then
        if not Class.isInstance(value, "table") or value["$type"] == nil then
            return false
        end
        local selected = Engine.metadataTypeName(value["$type"])
        for _, branch in ipairs(typeArguments(name)) do
            if branch == selected then
                return isLiteralInput(value["$value"], branch, declaringModule)
            end
        end
        return false
    end
    if name:match("^List%[") or name:match("^Tuple%[") or name:match("^Dict%[") then
        if not Class.isInstance(value, "table") then
            return false
        end
        local arguments = typeArguments(name)
        for key, item in pairs(value) do
            local itemType = arguments[1]
            if name:match("^Dict%[") then
                itemType = arguments[2]
            elseif name:match("^Tuple%[") then
                itemType = arguments[key]
            end
            if itemType == nil or not isLiteralInput(item, itemType, declaringModule) then
                return false
            end
        end
        return true
    end
    local qualified = name:find("%.") and name or (declaringModule or "") .. "." .. name
    local dataType = dataValueTypes[qualified]
    if dataType ~= nil and Class.isInstance(value, "table") then
        local descriptors = Engine.getAttrMetadata(dataType)
        for field, item in pairs(value) do
            local descriptor = descriptors[field]
            if descriptor == nil or not supportsType(descriptor.type, descriptor.module)
                or not isLiteralInput(item, descriptor.type, descriptor.module) then
                return false
            end
        end
    end
    return true
end

---@param actor Engine.Actor
---@return table<string, any>
local function getDescriptors(actor)
    local actorType = Class.type(actor)
    assert(Class.isInstance(actorType, "table"), "Actor runtime type must be a class")
    ---@cast actorType table
    local components = GlobalFunctions.Components.getComponentTypes(actorType)
    local invalid = {}
    for _, base in ipairs(Class.getMro(actorType)) do
        local metadata = Engine.getClassTypeMetadata(base)
        for _, name in ipairs(metadata ~= nil and metadata.InvalidVars or {}) do
            invalid[name] = true
        end
    end
    local descriptors = {}
    for name, descriptor in pairs(Engine.getAttrMetadata(actorType)) do
        local metadata = descriptor.metadata or {}
        local decorators = metadata.Meta or {}
        if not identityFields[name] and not invalid[name] and not name:match("^_") and not descriptor.component
            and components[name] == nil and not decorators.BlueprintOnly then
            descriptors[name] = descriptor
        end
    end
    return descriptors
end

function Values.Read(actor, actorId)
    local values = {}
    local schema = {}
    for name, descriptor in pairs(getDescriptors(actor)) do
        local encoded, supported = encodeField(actor[name], descriptor.type, descriptor.module)
        if supported then
            supported, encoded = pcall(typedSnapshot, actor[name], encoded, descriptor.type, descriptor.module)
        end
        if supported then
            values[name] = encoded
        else
            values[name] = "<runtime " .. Engine.metadataTypeName(descriptor.type) .. ">"
        end
        local fieldMetadata, metadataSupported = encode(
            descriptor.metadata or {}, { visited = {}, remaining = 1024, bytes = 65536 }, 0
        )
        schema[name] = metadataSupported and fieldMetadata or {}
        schema[name].type = Engine.metadataTypeName(descriptor.type)
        schema[name].module = descriptor.module
        ---@type table<string, any>
        local decorators = schema[name].Meta or {}
        schema[name].readOnly = not supported or not metadataSupported
            or schema[name].readOnly == true or decorators.ReadOnly == true
    end
    return { actorId = actorId, values = values, schema = schema }
end

function Values.Write(actor, name, value)
    assert(Class.isInstance(name, "string"), "Variable name must be a string")
    local descriptor = assert(getDescriptors(actor)[name], "Variable is not editable: " .. name)
    local metadata = descriptor.metadata or {}
    assert(
        metadata.readOnly ~= true and (metadata.Meta == nil or metadata.Meta.ReadOnly ~= true),
        "Variable is read-only: " .. name
    )
    local current, supported = encodeField(actor[name], descriptor.type, descriptor.module)
    if supported then
        supported = pcall(typedSnapshot, actor[name], current, descriptor.type, descriptor.module)
    end
    assert(supported, "Variable cannot be represented as JSON: " .. name)
    if value == cjson.null then
        value = nil
    end
    local input, inputSupported = encodeField(value, descriptor.type, descriptor.module)
    assert(
        inputSupported and isLiteralInput(input, descriptor.type, descriptor.module),
        "Variable input cannot be represented safely: " .. name
    )
    local previous = actor[name]
    local resolved = Engine.resolveTypedDataValue(value, descriptor.type, nil, descriptor.module, false)
    local _, resolvedSupported = encodeField(resolved, descriptor.type, descriptor.module)
    assert(resolvedSupported, "Variable value cannot be represented safely: " .. name)
    Engine.setRuntimeTypedAttribute(actor, name, resolved)
    if name == "texturePath" then
        local texture = bool(actor.texturePath) and GlobalCore.TextureManager.load(actor.texturePath) or nil
        actor:setTexture(texture, true)
        actor:setTextureRect(actor.defaultRect)
    elseif name == "defaultRect" then
        actor:setTextureRect(actor.defaultRect)
    elseif name == "defaultTranslation" then
        actor:setTranslation(actor.defaultTranslation)
    elseif name == "defaultRotation" then
        actor:setRotation(actor.defaultRotation)
    elseif name == "defaultScale" then
        actor:setScale(actor.defaultScale)
    elseif name == "defaultOrigin" then
        actor:setOrigin(actor.defaultOrigin)
    elseif name == "shaderPath" then
        actor:setShaderPath(actor.shaderPath)
    elseif name == "material" then
        actor:setMaterial(actor.material)
    elseif name == "visible" then
        actor:setVisible(actor.visible, false)
    elseif name == "animatable" then
        actor:setAnimatable(actor.animatable, false)
    elseif name == "tickable" then
        actor:setTickable(actor.tickable, false)
    elseif name == "collisionEnabled" then
        actor:setCollisionEnabled(actor.collisionEnabled)
    elseif name == "autoSoundParams" then
        actor:normaliseAutoSoundParams()
    end
    if Class.isInstance(actor, ConditionalActor)
        and (name == "conditionVariable" or name == "conditionOperator" or name == "conditionValue") then
        ---@cast actor Source.ConditionalActor
        local succeeded, failure = pcall(actor.refreshConditionMonitor, actor)
        if not succeeded then
            Engine.setRuntimeTypedAttribute(actor, name, previous)
            actor:refreshConditionMonitor()
            error(failure, 0)
        end
    end
    local gameMap = actor:getMap()
    if gameMap ~= nil then
        ---@cast gameMap GameMap
        gameMap:markPassabilityDirty()
    end
end

return Values
