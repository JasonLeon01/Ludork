local Codec = {}
local alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
local reverse = {}
for index = 1, #alphabet do
    reverse[alphabet:byte(index)] = index - 1
end

local function encodeBytes(value)
    local result = {}
    for index = 1, #value, 3 do
        local a, b, c = value:byte(index, index + 2)
        local number = (a << 16) | ((b or 0) << 8) | (c or 0)
        result[#result + 1] = alphabet:sub(((number >> 18) & 63) + 1, ((number >> 18) & 63) + 1) .. alphabet:sub(
                ((number >> 12) & 63) + 1, ((number >> 12) & 63) + 1
            ) .. (b and alphabet:sub(((number >> 6) & 63) + 1, ((number >> 6) & 63) + 1) or "=")
            .. (c and alphabet:sub((number & 63) + 1, (number & 63) + 1) or "=")
    end
    return table.concat(result)
end

local function decodeBytes(value)
    local result = {}
    for index = 1, #value, 4 do
        local a, b, c, d = value:byte(index, index + 3)
        local number = (reverse[a] << 18) | (reverse[b] << 12) | ((reverse[c] or 0) << 6) | (reverse[d] or 0)
        result[#result + 1] = string.char((number >> 16) & 255)
        if c ~= 61 then
            result[#result + 1] = string.char((number >> 8) & 255)
        end
        if d ~= 61 then
            result[#result + 1] = string.char(number & 255)
        end
    end
    return table.concat(result)
end

local function encoder()
    local nodes = 0
    local active = {}
    local function encode(value, depth)
        nodes = nodes + 1
        if nodes > 65536 or depth > 64 then
            error("Lua data exceeds the depth or node limit.")
        end
        local kind = type(value)
        if kind == "nil" then
            return '["nil"]'
        elseif kind == "boolean" then
            return '["bool",' .. tostring(value) .. ']'
        elseif kind == "string" then
            return '["string","' .. encodeBytes(value) .. '"]'
        elseif kind == "number" then
            if math.type(value) == "integer" then
                return '["int","' .. string.format("%d", value) .. '"]'
            end
            if value ~= value or value == math.huge or value == -math.huge then
                error("Only finite numbers can cross the network.")
            end
            return '["float","' .. string.format("%.17g", value) .. '"]'
        elseif kind ~= "table" then
            error("Only nil, booleans, numbers, strings and ordinary tables can cross the network.")
        end
        if getmetatable(value) ~= nil or active[value] then
            error("Metatables and cyclic tables cannot cross the network.")
        end
        active[value] = true
        local entries = {}
        for key, item in pairs(value) do
            local keyKind = type(key)
            if keyKind ~= "boolean" and keyKind ~= "number" and keyKind ~= "string" then
                error("Table keys must be booleans, finite numbers, or strings.")
            end
            entries[#entries + 1] = '[' .. encode(key, depth + 1) .. ',' .. encode(item, depth + 1) .. ']'
        end
        active[value] = nil
        return '["table",[' .. table.concat(entries, ',') .. ']]'
    end
    return encode
end

function Codec.Encode(value)
    return encoder()(value, 0)
end

function Codec.EncodeArgs(...)
    local encode = encoder()
    local result = {}
    for index = 1, select("#", ...) do
        result[index] = encode(select(index, ...), 0)
    end
    return '[' .. table.concat(result, ',') .. ']'
end

function Codec.Decode(value)
    local tag = value[1]
    if tag == "nil" then
        return nil
    elseif tag == "bool" then
        return value[2]
    elseif tag == "int" then
        return assert(math.tointeger(tonumber(value[2])))
    elseif tag == "float" then
        local text = value[2]
        if not string.find(text, "[.eE]") then
            text = text .. ".0"
        end
        return assert(tonumber(text))
    elseif tag == "string" then
        return decodeBytes(value[2])
    end
    local result = {}
    for _, entry in ipairs(value[2]) do
        result[Codec.Decode(entry[1])] = Codec.Decode(entry[2])
    end
    return result
end

return Codec
