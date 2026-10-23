local Codec = require("Framework.Codec")
local Net = {}
---@type table<string, LudorkServer.ModuleDefinition<table<string, function>>>
local modules = {}

function Net.RegisterModule(name, definition)
    assert(type(name) == "string" and #name > 0 and #name <= 128 and name:match("^[%w._-]+$"), "A module name is required.")
    assert(modules[name] == nil, "The module is already registered.")
    assert(type(definition) == "table", "A module definition is required.")
    assert(definition.Snapshot == nil or type(definition.Snapshot) == "function", "Snapshot must be a function.")
    assert(definition.Rpc == nil or type(definition.Rpc) == "table", "Rpc must be a table.")
    for key, value in pairs(definition.Rpc or {}) do
        assert(type(key) == "string" and #key > 0 and #key <= 128 and key:match("^[%w._-]+$") and type(value) == "function", "Rpc entries must be named functions.")
    end
    modules[name] = definition
end

---@param name LudorkServer.HostOperation
---@param ... LudorkServer.Value
---@return LudorkServer.Value
local function hostCall(name, ...)
    local args = {...}
    ---@cast args LudorkServer.Table
    local response = __HostCall(name, Codec.Encode(args)):await()
    if not response.ok then
        error({code = response.code, message = response.message, retryAfterSeconds = response.retryAfterSeconds}, 0)
    end
    ---@cast response LudorkServer.HostSuccess
    return Codec.Decode(response.data)
end

---@param request LudorkServer.Request
---@return LudorkServer.Context
local function contextFor(request)
    local context = {SessionId = request.sessionId, ModuleName = request.module, ProjectName = request.projectName, PeerAddress = request.peerAddress, Storage = {}}
    for _, name in ipairs({"AccountExists", "ListAccounts", "ReadField", "ListFields", "WriteField"}) do
        context.Storage[name] = function(_, ...)
            return hostCall(name, ...)
        end
    end
    function context.AdmitWrite(_, account, category, field)
        hostCall("AdmitWrite", account, category, field)
    end
    function context.Notify(_, event, ...)
        __QueueEvent(request.sessionId, event, Codec.EncodeArgs(...))
    end
    function context.Send(_, sessionId, event, ...)
        __QueueEvent(sessionId, event, Codec.EncodeArgs(...))
    end
    function context.Broadcast(_, event, ...)
        __QueueEvent("", event, Codec.EncodeArgs(...))
    end
    ---@cast context LudorkServer.Context
    return context
end

---@param request LudorkServer.Request
function Net.Dispatch(request)
    local ok, result = pcall(function()
        local definition = modules[request.module]
        if definition == nil then
            error({code = "InvalidArgument", message = "The network module is not registered."}, 0)
        end
        local context = contextFor(request)
        if request.type == "snapshot" then
            ---@cast request LudorkServer.SnapshotRequest
            if definition.Snapshot == nil then
                error({code = "InvalidArgument", message = "The module does not provide a snapshot."}, 0)
            end
            local snapshot = definition.Snapshot(context, Codec.Decode(request.params))
            if type(snapshot) ~= "table" then
                error("Snapshot must return an ordinary table.")
            end
            return snapshot
        end
        ---@cast request LudorkServer.RpcRequest
        local callback = (definition.Rpc or {})[request.name]
        if callback == nil then
            error({code = "InvalidArgument", message = "The RPC function is not registered."}, 0)
        end
        ---@cast callback LudorkServer.RpcHandler
        local args = {}
        for index, value in ipairs(request.args) do
            args[index] = Codec.Decode(value)
        end
        callback(context, table.unpack(args, 1, #request.args))
    end)
    if not ok then
        if type(result) == "table" and type(result.code) == "string" and type(result.message) == "string" then
            return Codec.Encode({ok = false, code = result.code, message = result.message, retryAfterSeconds = result.retryAfterSeconds})
        end
        return Codec.Encode({ok = false, code = "ServerFailure", message = tostring(result), reset = result == "not enough memory"})
    end
    return Codec.Encode({ok = true, data = result})
end

return Net
