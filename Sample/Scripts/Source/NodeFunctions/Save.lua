local SourceSave = require("Source.Save")
local Context = require("Source.NodeFunctions.Context")

local Save = {}

---@param filePath string
---@return string
local function resolveFilePath(filePath)
    if not bool(filePath) then
        return SourceSave.GetSavePath(1)
    end
    return filePath
end

function Save.SaveGame(filePath)
    SourceSave.SaveGame(resolveFilePath(filePath), Context.RequireGameInstance())
end

function Save.LoadGame(filePath)
    local instance = SourceSave.LoadGame(resolveFilePath(filePath))
    if instance == nil then
        return 1
    end
    Context.RequireSceneMap().inst = instance
    return 0
end

function Save.GetSavePath(slot)
    slot = slot == nil and 1 or slot
    return SourceSave.GetSavePath(slot)
end

return Save
