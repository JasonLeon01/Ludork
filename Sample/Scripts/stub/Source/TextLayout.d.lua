---@meta Source.TextLayout

---@class Source.TextLayout.Module
---@field MeasurePlainText fun(textConfigKey: string, text: string): number
---@field MeasureRichText  fun(textConfigKey: string, text: string): number
---@field FitPlainText     fun(text: string, maxWidth: number, textConfigKey: string): string
---@field WrapPlainText    fun(text: string, maxWidth: number, textConfigKey: string): string
---@field WrapRichText     fun(text: string, maxWidth: number, textConfigKey: string): string
local TextLayout = {}

---@param textConfigKey string
---@param text          string
---@return number
function TextLayout.MeasurePlainText(textConfigKey, text) end

---@param textConfigKey string
---@param text          string
---@return number
function TextLayout.MeasureRichText(textConfigKey, text) end

---@param text          string
---@param maxWidth      number
---@param textConfigKey string
---@return string
function TextLayout.FitPlainText(text, maxWidth, textConfigKey) end

---@param text          string
---@param maxWidth      number
---@param textConfigKey string
---@return string
function TextLayout.WrapPlainText(text, maxWidth, textConfigKey) end

---@param text          string
---@param maxWidth      number
---@param textConfigKey string
---@return string
function TextLayout.WrapRichText(text, maxWidth, textConfigKey) end

return TextLayout
