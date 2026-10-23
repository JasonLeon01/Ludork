local Storage = { Rpc = {} }

function Storage.Snapshot(context, params)
    if type(params) ~= "table" then
        error({ code = "InvalidArgument", message = "Storage snapshot parameters must be a table." }, 0)
    end
    if params.query == "account_exists" then
        return { exists = context.Storage:AccountExists(params.account) }
    elseif params.query == "accounts" then
        return { accounts = context.Storage:ListAccounts(params.sampleCount, params.categoryFilter) }
    elseif params.query == "field" then
        return context.Storage:ReadField(params.account, params.category, params.field)
    elseif params.query == "fields" then
        return context.Storage:ListFields(params.account, params.category, params.beforeKey, params.limit)
    end
    error({ code = "InvalidArgument", message = "The Storage query is not supported." }, 0)
end

function Storage.Rpc.WriteField(context, account, category, field, value)
    context:AdmitWrite(account, category, field)
    context.Storage:WriteField(account, category, field, value)
    context:Notify("FieldWritten", account, category, field)
end

return Storage
