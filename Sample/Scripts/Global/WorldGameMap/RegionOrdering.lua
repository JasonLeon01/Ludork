local RegionOrdering = {}

local function demandedBefore(world, left, right)
    local leftPriority = left.demand == "Active" and 0 or 1
    local rightPriority = right.demand == "Active" and 0 or 1
    if leftPriority ~= rightPriority then
        return leftPriority < rightPriority
    end
    local centreX = world._worldPreviousCameraCenterX or 0
    local centreY = world._worldPreviousCameraCenterY or 0
    local leftX = left.x + left.width / 2 - centreX
    local leftY = left.y + left.height / 2 - centreY
    local rightX = right.x + right.width / 2 - centreX
    local rightY = right.y + right.height / 2 - centreY
    local leftDistance = leftX * leftX + leftY * leftY
    local rightDistance = rightX * rightX + rightY * rightY
    if leftDistance == rightDistance then
        return left.index < right.index
    end
    return leftDistance < rightDistance
end

local function usedBefore(left, right)
    local leftTime = left.lastUsed or -math.huge
    local rightTime = right.lastUsed or -math.huge
    if leftTime == rightTime then
        return left.index < right.index
    end
    return leftTime < rightTime
end

local function indexBefore(left, right)
    return left.index < right.index
end

function RegionOrdering.SortByDemand(world, regions)
    table.sort(regions, function (left, right)
        return demandedBefore(world, left, right)
    end)
end

function RegionOrdering.SortByLastUsed(regions)
    table.sort(regions, usedBefore)
end

function RegionOrdering.SortByIndex(regions)
    table.sort(regions, indexBefore)
end

return RegionOrdering
