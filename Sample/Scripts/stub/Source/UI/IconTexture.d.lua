---@meta Source.UI.IconTexture

---@class Source.UI.IconTexture.Module
local IconTexture = {}

---@param iconPath string
---@param defaultFolder string
---@param defaultExtension string | nil
---@return sf.Texture | nil
function IconTexture.load(iconPath, defaultFolder, defaultExtension) end

return IconTexture
