local GlobalFunctions = require("GlobalFunctions")
---@type { NormaliseSeparators: fun(value: string): string }
local Path = require("Global.Utils.Path")

local ManagerFunctions = GlobalFunctions.Manager

local IconTexture = {}

function IconTexture.Load(iconPath, defaultFolder, defaultExtension)
    if not bool(iconPath) then
        return nil
    end
    local normalized = Path.NormaliseSeparators(iconPath)
    local folder = os.path.dirname(normalized)
    local filename = os.path.basename(normalized)
    if bool(folder) and bool(filename) then
        return ManagerFunctions.loadTexture(folder, filename)
    end
    local _, extension = os.path.splitext(filename)
    if defaultExtension ~= nil and not bool(extension) then
        filename = filename .. defaultExtension
    end
    return ManagerFunctions.loadTexture(defaultFolder, filename)
end

function IconTexture.LoadItem(iconPath)
    return IconTexture.Load(iconPath, "Characters/items")
end

return IconTexture
