---@meta Framework.Net

---@alias LudorkServer.RpcHandler fun(context: LudorkServer.Context, ...: LudorkServer.Value)
---@alias LudorkServer.SnapshotHandler fun(context: LudorkServer.Context, params: LudorkServer.Value): LudorkServer.Table

---@class LudorkServer.ModuleDefinition<Rpc: table<string, function>>
---@field Snapshot LudorkServer.SnapshotHandler | nil
---@field Rpc Rpc | nil

local Net = {}

--- Register a unique module during project initialization.
--- Module and RPC names contain 1 to 128 ASCII letters, digits, dots, underscores or hyphens.
--- Snapshot returns a fresh ordinary Lua table; its handler cannot write storage or send events.
--- RPC handlers receive positional arguments with nil positions preserved; their return values are discarded.
---@generic Rpc: table<string, function>
---@param name string
---@param definition LudorkServer.ModuleDefinition<Rpc>
function Net.RegisterModule(name, definition) end

--- Host dispatch entry point. Project logic registers modules instead of calling this directly.
--- Return the encoded success or failure envelope; RPC success carries no business result.
---@param request LudorkServer.Request
---@return string
function Net.Dispatch(request) end

return Net
