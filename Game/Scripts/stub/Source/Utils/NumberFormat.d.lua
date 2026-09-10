---@meta Source.Utils.NumberFormat

local NumberFormat = {}

---@brief Convert large numeric values to short display text.
---
--- Number or digit-only string inputs use the Game project's k, m and b display units.
--- Other values are returned unchanged; an absent value is treated as zero.
---@param value number | string | nil
---@return number | string
function NumberFormat.ToShortNumber(value) end

return NumberFormat
