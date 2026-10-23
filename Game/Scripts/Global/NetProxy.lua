local GlobalCore = require("GlobalCore")
local Operations = require("Internal.Net.Operations")

local NetProxy = {}

function NetProxy:init(moduleName, params, snapshot)
    self._moduleName = moduleName
    self._params = GlobalCore.NetTransport.CloneValue(params)
    self._snapshot = GlobalCore.NetTransport.CloneValue(snapshot)
    self._lastOperation = nil
end

function NetProxy:GetValue(key)
    return GlobalCore.NetTransport.CloneValue(self._snapshot[key])
end

function NetProxy:Sync(callback)
    assert(Class.isInstance(callback, "function"), "Net completion callback must be a function")
    local operation
    operation = Operations.Fetch(self._moduleName, self._params, function(snapshot)
        self._snapshot = snapshot
        return self
    end, function(result)
        if self._lastOperation == operation then
            self._lastOperation = nil
        end
        callback(result)
    end, self._lastOperation)
    self._lastOperation = operation
    return operation
end

return class(NetProxy)
