---@meta

---@class LudorkServer.FieldResult
---@field found boolean
---@field value LudorkServer.Value

---@class LudorkServer.FieldEntry
---@field key   string
---@field value LudorkServer.Value

---@class LudorkServer.FieldPage
---@field entries    LudorkServer.FieldEntry[]
---@field nextCursor string | nil

---@class LudorkServer.Storage
local Storage = {}

---@param account string
---@return boolean
function Storage:AccountExists(account) end

--- Return all eligible accounts in UTF-8 byte order when sampleCount is nil or zero.
--- A positive count samples distinct accounts, capped at the number of eligible accounts.
---@param sampleCount    integer | nil
---@param categoryFilter string | nil
---@return string[]
function Storage:ListAccounts(sampleCount, categoryFilter) end

---@param account  string
---@param category string
---@param field    string
---@return LudorkServer.FieldResult
function Storage:ReadField(account, category, field) end

--- Return fields in descending UTF-8 byte order, excluding beforeKey and earlier pages.
--- A nil limit selects 50 entries; explicit limits must be from 1 through 100.
---@param account   string
---@param category  string
---@param beforeKey string | nil
---@param limit     integer | nil
---@return LudorkServer.FieldPage
function Storage:ListFields(account, category, beforeKey, limit) end

--- Persist an ordinary Lua value. A nil value removes the field.
--- Available only during RPC handlers; this call does not apply the legacy write interval.
---@param account  string
---@param category string
---@param field    string
---@param value    LudorkServer.Value
---@return nil
function Storage:WriteField(account, category, field, value) end

---@class LudorkServer.Context
---@field SessionId   string
---@field ModuleName  string
---@field ProjectName string
---@field PeerAddress string
---@field Storage     LudorkServer.Storage
local Context = {}

--- Admit the built-in Storage.WriteField RPC under the configured interval for this project and client IP.
--- Other handlers cannot consume this interval; rejection raises a structured error with retryAfterSeconds.
---@param account  string
---@param category string
---@param field    string
function Context:AdmitWrite(account, category, field) end

--- Queue a named event for this caller in the current module.
--- Events are sent only after the RPC succeeds, to currently connected sessions.
--- Snapshot handlers cannot send events. Events never refresh client proxy snapshots.
---@param event string
---@param ...   LudorkServer.Value
function Context:Notify(event, ...) end

--- Queue a named event for an online session in the current module.
--- Delivery follows the same rules as Notify; disconnected recipients have no replay queue.
---@param sessionId string
---@param event     string
---@param ...       LudorkServer.Value
function Context:Send(sessionId, event, ...) end

--- Queue a named event for all online sessions of this project in the current module.
--- Delivery follows the same rules as Notify.
---@param event string
---@param ...   LudorkServer.Value
function Context:Broadcast(event, ...) end
