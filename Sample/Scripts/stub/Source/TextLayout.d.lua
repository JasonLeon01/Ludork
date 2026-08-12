---@meta Source.TextLayout

---@class Source.TextLayout.Module
---@field measurePlainText fun(textConfigKey: string, text: string): number
---@field measureRichText  fun(textConfigKey: string, text: string): number
---@field fitPlainText     fun(text: string, maxWidth: number, textConfigKey: string): string
---@field wrapPlainText    fun(text: string, maxWidth: number, textConfigKey: string): string
---@field wrapRichText     fun(text: string, maxWidth: number, textConfigKey: string): string
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
