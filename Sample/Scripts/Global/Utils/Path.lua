local Path = {}

function Path.NormaliseSeparators(value)
    return (value:gsub("\\", "/"))
end

return Path
