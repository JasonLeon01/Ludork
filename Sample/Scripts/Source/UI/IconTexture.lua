local GlobalCore = require("GlobalCore")

local TextureManager = GlobalCore.TextureManager

local IconTexture = {}

function IconTexture.Load(iconPath)
    if not bool(iconPath) then
        return nil
    end
    return TextureManager.load(iconPath)
end

return IconTexture
