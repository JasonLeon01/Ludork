local SHORT_NUMBER_UNITS = { { 1000000000, 1000000000, "b" }, { 1000000, 1000000, "m" }, { 10000, 1000, "k" } }

local NumberFormat = {}

---@param value number | string
---@return number | nil
local function getShortNumberValue(value)
    if Class.isInstance(value, "number") then
        ---@cast value number
        if not math.isFinite(value) then
            return nil
        end
        return value
    end
    if Class.isInstance(value, "string") then
        ---@cast value string
        if value:match("^%d+$") ~= nil then
            return tonumber(value)
        end
    end
    return nil
end

function NumberFormat.ToShortNumber(value)
    value = value == nil and 0 or value
    local numericValue = getShortNumberValue(value)
    if numericValue == nil then
        return value
    end
    local absoluteValue = math.abs(numericValue)
    for _, unit in ipairs(SHORT_NUMBER_UNITS) do
        if absoluteValue > unit[1] then
            return string.format("%.1f%s", numericValue / unit[2], unit[3])
        end
    end
    return numericValue
end

return NumberFormat
