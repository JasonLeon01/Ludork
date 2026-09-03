---@meta Source.Windows.WindowMessageLayout

local WindowMessageLayout = {}

---@param text string
---@return string
function WindowMessageLayout.NormaliseText(text) end

---@param bounds sf.FloatRect
---@return integer
function WindowMessageLayout.GetTextLineHeight(bounds) end

---@param text          string
---@param maxWidth      number
---@param textConfigKey string
---@return string
function WindowMessageLayout.WrapMessage(text, maxWidth, textConfigKey) end

---@param target Engine.Canvas
---@param width  integer
---@param height integer
function WindowMessageLayout.ResizeCanvas(target, width, height) end

return WindowMessageLayout
