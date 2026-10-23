local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local NetProxy = require("Global.NetProxy")
local Operations = require("Internal.Net.Operations")

local Net = {}

---@type table<string, Class.ClassType<Global.NetProxy> | nil>
local proxyClasses = {}

function Net.DefineModule(name, proxyClass)
    assert(Class.isInstance(name, "string") and name ~= "", "Net module name must be a nonempty string")
    assert(proxyClasses[name] == nil, "Net module is already defined: " .. name)
    assert(Class.isSubclass(proxyClass, NetProxy), "Net proxy class must derive from Global.NetProxy")
    proxyClasses[name] = proxyClass

    local module = {}

    function module.CallRpcFunction(functionName, ...)
        GlobalFunctions.NodeGraph.initLatent()
        GlobalCore.NetTransport.CallRpcFunction(name, functionName, table.pack(...))
    end

    function module.Subscribe(eventName, handler)
        assert(Class.isInstance(handler, "function"), "Net event handler must be a function")
        GlobalFunctions.NodeGraph.initLatent()
        return GlobalCore.NetTransport.Subscribe(name, eventName, function (arguments)
            handler(table.unpack(arguments, 1, arguments.n))
        end)
    end

    function module.SubscribeError(handler)
        assert(Class.isInstance(handler, "function"), "Net error handler must be a function")
        GlobalFunctions.NodeGraph.initLatent()
        return GlobalCore.NetTransport.SubscribeError(name, handler)
    end

    function module.Unsubscribe(token)
        return GlobalCore.NetTransport.Unsubscribe(name, token)
    end

    return module
end

function Net.GetProxy(moduleName, params, callback)
    local proxyClass = assert(proxyClasses[moduleName], "Net module is not defined: " .. moduleName)
    local requestParams = GlobalCore.NetTransport.CloneValue(params)
    return Operations.Fetch(moduleName, requestParams, function (snapshot)
        return proxyClass.new(moduleName, requestParams, snapshot)
    end, callback, nil
    )
end

return Net
