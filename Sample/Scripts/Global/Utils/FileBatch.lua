local FileBatch = {}

function FileBatch.FormatError(errorData)
    if errorData == nil then
        return "File batch failed"
    end
    return string.format("%s failed for %s: %s", errorData.operation, errorData.path, errorData.message)
end

return FileBatch
