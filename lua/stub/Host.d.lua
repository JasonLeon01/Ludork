---@meta

---@class LudorkServer.RequestBase
---@field id string
---@field module string
---@field sessionId string
---@field projectName string
---@field peerAddress string

---@class LudorkServer.SnapshotRequest: LudorkServer.RequestBase
---@field type "snapshot"
---@field params LudorkServer.WireValue

---@class LudorkServer.RpcRequest: LudorkServer.RequestBase
---@field type "rpc"
---@field name string
---@field args LudorkServer.WireValue[]

---@alias LudorkServer.Request LudorkServer.SnapshotRequest | LudorkServer.RpcRequest
---@alias LudorkServer.HostOperation "AccountExists" | "ListAccounts" | "ReadField" | "ListFields" | "WriteField" | "AdmitWrite"

---@class LudorkServer.HostResultBase
---@field type "host_result"
---@field hostId integer

---@class LudorkServer.HostSuccess: LudorkServer.HostResultBase
---@field ok true
---@field data LudorkServer.WireValue

---@class LudorkServer.HostFailure: LudorkServer.HostResultBase
---@field ok false
---@field code string
---@field message string
---@field retryAfterSeconds integer | nil

---@alias LudorkServer.HostResult LudorkServer.HostSuccess | LudorkServer.HostFailure

---@class LudorkServer.HostPromise
local HostPromise = {}

--- Yield the invocation coroutine until the Node.js host finishes this operation.
---@return LudorkServer.HostResult
function HostPromise:await() end

--- Internal bridge installed by the Node.js host. Arguments are an encoded Lua argument table.
---@param name LudorkServer.HostOperation
---@param argsJson string
---@return LudorkServer.HostPromise
function __HostCall(name, argsJson) end

--- Internal bridge installed by the Node.js host. An empty target broadcasts within the project.
---@param target string
---@param event string
---@param argsJson string
function __QueueEvent(target, event, argsJson) end
