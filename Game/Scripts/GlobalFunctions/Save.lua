local SourceSave
local function loadSourceSave()
    if SourceSave == nil then
        SourceSave = require("Source.Save")
    end
    return SourceSave
end

local Context
local function loadContext()
    if Context == nil then
        Context = require("GlobalFunctions.Context")
    end
    return Context
end

local Save = {}

---@param filePath string
---@return string
local function resolveFilePath(filePath)
    if not bool(filePath) then
        return loadSourceSave().GetSavePath(1)
    end
    return filePath
end

function Save.SaveGame(filePath)
    loadSourceSave().SaveGame(resolveFilePath(filePath), loadContext().RequireGameInstance())
end

function Save.LoadGame(filePath)
    local instance = loadSourceSave().LoadGame(resolveFilePath(filePath))
    if instance == nil then
        return 1
    end
    loadContext().RequireSceneMap().inst = instance
    return 0
end

function Save.GetSavePath(slot)
    slot = slot == nil and 1 or slot
    return loadSourceSave().GetSavePath(slot)
end

return Save
