---@meta Source.TextLayout

local TextLayout = {}

---@param textConfigKey string
---@param text string
---@return number
function TextLayout.measurePlainText(textConfigKey, text) end

---@param textConfigKey string
---@param text string
---@return number
function TextLayout.measureRichText(textConfigKey, text) end

---@param text string
---@param maxWidth number
---@param textConfigKey string
---@return string
function TextLayout.fitPlainText(text, maxWidth, textConfigKey) end

---@param text string
---@param maxWidth number
---@param textConfigKey string
---@return string
function TextLayout.wrapPlainText(text, maxWidth, textConfigKey) end

---@param text string
---@param maxWidth number
---@param textConfigKey string
---@return string
function TextLayout.wrapRichText(text, maxWidth, textConfigKey) end

return TextLayout
