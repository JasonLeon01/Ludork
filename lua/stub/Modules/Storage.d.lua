---@meta Modules.Storage

---@class LudorkServer.StorageAccountQuery
---@field query   "account_exists"
---@field account string

---@class LudorkServer.StorageAccountsQuery
---@field query          "accounts"
---@field sampleCount    integer | nil
---@field categoryFilter string | nil

---@class LudorkServer.StorageFieldQuery
---@field query    "field"
---@field account  string
---@field category string
---@field field    string

---@class LudorkServer.StorageFieldsQuery
---@field query     "fields"
---@field account   string
---@field category  string
---@field beforeKey string | nil
---@field limit     integer | nil

---@alias LudorkServer.StorageQuery LudorkServer.StorageAccountQuery | LudorkServer.StorageAccountsQuery | LudorkServer.StorageFieldQuery | LudorkServer.StorageFieldsQuery

local Storage = { Rpc = {} }

--- Read one explicit Storage snapshot. Unsupported queries or malformed parameters raise an error.
---@overload fun(context: LudorkServer.Context, params: LudorkServer.StorageAccountQuery): { exists: boolean }
---@overload fun(context: LudorkServer.Context, params: LudorkServer.StorageAccountsQuery): { accounts: string[] }
---@overload fun(context: LudorkServer.Context, params: LudorkServer.StorageFieldQuery): LudorkServer.FieldResult
---@overload fun(context: LudorkServer.Context, params: LudorkServer.StorageFieldsQuery): LudorkServer.FieldPage
---@param context LudorkServer.Context
---@param params  LudorkServer.Value
---@return LudorkServer.Table
function Storage.Snapshot(context, params) end

--- Apply the configured legacy write interval, persist the field and notify the caller with FieldWritten.
--- The event arguments are account, category and field. A nil value removes the field.
---@param context  LudorkServer.Context
---@param account  string
---@param category string
---@param field    string
---@param value    LudorkServer.Value
function Storage.Rpc.WriteField(context, account, category, field, value) end

return Storage
