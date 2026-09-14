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
    if package.preload[moduleName] ~= nil then
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

    return Class.isInstance(module, "table") and module or nil
end

---@param moduleName string
---@param module     table
---@return table | nil
local function moduleMetadata(moduleName, module)
    if Class.hasOwnField(module, "__runtimeMetadata") and Class.isInstance(module.__runtimeMetadata, "table") then
        return module.__runtimeMetadata
    end
    local metadataName = moduleName .. "_meta"
    if not moduleExists(metadataName) then
        return nil
    end
    local metadata = require(metadataName)

    return Class.isInstance(metadata, "table") and metadata or nil
end

---@param value      any
---@param parts      string[]
---@param startIndex integer
---@param endIndex   integer | nil
---@return any
local function traverse(value, parts, startIndex, endIndex)
    local current = value
    for index = startIndex, endIndex or #parts do
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
    if Class.isInstance(value, "function") then
        return true
    end
    if not Class.isInstance(value, "table") and not Class.isInstance(value, "userdata") then
        return false
    end
    local valueMeta = getmetatable(value)
    return valueMeta ~= nil and Class.isInstance(valueMeta.__call, "function")
end

---@param value any
---@return boolean
local function isNodeMemberDescriptor(value)
    if not Class.isInstance(value, "table") then
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
---@param moduleName string
---@param owner      any
---@return table | nil, string | nil
local function findMemberMetadata(metadata, typeName, memberName, moduleName, owner)
    if not Class.isInstance(metadata, "table") then
        return nil
    end
    ---@cast metadata table
    if typeName == "" then
        if isNodeMemberDescriptor(metadata[memberName]) then
            return metadata[memberName], moduleName
        end
        typeName = moduleName:match("([^.]+)$") or moduleName
        for name, candidate in pairs(metadata) do
            if Class.isInstance(candidate, "table") and candidate.moduleReturn == true then
                typeName = name
                break
            end
        end
    end
    local typeMetadata = metadata[typeName]
    if not Class.isInstance(typeMetadata, "table") then
        return nil
    end
    if isNodeMemberDescriptor(typeMetadata[memberName]) then
        return typeMetadata[memberName], moduleName
    end
    if bool(typeMetadata.bases) then
        return Engine.resolveMemberMetadata(owner, memberName)
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
    ---@cast values - nil
    for index, name in ipairs(values) do
        if not Class.isInstance(name, "string") or values[name] == nil then
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
    ---@cast values - nil
    for index, name in ipairs(values) do
        if not Class.isInstance(name, "string") then
            error(label .. " order contains an unknown key")
        end
        if included[name] then
            error(label .. " order contains duplicate key '" .. name .. "'")
        end
        included[name] = true
        local rawValue = values[name]
        local entryValues = {}
        local count = 1
        if Class.isInstance(rawValue, "table") then
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
    if not Class.isInstance(metadata, "table") then
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
    ---@cast metadata table
    local parameters = orderedParameters(metadata.parameters, "parameters")
    local parameterTypes = {}
    for _, parameter in ipairs(parameters) do
        parameterTypes[parameter.name] = parameter.type
    end
    local defaults = {}
    if Class.isInstance(metadata.default, "table") then
        ---@cast metadata.default table
        defaults = packedValues(metadata.default, math.max(metadata.default.n or #metadata.default, #parameters))
    end
    local latentStates = metadata.LatentStates
    if not Class.isInstance(latentStates, "table") and Class.isInstance(metadata.Latent, "table") then
        latentStates = metadata.Latent
    end
    local latent = metadata.Latent == true or Class.isInstance(metadata.Latent, "table")
    local loop = metadata.Loop == true or metadata.LoopNode ~= nil
    local kind = Class.isInstance(metadata.type, "string") and metadata.type or ""
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
        loopNode = Class.isInstance(metadata.LoopNode, "string") and metadata.LoopNode or "",
        kind = kind,
        needsRefLocal = Class.isInstance(metadata.ExecSplit, "table") or latent or loop or pure or kind == "event"
            or Class.isInstance(metadata["return"], "table")
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
    if context.moduleCandidates ~= nil then
        for _, moduleName in ipairs(context.moduleCandidates(prefix) or {}) do
            append(moduleName)
        end
    end
    return result
end

---@param functionName string
---@param parentClass  Class.ClassType<any> | nil
---@param context      NodeCompiler.Context
---@return NodeCompiler.Callable | nil, string | nil
local function resolveCallable(functionName, parentClass, context)
    local explicitSelf = functionName:sub(1, 5) == "self."
    local lookupName = explicitSelf and functionName:sub(6) or functionName
    local parts = splitPath(lookupName)
    if not bool(parts) then
        return nil
    end
    if parentClass ~= nil and (explicitSelf or #parts == 1) then
        local candidate = traverse(parentClass, parts, 1)
        if isCallable(candidate) then
            return candidate, Engine.getClassModulePath(parentClass) or ""
        end
    end
    if explicitSelf then
        return nil
    end
    if #parts > 1 then
        for prefixLength = #parts - 1, 1, -1 do
            local prefix = table.concat(parts, ".", 1, prefixLength)
            for _, moduleName in ipairs(moduleCandidates(prefix, context)) do
                local module = loadModule(moduleName)
                if module ~= nil then
                    local candidate = traverse(module, parts, prefixLength + 1)
                    if isCallable(candidate) then
                        return candidate, moduleName
                    end
                end
            end
        end
    end
    for _, root in ipairs(context.roots or {}) do
        local startIndex = parts[1] == root.name and 2 or 1
        local candidate = traverse(root.value, parts, startIndex)
        if isCallable(candidate) then
            return candidate, root.name
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
        if parentClass ~= nil then
            metadata, moduleName = Engine.resolveMemberMetadata(parentClass, memberName)
        end
        if metadata ~= nil or explicitSelf then
            return metadata, moduleName or ""
        end
    end
    if #parts > 1 then
        for prefixLength = #parts - 1, 1, -1 do
            local prefix = table.concat(parts, ".", 1, prefixLength)
            local typeName = table.concat(parts, ".", prefixLength + 1, #parts - 1)
            for _, moduleName in ipairs(moduleCandidates(prefix, context)) do
                local module = loadModule(moduleName)
                if module ~= nil then
                    local metadata, declaringModule = findMemberMetadata(
                        moduleMetadata(moduleName, module), typeName, memberName, moduleName,
                        traverse(module, parts, prefixLength + 1, #parts - 1)
                    )
                    if metadata ~= nil then
                        return metadata, declaringModule or moduleName
                    end
                end
            end
        end
    end
    for _, root in ipairs(context.roots or {}) do
        ---@type string
        local rootName = root.name
        local startIndex = parts[1] == rootName and 2 or 1
        local typeName = table.concat(parts, ".", startIndex, #parts - 1)
        local metadata, declaringModule = findMemberMetadata(
            moduleMetadata(rootName, root.value), typeName, memberName, rootName,
            traverse(root.value, parts, startIndex, #parts - 1)
        )
        if metadata ~= nil then
            return metadata, declaringModule or rootName
        end
    end
    return nil, ""
end

function NodeCompiler.Compile(functionName, parentClass, context)
    context = context or {}
    local callable, declaringModule = resolveCallable(functionName, parentClass, context)
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
    if not bool(paramNames) and Class.isInstance(callable, "function") then
        ---@cast callable function
        paramNames = Class.getParameterNames(callable)
    end
    local parts = splitPath(functionName)
    return {
        callable = callable,
        memberMeta = memberMeta,
        paramNames = paramNames,
        displayName = parts[#parts] or functionName,
        declaringModule = declaringModule or ""
    }
end

return NodeCompiler
