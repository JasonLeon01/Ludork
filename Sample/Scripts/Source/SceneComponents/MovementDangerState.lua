local ComponentBase = require("Global.Components.ComponentBase")
local Enemy = require("Source.Enemy")
---@type { Special: Source.Configs.GeneralEnum.Special }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local MovementSpecials = require("Source.MovementSpecials")

local Special = GeneralEnum.Special

---@class (partial) Source.SceneComponents.MovementDangerState
local MovementDangerState = {}

---@param enemy Source.Enemy
---@return boolean
local function hasMovementSpecial(enemy)
    return enemy:hasSpecial(Special.Domain) or enemy:hasSpecial(Special.Flank) or enemy:hasSpecial(Special.Blockade)
end

---@param entry           Source.SceneComponents.MovementDangerEntry
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
    self._player = nil
    self._playerInfoComp = nil
    self._playerCombatRevision = nil
    self._enemySnapshots = {}
    self._enemies = {}
    self._enemyScanRevision = 0
    self._entries = {}
    self._entryGrid = {}
    self._revision = 0
    self._mapWidth = size.x
    self._mapHeight = size.y
end

function MovementDangerState:onTick(_deltaTime)
    ---@cast self._parent GameMap
    local player = self._parent:getPlayer()
    local changed = player ~= self._player
    local playerCombatRevision = nil
    local playerInfoComp = nil
    ---@cast playerCombatRevision integer | nil
    ---@cast playerInfoComp Source.Components.PlayerInfoComponent | nil
    if player ~= nil then
        ---@cast player Source.Player.Player
        playerCombatRevision = player:getCombatRevision()
        playerInfoComp = player.infoComp
    end
    if playerCombatRevision ~= self._playerCombatRevision or playerInfoComp ~= self._playerInfoComp then
        changed = true
    end

    local enemyCount = 0
    local enemyScanRevision = self._enemyScanRevision + 1
    self._enemyScanRevision = enemyScanRevision
    if player ~= nil then
        ---@cast player Source.Player.Player
        for _, actorList in pairs(self._parent._actors) do
            for _, actor in ipairs(actorList) do
                if Class.isInstance(actor, Enemy) and not actor:isDestroyed() and hasMovementSpecial(actor) then
                    ---@cast actor Source.Enemy
                    local position = actor:getMapPosition()
                    local combatRevision = actor:getCombatRevision()
                    if self._enemySnapshots[actor] == nil then
                        self._enemySnapshots[actor] = {}
                        changed = true
                    elseif self._enemySnapshots[actor].x ~= position.x or self._enemySnapshots[actor].y ~= position.y
                        or self._enemySnapshots[actor].combatRevision ~= combatRevision
                        or self._enemySnapshots[actor].infoComp ~= actor.infoComp then
                        changed = true
                    end
                    self._enemySnapshots[actor].x = position.x
                    self._enemySnapshots[actor].y = position.y
                    self._enemySnapshots[actor].combatRevision = combatRevision
                    self._enemySnapshots[actor].infoComp = actor.infoComp
                    self._enemySnapshots[actor].scanRevision = enemyScanRevision
                    enemyCount = enemyCount + 1
                    if self._enemies[enemyCount] ~= actor then
                        self._enemies[enemyCount] = actor
                        changed = true
                    end
                end
            end
        end
    end
    for enemy, snapshot in pairs(self._enemySnapshots) do
        if snapshot.scanRevision ~= enemyScanRevision then
            self._enemySnapshots[enemy] = nil
            changed = true
        end
    end
    for index = #self._enemies, enemyCount + 1, -1 do
        self._enemies[index] = nil
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
    if self._player == nil or not bool(self._enemies) then
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
                self._enemies, self._player, position, nil
            )
            if damage > 0 then
                local entryPosition = sf.Vector2i.new(x, y)
                ---@cast entryPosition sf.Vector2i
                ---@type Source.SceneComponents.MovementDangerEntry
                local entry = { position = entryPosition, damage = damage, sources = sources }
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
    local entry = self._entryGrid[position.y + 1] ~= nil and self._entryGrid[position.y + 1][position.x + 1] or nil
    if entry == nil then
        return 0
    end
    local ignoredEnemySet = nil
    if bool(ignoredEnemies) then
        ---@cast ignoredEnemies - nil
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
        ---@cast ignoredGoalEnemies - nil
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
            result[#result + 1] = copy(position)
        end
    end
    return result
end

return class(MovementDangerState, ComponentBase)
