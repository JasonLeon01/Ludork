---@meta Source.Data.TextConfigParser

local TextConfigParser = {}

---@param data  table
---@param name  string
---@param value table
---@return Engine.PlainTextConfig
function TextConfigParser.BuildPlain(data, name, value) end

---@param data  table
---@param name  string
---@param value table
---@return Engine.RichTextConfig
function TextConfigParser.BuildRich(data, name, value) end

return TextConfigParser
