local TelepointKey = {}

function TelepointKey.FromPoint(point)
    if point == nil then
        return tuple()
    end
    return tuple { point.x, point.y }
end

function TelepointKey.FromEntries(entries)
    local parts = {}
    for index, entry in ipairs(entries) do
        parts[index] = tuple { TelepointKey.FromPoint(entry[1]), tostring(entry[2]) }
    end
    return tuple(parts)
end

return TelepointKey
