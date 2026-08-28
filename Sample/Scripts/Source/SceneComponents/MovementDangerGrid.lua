---@type { Special: Source.Configs.GeneralEnum.Special }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local MovementSpecials = require("Source.MovementSpecials")

local Special = GeneralEnum.Special

local MovementDangerGrid = {}

local function calculateEntry(enemies, player, position)
    local damage, sources = MovementSpecials.CalculateDangerAtPosition(enemies, player, position, nil)
    if damage <= 0 then
        return nil
    end
    local entryPosition = sf.Vector2i.new(position.x, position.y)
    ---@cast entryPosition sf.Vector2i
    return { position = entryPosition, damage = damage, sources = sources }
end

local function markCandidate(rows, x, y, areaX, areaY, right, bottom)
    if x < areaX or y < areaY or x >= right or y >= bottom then
        return
    end
    local row = rows[y] or {}
    rows[y] = row
    row[x] = true
end

local function buildCandidateRows(enemies, areaX, areaY, areaWidth, areaHeight)
    local rows = {}
    local right = areaX + areaWidth
    local bottom = areaY + areaHeight
    for _, enemy in ipairs(enemies) do
        local enemyPosition = enemy:getMapPosition()
        if enemy:hasSpecial(Special.Domain) then
            local radius = enemy:getSpecialIntValue(Special.Domain, 0, 1) - 1
            local firstY = math.max(areaY, enemyPosition.y - radius)
            local lastY = math.min(bottom - 1, enemyPosition.y + radius)
            for y = firstY, lastY do
                local horizontalRadius = radius - math.abs(y - enemyPosition.y)
                local firstX = math.max(areaX, enemyPosition.x - horizontalRadius)
                local lastX = math.min(right - 1, enemyPosition.x + horizontalRadius)
                for x = firstX, lastX do
                    markCandidate(rows, x, y, areaX, areaY, right, bottom)
                end
            end
        end
        if enemy:hasSpecial(Special.Blockade) or enemy:hasSpecial(Special.Flank) then
            markCandidate(rows, enemyPosition.x - 1, enemyPosition.y, areaX, areaY, right, bottom)
            markCandidate(rows, enemyPosition.x + 1, enemyPosition.y, areaX, areaY, right, bottom)
            markCandidate(rows, enemyPosition.x, enemyPosition.y - 1, areaX, areaY, right, bottom)
            markCandidate(rows, enemyPosition.x, enemyPosition.y + 1, areaX, areaY, right, bottom)
        end
    end
    return rows
end

function MovementDangerGrid.HasMovementSpecial(enemy)
    return enemy:hasSpecial(Special.Domain) or enemy:hasSpecial(Special.Flank) or enemy:hasSpecial(Special.Blockade)
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

function MovementDangerGrid.Build(enemies, player, areaX, areaY, areaWidth, areaHeight)
    local entries = {}
    local grid = {}
    local position = sf.Vector2i.new(0, 0)
    ---@cast position sf.Vector2i
    local candidates = buildCandidateRows(enemies, areaX, areaY, areaWidth, areaHeight)
    for y = areaY, areaY + areaHeight - 1 do
        ---@type table<integer, Source.SceneComponents.MovementDangerEntry>
        local row = {}
        grid[y + 1] = row
        local candidateRow = candidates[y]
        position.y = y
        for x = areaX, areaX + areaWidth - 1 do
            if candidateRow ~= nil and candidateRow[x] then
                position.x = x
                local entry = calculateEntry(enemies, player, position)
                if entry ~= nil then
                    entries[#entries + 1] = entry
                    row[x + 1] = entry
                end
            end
        end
    end
    return entries, grid
end

function MovementDangerGrid.Refresh(
    enemies, player, areaX, areaY, areaWidth, areaHeight, previousAreaX, previousAreaY, previousAreaWidth,
    previousAreaHeight, previousGrid
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
                entry = calculateEntry(enemies, player, position)
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
