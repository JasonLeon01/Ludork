---@meta Internal.UIBase.IconTexture

---@class Internal.UIBase.IconTexture.Module
local IconTexture = {}

---@param iconPath string
---@return sf.Texture | nil
function IconTexture.Load(iconPath) end

---@param controller Internal.UIBase.UiController
---@param nodeName   string
---@param texture    sf.Texture | nil
---@return boolean
function IconTexture.Apply(controller, nodeName, texture) end

return IconTexture
