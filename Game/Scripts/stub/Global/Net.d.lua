---@meta Global.Net

---@alias Global.Net.Key boolean | number | string
---@alias Global.Net.Value nil | boolean | number | string | table<Global.Net.Key, Global.Net.Value>

---@class Global.Net.Error
---@field code              string
---@field message           string
---@field module            string
---@field functionName      string | nil
---@field requestId         string | nil
---@field retryAfterSeconds integer | nil

---@class Global.Net.Result<T: Global.NetProxy>
---@field ok                boolean
---@field code              string
---@field message           string
---@field proxy             T | nil
---@field retryAfterSeconds integer | nil

---@class Global.Net.Module
local NetModule = {}

--- Call the identically named server Lua RPC. There are no business return values.
--- Arguments preserve nil positions and only accept ordinary, acyclic Lua data.
--- When disabled, only log a diagnostic; no RPC or error event is dispatched.
--- An accepted write is never replayed automatically after a timeout or disconnect.
---@param functionName string
---@param ...          Global.Net.Value
function NetModule.CallRpcFunction(functionName, ...) end

--- Listen for online events in this module. Events do not refresh existing proxies.
--- When disabled, log a diagnostic and return 0 without registering a listener.
---@param eventName string
---@param handler   fun(...: Global.Net.Value)
---@return integer
function NetModule.Subscribe(eventName, handler) end

--- Listen for transport and RPC failures. Business results arrive through named events.
--- When disabled, log a diagnostic and return 0 without registering a listener.
---@param handler fun(error: Global.Net.Error)
---@return integer
function NetModule.SubscribeError(handler) end

--- When disabled, log a diagnostic and return false.
---@param token integer
---@return boolean
function NetModule.Unsubscribe(token) end

local Net = {}

--- Register a unique module and its Lua proxy class without connecting.
--- Project modules explicitly require their definitions before calling GetProxy.
---@param name       string
---@param proxyClass Class.ClassType<Global.NetProxy>
---@return Global.Net.Module
function Net.DefineModule(name, proxyClass) end

--- Fetch a fresh snapshot and construct a new proxy for every call.
--- When disabled, log a diagnostic and return nil without creating an operation or invoking the callback.
--- Completion runs on the logic thread after storing the same result object on the operation.
--- Cancelling suppresses construction and the callback. Requests have a ten-second deadline.
---@param moduleName string
---@param params     Global.Net.Value
---@param callback   fun(result: Global.Net.Result<Global.NetProxy>)
---@return Engine.AsyncOperation | nil
function Net.GetProxy(moduleName, params, callback) end

return Net
