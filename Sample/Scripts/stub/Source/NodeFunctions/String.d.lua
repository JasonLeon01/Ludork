---@meta Source.NodeFunctions.String

---@generic T
---@param value T | nil
---@return string
function String.ToString(value) end

---@param value string
---@return integer
function String.GetIntFromStr(value) end

---@param value string
---@return number
function String.GetFloatFromStr(value) end

---@param str1 string
---@param str2 string
---@return string
function String.StringConcat(str1, str2) end

---@param str1 string
---@param str2 string
---@return boolean
function String.StringContains(str1, str2) end

---@param str1 string
---@return integer
function String.StringLength(str1) end

---@param str1 string
---@param str2 string
---@return integer | nil
function String.StringFind(str1, str2) end

---@param str1 string
---@param str2 string
---@param str3 string
---@return string
function String.StringReplace(str1, str2, str3) end

---@param str1 string
---@param str2 string
---@return string[]
function String.StringSplit(str1, str2) end

---@param str1   string
---@param start  integer
---@param finish integer
---@return string
function String.StringSubstring(str1, start, finish) end

---@param str1 string
---@return string
function String.StringToLower(str1) end

---@param str1 string
---@return string
function String.StringToUpper(str1) end

---@param str1 string
---@return string
function String.StringStrip(str1) end

return String
