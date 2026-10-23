---@meta Global.Net.StorageProxy

---@class Global.Net.StorageProxy: Global.NetProxy
local StorageProxy = {}

--- Available for the account_exists query.
---@return boolean | nil
function StorageProxy:AccountExists() end

--- Available for the accounts query.
---@return string[] | nil
function StorageProxy:GetAccounts() end

--- Available for the field query; nil values are absent rather than stored nulls.
---@return boolean | nil
function StorageProxy:IsFound() end

---@return Global.Net.Value
function StorageProxy:GetFieldValue() end

---@class Global.Net.StorageProxy.Entry
---@field key   string
---@field value Global.Net.Value

--- Available for the fields query, in descending UTF-8 key order.
---@return Global.Net.StorageProxy.Entry[] | nil
function StorageProxy:GetEntries() end

--- Exclusive upper bound for the next page; nil indicates the final page.
---@return string | nil
function StorageProxy:GetNextCursor() end

---@cast StorageProxy Global.Net.StorageProxy & Class.ClassType<Global.Net.StorageProxy>
return StorageProxy
