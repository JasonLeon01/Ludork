local Engine = require("Engine")

local NodeCompiler = {}

---@param path string
---@return string[]
local function splitPath(path)
    local parts = {}
    for part in tostring(path):gmatch("[^.]+") do
        parts[#parts + 1] = part:match("^%s*(.-)%s*$")
    end
    return parts
end

---@param moduleName string
---@return boolean
local function moduleExists(moduleName)
    if package.loaded[moduleName] ~= nil then
        return true
    end
    if package.searchpath(moduleName, package.path) ~= nil then
        return true
    end
    return package.cpath ~= nil and package.searchpath(moduleName, package.cpath) ~= nil
end

---@param moduleName string
---@return table | nil
local function loadModule(moduleName)
    if not moduleExists(moduleName) then
        return nil
    end
    local module = require(moduleName)

    return type(module) == "table" and module or nil
end

---@param moduleName string
---@param module     table
---@return table | nil
local function moduleMetadata(moduleName, module)
    if type(module) == "table" and type(module.__runtimeMetadata) == "table" then
        return module.__runtimeMetadata
    end
    local metadataName = moduleName .. "_meta"
    if not moduleExists(metadataName) then
        return nil
    end
    local metadata = require(metadataName)

    return type(metadata) == "table" and metadata or nil
end

---@param value      any
---@param parts      string[]
---@param startIndex integer
---@return any
local function traverse(value, parts, startIndex)
    local current = value
    for index = startIndex, #parts do
        if current == nil then
            return nil
        end
        current = current[parts[index]]
    end
    return current
end

---@param value any
---@return boolean
local function isCallable(value)
    if type(value) == "function" then
        return true
    end
    if type(value) ~= "table" and type(value) ~= "userdata" then
        return false
    end
    local valueMeta = getmetatable(value)
    return valueMeta ~= nil and type(valueMeta.__call) == "function"
end

---@param value any
---@return boolean
local function isNodeMemberDescriptor(value)
    if type(value) ~= "table" then
        return false
    end
    for _, name in ipairs({
        "parameters",
        "return",
        "ExecSplit",
        "Latent",
        "LatentStates",
        "Loop",
        "LoopNode",
        "Pure"
    }) do
        if value[name] ~= nil then
            return true
        end
    end
    return value.type == "function" or value.type == "event"
end

---@param metadata   table | nil
---@param typeName   string
---@param memberName string
---@return table | nil
local function findMemberMetadata(metadata, typeName, memberName)
    if type(metadata) ~= "table" then
        return nil
    end
    if isNodeMemberDescriptor(metadata[memberName]) then
        return metadata[memberName]
    end
    local typeMetadata = metadata[typeName]
    if type(typeMetadata) == "table" and type(typeMetadata[memberName]) == "table" then
        return typeMetadata[memberName]
    end
    for _, candidate in pairs(metadata) do
        if type(candidate) == "table" and isNodeMemberDescriptor(candidate[memberName]) then
            return candidate[memberName]
        end
    end
    return nil
end

---@param values table | nil
---@param label  string
---@return NodeCompiler.Parameter[]
local function orderedParameters(values, label)
    local result = {}
    local included = {}
    if not bool(values) then
        return result
    end
    ---@cast values -nil
    for index, name in ipairs(values) do
        if type(name) ~= "string" or values[name] == nil then
            error(label .. " order contains an unknown key")
        end
        if included[name] then
            error(label .. " order contains duplicate key '" .. name .. "'")
        end
        included[name] = true
        result[index] = { name = name, type = values[name] }
    end
    return result
end

---@param values table
---@param count  integer
---@return table
local function packedValues(values, count)
    local result = { n = count }
    for index = 1, count do
        result[index] = values[index]
    end
    return result
end

---@param values table | nil
---@param label  string
---@return NodeCompiler.OrderedEntry[]
local function orderedEntries(values, label)
    local result = {}
    local included = {}
    if not bool(values) then
        return result
    end
    ---@cast values -nil
    for index, name in ipairs(values) do
        if type(name) ~= "string" then
            error(label .. " order contains an unknown key")
        end
        if included[name] then
            error(label .. " order contains duplicate key '" .. name .. "'")
        end
        included[name] = true
        local rawValue = values[name]
        local entryValues = {}
        local count = 1
        if type(rawValue) == "table" then
            count = rawValue.n or #rawValue
            for valueIndex = 1, count do
                entryValues[valueIndex] = rawValue[valueIndex]
            end
        else
            entryValues[1] = rawValue
        end
        result[index] = { name = name, values = packedValues(entryValues, count) }
    end
    return result
end

---@param metadata table | nil
---@return NodeCompiler.MemberMetadata
local function normaliseMemberMetadata(metadata)
    if type(metadata) ~= "table" then
        return {
            parameters = {},
            parameterTypes = {},
            defaults = {},
            returns = {},
            execSplit = {},
            latentStates = {},
            latent = false,
            loop = false,
            pure = false,
            loopNode = "",
            kind = "",
            needsRefLocal = false
        }
    end
    local parameters = orderedParameters(metadata.parameters, "parameters")
    local parameterTypes = {}
    for _, parameter in ipairs(parameters) do
        parameterTypes[parameter.name] = parameter.type
    end
    local defaults = {}
    if type(metadata.default) == "table" then
        defaults = packedValues(metadata.default, math.max(metadata.default.n or #metadata.default, #parameters))
    end
    local latentStates = metadata.LatentStates
    if type(latentStates) ~= "table" and type(metadata.Latent) == "table" then
        latentStates = metadata.Latent
    end
    local latent = metadata.Latent ~= nil
    local loop = metadata.Loop == true or metadata.LoopNode ~= nil
    local kind = type(metadata.type) == "string" and metadata.type or ""
    local pure = metadata.Pure == true
    return {
        parameters = parameters,
        parameterTypes = parameterTypes,
        defaults = defaults,
        returns = orderedParameters(metadata["return"], "return"),
        execSplit = orderedEntries(metadata.ExecSplit, "ExecSplit"),
        latentStates = orderedEntries(latentStates, "Latent"),
        latent = latent,
        loop = loop,
        pure = pure,
        loopNode = type(metadata.LoopNode) == "string" and metadata.LoopNode or "",
        kind = kind,
        needsRefLocal = type(metadata.ExecSplit) == "table" or latent
            or loop or pure
            or kind == "event" or type(metadata["return"]) == "table"
    }
end

---@param prefix  string
---@param context NodeCompiler.Context
---@return string[]
local function moduleCandidates(prefix, context)
    local result = {}
    local included = {}
    ---@param moduleName string
    local function append(moduleName)
        if not included[moduleName] then
            included[moduleName] = true
            result[#result + 1] = moduleName
        end
    end
    append(prefix)
    if type(context) == "table" and type(context.moduleCandidates) == "function" then
        for _, moduleName in ipairs(context.moduleCandidates(prefix) or {}) do
            append(moduleName)
        end
    end
    return result
end

---@param functionName string
---@param parentClass  Class.ClassType<any> | nil
---@param context      NodeCompiler.Context
---@return NodeCompiler.Callable | nil, boolean | nil, string | nil
local function resolveCallable(functionName, parentClass, context)
    local explicitSelf = functionName:sub(1, 5) == "self."
    local lookupName = explicitSelf and functionName:sub(6) or functionName
    local parts = splitPath(lookupName)
    if not bool(parts) then
        return nil
    end
    if parentClass ~= nil then
        local candidate = traverse(parentClass, parts, 1)
        if isCallable(candidate) then
            return candidate, explicitSelf or (#parts == 1 and type(parentClass) == "table"),
                Engine.getClassModulePath(parentClass) or ""
        end
        if explicitSelf then
            return nil
        end
    end
    if #parts > 1 then
        for prefixLength = #parts - 1, 1, -1 do
            local prefix = table.concat(parts, ".", 1, prefixLength)
            for _, moduleName in ipairs(moduleCandidates(prefix, context)) do
                local module = loadModule(moduleName)
                if module ~= nil then
                    local candidate = traverse(module, parts, prefixLength + 1)
                    if isCallable(candidate) then
                        return candidate, false, moduleName
                    end
                end
            end
        end
    end
    for _, root in ipairs(context.roots or {}) do
        local startIndex = parts[1] == root.name and 2 or 1
        local candidate = traverse(root.value, parts, startIndex)
        if isCallable(candidate) then
            return candidate, false, root.name
        end
    end
    return nil
end

---@param functionName string
---@param parentClass  Class.ClassType<any> | nil
---@param context      NodeCompiler.Context
---@return table | nil, string
local function resolveMetadata(functionName, parentClass, context)
    local explicitSelf = functionName:sub(1, 5) == "self."
    local lookupName = explicitSelf and functionName:sub(6) or functionName
    local parts = splitPath(lookupName)
    if not bool(parts) then
        return nil, ""
    end
    local memberName = parts[#parts]
    if explicitSelf or #parts == 1 then
        local metadata = nil
        local moduleName = ""
        if type(parentClass) == "table" then
            metadata, moduleName = Engine.resolveMemberMetadata(parentClass, memberName)
        end
        if metadata ~= nil or explicitSelf then
            return metadata, moduleName or ""
        end
    end
    if #parts > 1 then
        for prefixLength = #parts - 1, 1, -1 do
            local prefix = table.concat(parts, ".", 1, prefixLength)
            local typeName = parts[prefixLength]
            ---@cast typeName string
            for _, moduleName in ipairs(moduleCandidates(prefix, context)) do
                local module = loadModule(moduleName)
                if module ~= nil then
                    local metadata = findMemberMetadata(moduleMetadata(moduleName, module), typeName, memberName)
                    if metadata ~= nil then
                        return metadata, moduleName
                    end
                end
            end
        end
    end
    for _, root in ipairs(context.roots or {}) do
        local metadata = findMemberMetadata(
            moduleMetadata(root.name, root.value), #parts > 1 and parts[#parts - 1] or "", memberName
        )
        if metadata ~= nil then
            return metadata, root.name
        end
    end
    return nil, ""
end

function NodeCompiler.compile(functionName, parentClass, context)
    context = context or {}
    local callable, isSelf, declaringModule = resolveCallable(functionName, parentClass, context)
    if callable == nil then
        return nil
    end
    local rawMetadata, metadataModule = resolveMetadata(functionName, parentClass, context)
    if bool(metadataModule) then
        declaringModule = metadataModule
    end
    local memberMeta = normaliseMemberMetadata(rawMetadata)
    local paramNames = {}
    for _, parameter in ipairs(memberMeta.parameters) do
        paramNames[#paramNames + 1] = parameter.name
    end
    if not bool(paramNames) and type(callable) == "function" then
        paramNames = Class.getParameterNames(callable)
    end
    local parts = splitPath(functionName)
    return {
        callable = callable,
        memberMeta = memberMeta,
        paramNames = paramNames,
        isSelf = isSelf == true,
        displayName = parts[#parts] or functionName,
        declaringModule = declaringModule or ""
    }
end

return NodeCompiler
