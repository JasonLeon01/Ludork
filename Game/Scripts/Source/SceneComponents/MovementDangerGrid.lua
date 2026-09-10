local MovementSpecials = require("Source.MovementSpecials")
local SpecialAbilities = require("Source.Gameplay.SpecialAbilities")

local MovementDangerGrid = {}

local function calculateEntry(enemies, player, position, previewContext)
    local result = MovementSpecials.Preview(enemies, player, position, nil, previewContext)
    local damage = result.data.damage
    if damage <= 0 then
        return nil
    end
    local entryPosition = sf.Vector2i.new(position.x, position.y)
    ---@cast entryPosition sf.Vector2i
    return { position = entryPosition, damage = damage, sources = result.data.sources }
end

function MovementDangerGrid.HasMovementSpecial(enemy)
    return enemy:getAbilitySystemComponent():hasMatchingGameplayTag(SpecialAbilities.MOVEMENT_HAZARD_TAG)
end

function MovementDangerGrid.GetEntryDamage(entry, ignoredEnemySet)
    if ignoredEnemySet == nil then
        return entry.damage
    end
    local damage = 0
    for _, source in ipairs(entry.sources) do
        if not ignoredEnemySet[source.enemy] then
            damage = damage + source.damage
        end
    end
    return damage
end

function MovementDangerGrid.Build(enemies, player, areaX, areaY, areaWidth, areaHeight, previewContext)
    local entries = {}
    local grid = {}
    local position = sf.Vector2i.new(0, 0)
    ---@cast position sf.Vector2i
    for y = areaY, areaY + areaHeight - 1 do
        ---@type table<integer, Source.SceneComponents.MovementDangerEntry>
        local row = {}
        grid[y + 1] = row
        position.y = y
        for x = areaX, areaX + areaWidth - 1 do
            position.x = x
            local entry = calculateEntry(enemies, player, position, previewContext)
            if entry ~= nil then
                entries[#entries + 1] = entry
                row[x + 1] = entry
            end
        end
    end
    return entries, grid
end

function MovementDangerGrid.Refresh(
    enemies, player, areaX, areaY, areaWidth, areaHeight, previousAreaX, previousAreaY, previousAreaWidth,
    previousAreaHeight, previousGrid, previewContext
)
    local entries = {}
    local grid = {}
    local previousRight = previousAreaX + previousAreaWidth
    local previousBottom = previousAreaY + previousAreaHeight
    local position = sf.Vector2i.new(0, 0)
    ---@cast position sf.Vector2i
    for y = areaY, areaY + areaHeight - 1 do
        ---@type table<integer, Source.SceneComponents.MovementDangerEntry>
        local row = {}
        grid[y + 1] = row
        local previousRow = previousGrid[y + 1]
        local reusePreviousRow = y >= previousAreaY and y < previousBottom
        position.y = y
        for x = areaX, areaX + areaWidth - 1 do
            local entry
            if reusePreviousRow and x >= previousAreaX and x < previousRight then
                entry = previousRow ~= nil and previousRow[x + 1] or nil
            else
                position.x = x
                entry = calculateEntry(enemies, player, position, previewContext)
            end
            if entry ~= nil then
                entries[#entries + 1] = entry
                row[x + 1] = entry
            end
        end
    end
    return entries, grid
end

return MovementDangerGrid
