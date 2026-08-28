---@meta Source.UI.IconTexture

---@class Source.UI.IconTexture.Module
local IconTexture = {}

---@param iconPath         string
---@param defaultFolder    string
---@param defaultExtension string | nil
---@return sf.Texture | nil
function IconTexture.Load(iconPath, defaultFolder, defaultExtension) end

---@param iconPath string
---@return sf.Texture | nil
function IconTexture.LoadItem(iconPath) end

return IconTexture
