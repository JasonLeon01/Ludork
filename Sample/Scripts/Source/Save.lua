local CoreSystem = require("CoreSystem")
local Engine = require("Engine")
local Logging = require("Global.Utils.Logging")

local Save = {}
local SAVE_FILE_EXTENSION = bool(SAVE_AS_LDC) and ".ldc" or ".json"

local function assertConfiguredSavePath(filePath)
    local _, fileExtension = os.path.splitext(filePath)
    assert(
        fileExtension:lower() == SAVE_FILE_EXTENSION,
        "Save file extension must match SAVE_AS_LDC (" .. SAVE_FILE_EXTENSION .. ")"
    )
end

function Save.SaveGame(filePath, instance)
    assertConfiguredSavePath(filePath)
    local directory = os.path.dirname(os.path.abspath(filePath))
    CoreSystem.createDirectories(directory)
    Engine.writeJSON(filePath, instance:asDict())
    Logging.info("Saved game to %s", filePath)
end

function Save.LoadGame(filePath)
    local GameInstance = require("Source.GameInstance")

    assertConfiguredSavePath(filePath)
    if not CoreSystem.exists(filePath) then
        return nil
    end
    local instance = GameInstance.FromDict(Engine.getJSONData(filePath))
    Logging.info("Loaded game from %s", filePath)
    return instance
end

function Save.GetSavePath(slot)
    slot = slot == nil and 1 or slot
    return Engine.getSavePath() .. "/Save_" .. tostring(slot) .. SAVE_FILE_EXTENSION
end

return Save
