---@meta Source.Windows.WindowMessage.Layout

local WindowMessageLayout = {}

---@param text string
---@return string
function WindowMessageLayout.NormaliseText(text) end

---@param bounds sf.FloatRect
---@return integer
function WindowMessageLayout.GetTextLineHeight(bounds) end

---@param text     string
---@param maxWidth number
---@param control  Engine.RichText
---@return string
function WindowMessageLayout.WrapMessage(text, maxWidth, control) end

return WindowMessageLayout
