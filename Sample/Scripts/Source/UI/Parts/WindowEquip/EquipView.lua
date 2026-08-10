local IconTexture = require("Source.UI.IconTexture")

local EquipViewUtils = {}

function EquipViewUtils.loadIcon(iconPath)
    return IconTexture.load(iconPath, "Characters/items")
end

return EquipViewUtils
