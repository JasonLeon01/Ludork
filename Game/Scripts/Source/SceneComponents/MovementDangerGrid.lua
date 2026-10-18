local MovementSpecials = require("Source.Utils.MovementSpecials")
local GameplayConstants = require("Source.Configs.GameplayConstants")

local MovementDangerGrid = {}

---@param enemies        Source.MapActors.Enemy[]
---@param areaX          integer
---@param areaY          integer
---@param areaWidth      integer
---@param areaHeight     integer
---@param previewContext Source.Utils.MovementSpecials.PreviewContext
---@return { x: integer, y: integer, enemies: Source.MapActors.Enemy[] } []
local function collectCandidates(enemies, areaX, areaY, areaWidth, areaHeight, previewContext)
    local candidates = {}
    ---@type table<integer, table<integer, { x: integer, y: integer, enemies: Source.MapActors.Enemy[] }>>
    local rows = {}
    for _, enemy in ipairs(enemies) do
        local radius = previewContext.radiusByEnemy[enemy]
        local position = enemy:getMapPosition()
        local left = position.x - math.min(radius, position.x - areaX)
        local top = position.y - math.min(radius, position.y - areaY)
        local right = position.x + math.min(radius, areaX + areaWidth - 1 - position.x)
        local bottom = position.y + math.min(radius, areaY + areaHeight - 1 - position.y)
        for y = top, bottom do
            local row = rows[y]
            if row == nil then
                row = {}
                rows[y] = row
            end
            for x = left, right do
                local candidate = row[x]
                if candidate == nil then
                    candidate = { x = x, y = y, enemies = {} }
                    row[x] = candidate
                    candidates[#candidates + 1] = candidate
                end
                candidate.enemies[#candidate.enemies + 1] = enemy
            end
        end
    end
    table.sort(candidates, function (left, right)
        if left.y == right.y then
            return left.x < right.x
        end
        return left.y < right.y
    end)
    return candidates
end

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
    return enemy:getAbilitySystemComponent():hasMatchingGameplayTag(GameplayConstants.MOVEMENT_HAZARD_TAG)
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
    for _, candidate in ipairs(collectCandidates(enemies, areaX, areaY, areaWidth, areaHeight, previewContext)) do
        position.x = candidate.x
        position.y = candidate.y
        local entry = calculateEntry(candidate.enemies, player, position, previewContext)
        if entry ~= nil then
            entries[#entries + 1] = entry
            local row = grid[candidate.y + 1] or {}
            grid[candidate.y + 1] = row
            row[candidate.x + 1] = entry
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
    for _, candidate in ipairs(collectCandidates(enemies, areaX, areaY, areaWidth, areaHeight, previewContext)) do
        local entry
        if candidate.y >= previousAreaY and candidate.y < previousBottom
            and candidate.x >= previousAreaX and candidate.x < previousRight then
            local previousRow = previousGrid[candidate.y + 1]
            entry = previousRow ~= nil and previousRow[candidate.x + 1] or nil
        else
            position.x = candidate.x
            position.y = candidate.y
            entry = calculateEntry(candidate.enemies, player, position, previewContext)
        end
        if entry ~= nil then
            entries[#entries + 1] = entry
            local row = grid[candidate.y + 1] or {}
            grid[candidate.y + 1] = row
            row[candidate.x + 1] = entry
        end
    end
    return entries, grid
end

return MovementDangerGrid
