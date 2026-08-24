local Validation = {}

function Validation.RequireNamedValue(values, key, message)
    local value = rawget(values, key)
    assert(value ~= nil, message)
    return value
end

return Validation
