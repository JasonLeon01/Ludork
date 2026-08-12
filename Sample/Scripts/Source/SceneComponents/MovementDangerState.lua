local ComponentBase = require("Global.Components.ComponentBase")
local Enemy = require("Source.Enemy")
---@type { Special: Source.Configs.GeneralEnum.Special }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local MovementSpecials = require("Source.MovementSpecials")

local Special = GeneralEnum.Special

local MovementDangerState = {}

---@param enemy Source.Enemy
---@return boolean
local function hasMovementSpecial(enemy)
    enemy:normaliseInfoComp()
    local special = enemy.infoComp.special
    return special ~= nil and (special[Special.Domain] ~= nil or special[Special.Flank] ~= nil
        or special[Special.Blockade] ~= nil)
end

---@param entry Source.SceneComponents.MovementDangerEntry
---@param ignoredEnemySet table<Source.Enemy, boolean> | nil
---@return integer
local function getEntryDamage(entry, ignoredEnemySet)
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

function MovementDangerState:init(gameMap)
    super(MovementDangerState, self).init(gameMap)
    local size = gameMap:getSize()
    ---@type Source.Player.Player | nil
    self._player = nil
    ---@type Source.Components.PlayerInfoComponent | nil
    self._playerInfoComp = nil
    ---@type integer | nil
    self._playerCombatRevision = nil
    ---@type table<Source.Enemy, { x: integer | nil, y: integer | nil, combatRevision: integer | nil, infoComp: Source.Components.EnemyInfoComponent | nil, scanRevision: integer | nil }>
    self._enemySnapshots = {}
    ---@type Source.Enemy[]
    self._enemies = {}
    ---@type integer
    self._enemyScanRevision = 0
    ---@type Source.SceneComponents.MovementDangerEntry[]
    self._entries = {}
    ---@type table<integer, table<integer, Source.SceneComponents.MovementDangerEntry>>
    self._entryGrid = {}
    ---@type integer
    self._revision = 0
    self._mapWidth = size.x
    self._mapHeight = size.y
end

function MovementDangerState:onTick(_deltaTime)
    ---@type GameMap
    local gameMap = self._parent
    local player = gameMap:getPlayer()
    local changed = player ~= self._player
    ---@type integer | nil
    local playerCombatRevision = nil
    ---@type Source.Components.PlayerInfoComponent | nil
    local playerInfoComp = nil
    if player ~= nil then
        ---@cast player Source.Player.Player
        player:normaliseInfoComp()
        playerCombatRevision = player:getCombatRevision()
        playerInfoComp = player.infoComp
    end
    if playerCombatRevision ~= self._playerCombatRevision or playerInfoComp ~= self._playerInfoComp then
        changed = true
    end

    local enemies = self._enemies
    local enemySnapshots = self._enemySnapshots
    local enemyCount = 0
    local enemyScanRevision = self._enemyScanRevision + 1
    self._enemyScanRevision = enemyScanRevision
    if player ~= nil then
        ---@cast player Source.Player.Player
        for _, actorList in pairs(gameMap._actors) do
            for _, actor in ipairs(actorList) do
                if Class.isInstance(actor, Enemy) and not actor:isDestroyed() and hasMovementSpecial(actor) then
                    ---@cast actor Source.Enemy
                    local position = actor:getMapPosition()
                    local combatRevision = actor:getCombatRevision()
                    local snapshot = enemySnapshots[actor]
                    if snapshot == nil then
                        snapshot = {}
                        enemySnapshots[actor] = snapshot
                        changed = true
                    elseif snapshot.x ~= position.x or snapshot.y ~= position.y
                        or snapshot.combatRevision ~= combatRevision or snapshot.infoComp ~= actor.infoComp then
                        changed = true
                    end
                    snapshot.x = position.x
                    snapshot.y = position.y
                    snapshot.combatRevision = combatRevision
                    snapshot.infoComp = actor.infoComp
                    snapshot.scanRevision = enemyScanRevision
                    enemyCount = enemyCount + 1
                    if enemies[enemyCount] ~= actor then
                        enemies[enemyCount] = actor
                        changed = true
                    end
                end
            end
        end
    end
    for enemy, snapshot in pairs(enemySnapshots) do
        if snapshot.scanRevision ~= enemyScanRevision then
            enemySnapshots[enemy] = nil
            changed = true
        end
    end
    for index = #enemies, enemyCount + 1, -1 do
        enemies[index] = nil
        changed = true
    end
    if not changed then
        return
    end

    self._player = player
    self._playerInfoComp = playerInfoComp
    self._playerCombatRevision = playerCombatRevision
    self:_rebuildEntries()
    self._revision = self._revision + 1
end

function MovementDangerState:_rebuildEntries()
    self._entries = {}
    self._entryGrid = {}
    local player = self._player
    if player == nil or not bool(self._enemies) then
        return
    end
    local position = sf.Vector2i.new(0, 0)
    ---@cast position sf.Vector2i
    for y = 0, self._mapHeight - 1 do
        ---@type table<integer, Source.SceneComponents.MovementDangerEntry>
        local row = {}
        self._entryGrid[y + 1] = row
        position.y = y
        for x = 0, self._mapWidth - 1 do
            position.x = x
            local damage, sources = MovementSpecials.CalculateDangerAtPosition(
                self._enemies, player, position, nil
            )
            if damage > 0 then
                local entryPosition = sf.Vector2i.new(x, y)
                ---@cast entryPosition sf.Vector2i
                ---@type Source.SceneComponents.MovementDangerEntry
                local entry = {
                    position = entryPosition,
                    damage = damage,
                    sources = sources
                }
                self._entries[#self._entries + 1] = entry
                row[x + 1] = entry
            end
        end
    end
end

function MovementDangerState:getRevision()
    return self._revision
end

function MovementDangerState:getEntries()
    return self._entries
end

function MovementDangerState:getDamageAt(position, ignoredEnemies)
    local row = self._entryGrid[position.y + 1]
    local entry = row ~= nil and row[position.x + 1] or nil
    if entry == nil then
        return 0
    end
    local ignoredEnemySet = nil
    if bool(ignoredEnemies) then
        ---@cast ignoredEnemies -nil
        ignoredEnemySet = {}
        for _, enemy in ipairs(ignoredEnemies) do
            ignoredEnemySet[enemy] = true
        end
    end
    return getEntryDamage(entry, ignoredEnemySet)
end

function MovementDangerState:getExcludedAnchors(goal, ignoredGoalEnemies, allowGoal)
    local result = {}
    local ignoredEnemySet = nil
    if bool(ignoredGoalEnemies) then
        ---@cast ignoredGoalEnemies -nil
        ignoredEnemySet = {}
        for _, enemy in ipairs(ignoredGoalEnemies) do
            ignoredEnemySet[enemy] = true
        end
    end
    for _, entry in ipairs(self._entries) do
        local position = entry.position
        local isGoal = position == goal
        local damage = getEntryDamage(entry, ignoredEnemySet)
        if damage > 0 and not (isGoal and allowGoal) then
            result[#result + 1] = sf.Vector2i.new(position.x, position.y)
        end
    end
    return result
end

return class(MovementDangerState, ComponentBase)
