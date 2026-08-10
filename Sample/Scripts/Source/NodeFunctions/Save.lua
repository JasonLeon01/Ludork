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
    SourceSave.SaveGame(resolveFilePath(filePath), Context.requireGameInstance())
end

function Save.LoadGame(filePath)
    local instance = SourceSave.LoadGame(resolveFilePath(filePath))
    if instance == nil then
        return 1
    end
    Context.requireSceneMap().inst = instance
    return 0
end

function Save.GetSavePath(slot)
    slot = slot == nil and 1 or slot
    local integerSlot = math.tointeger(slot)
    ---@cast integerSlot integer
    return SourceSave.GetSavePath(integerSlot)
end

return Save
