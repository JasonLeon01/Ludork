local NetProxy = require("Global.NetProxy")

local StorageProxy = {}

function StorageProxy:AccountExists()
    return self:GetValue("exists")
end

function StorageProxy:GetAccounts()
    return self:GetValue("accounts")
end

function StorageProxy:IsFound()
    return self:GetValue("found")
end

function StorageProxy:GetFieldValue()
    return self:GetValue("value")
end

function StorageProxy:GetEntries()
    return self:GetValue("entries")
end

function StorageProxy:GetNextCursor()
    return self:GetValue("nextCursor")
end

return class(StorageProxy, NetProxy)
