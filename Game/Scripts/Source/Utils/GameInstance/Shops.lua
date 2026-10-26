local MapPath = require("Global.Utils.MapPath")

local Shops = {}

---@param mapPath string
---@param mapTag  string
---@return string
local function normaliseKey(mapPath, mapTag)
    assert(Class.isInstance(mapPath, "string") and bool(mapPath), "Shop map path must be a non-empty string")
    assert(Class.isInstance(mapTag, "string") and bool(mapTag), "Shop map tag must be a non-empty string")
    local result = MapPath.Normalise(mapPath)
    assert(bool(result), "Shop map path must be a non-empty map path")
    return result
end

---@param shops   Source.GameInstance.ShopRecord[]
---@param mapPath string
---@param mapTag  string
---@return integer | nil
local function findRecord(shops, mapPath, mapTag)
    for index, record in ipairs(shops) do
        if record[1] == mapPath and record[2] == mapTag then
            return index
        end
    end
    return nil
end

---@param value unknown
---@param name  string
---@return integer
local function requireArrayLength(value, name)
    assert(Class.isInstance(value, "table"), name .. " must be an array")
    ---@cast value table
    local count = 0
    for key in pairs(value) do
        if key ~= "n" then
            assert(
                Class.isInstance(key, "number") and math.type(key) == "integer" and key >= 1,
                name .. " must be an array"
            )
            count = count + 1
        end
    end
    local length = rawget(value, "n")
    assert(length == nil or length == count, name .. " must be a dense array")
    for index = 1, count do
        assert(value[index] ~= nil, name .. " must be a dense array")
    end
    return count
end

function Shops.Append(shops, mapPath, mapTag)
    mapPath = normaliseKey(mapPath, mapTag)
    if findRecord(shops, mapPath, mapTag) ~= nil then
        return false
    end
    shops[#shops + 1] = { mapPath, mapTag }
    return true
end

function Shops.Remove(shops, mapPath, mapTag)
    mapPath = normaliseKey(mapPath, mapTag)
    local index = findRecord(shops, mapPath, mapTag)
    if index == nil then
        return false
    end
    table.remove(shops, index)
    return true
end

function Shops.Decode(shops)
    local result = {}
    if shops == nil then
        return result
    end
    local count = requireArrayLength(shops, "shops")
    for index = 1, count do
        local record = shops[index]
        assert(requireArrayLength(record, "Shop record") == 2, "Shop record must contain a map path and map tag")
        Shops.Append(result, record[1], record[2])
    end
    return result
end

return Shops
