local String = {}

function String.ToString(value)
    value = value == nil and "" or value
    return tostring(value)
end

function String.GetIntFromStr(value)
    value = value == nil and "0" or value
    local number = assert(tonumber(value), "invalid literal for int: " .. tostring(value))
    return (math.modf(number))
end

function String.GetFloatFromStr(value)
    value = value == nil and "0" or value
    return assert(tonumber(value), "could not convert string to float: " .. tostring(value))
end

function String.StringConcat(str1, str2)
    str1 = str1 == nil and "" or str1
    str2 = str2 == nil and "" or str2
    return str1 .. str2
end

function String.StringContains(str1, str2)
    str1 = str1 == nil and "" or str1
    str2 = str2 == nil and "" or str2
    return str1:contains(str2)
end

function String.StringLength(str1)
    str1 = str1 == nil and "" or str1
    return str1:utf8Length()
end

function String.StringFind(str1, str2)
    str1 = str1 == nil and "" or str1
    str2 = str2 == nil and "" or str2
    local first = string.find(str1, str2, 1, true)
    if first == nil then
        return -1
    end
    return string.sub(str1, 1, first - 1):utf8Length()
end

function String.StringReplace(str1, str2, str3)
    str1 = str1 == nil and "" or str1
    str2 = str2 == nil and "" or str2
    str3 = str3 == nil and "" or str3
    return str1:replace(str2, str3)
end

function String.StringSplit(str1, str2)
    str1 = str1 == nil and "" or str1
    str2 = str2 == nil and "," or str2
    return str1:split(str2)
end

function String.StringSubstring(str1, start, finish)
    str1 = str1 == nil and "" or str1
    start = start == nil and 0 or start
    finish = finish == nil and 0 or finish
    return str1:utf8Slice(start, finish)
end

function String.StringToLower(str1)
    str1 = str1 == nil and "" or str1
    return string.lower(str1)
end

function String.StringToUpper(str1)
    str1 = str1 == nil and "" or str1
    return string.upper(str1)
end

function String.StringStrip(str1)
    str1 = str1 == nil and "" or str1
    return str1:strip()
end

return String
