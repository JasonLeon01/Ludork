local GlobalFunctions = require("GlobalFunctions")
---@type { NormaliseSeparators: fun(value: string): string }
local Path = require("Global.Utils.Path")

local ManagerFunctions = GlobalFunctions.Manager

local IconTexture = {}

function IconTexture.load(iconPath, defaultFolder, defaultExtension)
    if not bool(iconPath) then
        return nil
    end
    local normalized = Path.NormaliseSeparators(iconPath)
    local subfolder, filename = normalized:match("^(.*)/([^/]+)$")
    if bool(subfolder) and bool(filename) then
        return ManagerFunctions.loadTexture(subfolder, filename)
    end
    if defaultExtension ~= nil and iconPath:find("%.") == nil then
        filename = iconPath .. defaultExtension
    else
        filename = iconPath
    end
    return ManagerFunctions.loadTexture(defaultFolder, filename)
end

return IconTexture
