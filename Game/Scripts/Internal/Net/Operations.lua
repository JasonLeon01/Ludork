local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")

local Operations = {}

function Operations.Fetch(moduleName, params, transform, callback, after)
    assert(Class.isInstance(callback, "function"), "Net completion callback must be a function")
    GlobalFunctions.NodeGraph.initLatent()
    return GlobalCore.NetTransport.GetSnapshotAsync(moduleName, params, function (response)
        ---@cast response Internal.Net.Response
        local result = {
            ok = response.ok,
            code = response.code,
            message = response.message,
            retryAfterSeconds = response.retryAfterSeconds
        }
        if response.ok then
            result.proxy = transform(assert(response.data, "A successful Net snapshot must contain a table"))
        end
        return result
    end, callback, after
    )
end

return Operations
