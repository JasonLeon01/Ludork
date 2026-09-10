---@meta Global.Pool

---@class Global.Pool.Module
---@field Get fun<T>(className: string, classType: { new: fun(): T }, objectTable?: table): T
---@field Put fun<T>(className: string, object: T)
---@field Release fun(className: string)
local Pool = {}

---@generic T
---@param className    string
---@param classType    { new: fun(): T }
---@param objectTable? table
---@return T
function Pool.Get(className, classType, objectTable) end

--- Only return objects after native consumers have copied their values.
--- Do not return an object twice or while Lua or native code still retains it.
---@generic T
---@param className string
---@param object    T
function Pool.Put(className, object) end

---@param className string
function Pool.Release(className) end

return Pool
