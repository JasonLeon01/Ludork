local ActorTree = require("Global.ActorTree")
local ComponentBase = require("Global.Components.ComponentBase")
local Enemy = require("Source.Enemy")
local MovementDangerGrid = require("Source.SceneComponents.MovementDangerGrid")

---@class (partial) Source.SceneComponents.MovementDangerState
local MovementDangerState = {}

function MovementDangerState:init(gameMap)
    super(MovementDangerState, self).init(gameMap)
    self._player = nil
    self._playerInfoComp = nil
    self._playerCombatRevision = nil
    self._enemySnapshots = {}
    self._enemies = {}
    self._enemyScanRevision = 0
    self._entries = {}
    self._entryGrid = {}
    self._pathfindingRevision = 0
    self._previewRevision = 0
    self._areaX = nil
    self._areaY = nil
    self._areaWidth = nil
    self._areaHeight = nil
end

function MovementDangerState:onTick(_deltaTime)
    ---@cast self._parent GameMap
    local area = self._parent:_getGameplayCellRect()
    local player = self._parent:getPlayer()
    local pathfindingChanged = player ~= self._player
    local dangerChanged = pathfindingChanged
    local areaChanged = self._areaX ~= area.x or self._areaY ~= area.y
        or self._areaWidth ~= area.width or self._areaHeight ~= area.height
    ---@type integer | nil
    local playerCombatRevision
    ---@type Source.Components.PlayerInfoComponent | nil
    local playerInfoComp
    if player ~= nil then
        ---@cast player Source.Player.Player
        playerCombatRevision = player:getCombatRevision()
        playerInfoComp = player.infoComp
    end
    if playerCombatRevision ~= self._playerCombatRevision or playerInfoComp ~= self._playerInfoComp then
        dangerChanged = true
        pathfindingChanged = true
    end

    local enemyCount = 0
    local enemyScanRevision = self._enemyScanRevision + 1
    self._enemyScanRevision = enemyScanRevision
    if player ~= nil then
        ---@cast player Source.Player.Player
        for _, actorList in pairs(self._parent._actors) do
            for _, actor in ipairs(actorList) do
                if Class.isInstance(actor, Enemy) and not actor:isDestroyed()
                    and MovementDangerGrid.HasMovementSpecial(actor) then
                    ---@cast actor Source.Enemy
                    local position = actor:getMapPosition()
                    local combatRevision = actor:getCombatRevision()
                    local snapshot = self._enemySnapshots[actor]
                    if snapshot == nil then
                        snapshot = {
                            x = position.x,
                            y = position.y,
                            combatRevision = combatRevision,
                            infoComp = actor.infoComp,
                            scanRevision = enemyScanRevision
                        }
                        self._enemySnapshots[actor] = snapshot
                        dangerChanged = true
                        pathfindingChanged = true
                    elseif snapshot.x ~= position.x or snapshot.y ~= position.y
                        or snapshot.combatRevision ~= combatRevision or snapshot.infoComp ~= actor.infoComp then
                        dangerChanged = true
                        pathfindingChanged = true
                    end
                    snapshot.x = position.x
                    snapshot.y = position.y
                    snapshot.combatRevision = combatRevision
                    snapshot.infoComp = actor.infoComp
                    snapshot.scanRevision = enemyScanRevision
                    enemyCount = enemyCount + 1
                    if self._enemies[enemyCount] ~= actor then
                        self._enemies[enemyCount] = actor
                        dangerChanged = true
                        pathfindingChanged = true
                    end
                end
            end
        end
    end
    for enemy, snapshot in pairs(self._enemySnapshots) do
        if snapshot.scanRevision ~= enemyScanRevision then
            self._enemySnapshots[enemy] = nil
            dangerChanged = true
            pathfindingChanged = true
        end
    end
    for index = #self._enemies, enemyCount + 1, -1 do
        self._enemies[index] = nil
        dangerChanged = true
        pathfindingChanged = true
    end
    if not dangerChanged and not areaChanged then
        return
    end

    local previousAreaX = self._areaX
    local previousAreaY = self._areaY
    local previousAreaWidth = self._areaWidth
    local previousAreaHeight = self._areaHeight
    self._player = player
    self._playerInfoComp = playerInfoComp
    self._playerCombatRevision = playerCombatRevision
    self._areaX = area.x
    self._areaY = area.y
    self._areaWidth = area.width
    self._areaHeight = area.height
    if dangerChanged or previousAreaX == nil
        or previousAreaY == nil or previousAreaWidth == nil
        or previousAreaHeight == nil then
        self:_rebuildEntries()
    else
        self:_refreshEntriesForArea(previousAreaX, previousAreaY, previousAreaWidth, previousAreaHeight)
    end
    self._previewRevision = self._previewRevision + 1
    if pathfindingChanged then
        self._pathfindingRevision = self._pathfindingRevision + 1
    end
end

function MovementDangerState:_rebuildEntries()
    self._entries = {}
    self._entryGrid = {}
    if self._player == nil or not bool(self._enemies) then
        return
    end
    self._entries, self._entryGrid = MovementDangerGrid.Build(
        self._enemies, self._player, assert(self._areaX), assert(self._areaY), assert(self._areaWidth),
        assert(self._areaHeight)
    )
end

---@param previousAreaX      integer
---@param previousAreaY      integer
---@param previousAreaWidth  integer
---@param previousAreaHeight integer
function MovementDangerState:_refreshEntriesForArea(previousAreaX, previousAreaY, previousAreaWidth, previousAreaHeight)
    local previousGrid = self._entryGrid
    self._entries = {}
    self._entryGrid = {}
    if self._player == nil or not bool(self._enemies) then
        return
    end
    self._entries, self._entryGrid = MovementDangerGrid.Refresh(
        self._enemies, self._player, assert(self._areaX), assert(self._areaY), assert(self._areaWidth),
        assert(self._areaHeight), previousAreaX, previousAreaY, previousAreaWidth, previousAreaHeight, previousGrid
    )
end

function MovementDangerState:getPathfindingRevision()
    return self._pathfindingRevision
end

function MovementDangerState:getPreviewRevision()
    return self._previewRevision
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
        ignoredEnemySet = ActorTree.ToSet(ignoredEnemies)
    end
    return MovementDangerGrid.GetEntryDamage(entry, ignoredEnemySet)
end

function MovementDangerState:getExcludedAnchors(goal, ignoredGoalEnemies, allowGoal)
    local result = {}
    local ignoredEnemySet = nil
    if bool(ignoredGoalEnemies) then
        ---@cast ignoredGoalEnemies - nil
        ignoredEnemySet = ActorTree.ToSet(ignoredGoalEnemies)
    end
    for _, entry in ipairs(self._entries) do
        local position = entry.position
        local isGoal = position == goal
        local damage = MovementDangerGrid.GetEntryDamage(entry, ignoredEnemySet)
        if damage > 0 and not (isGoal and allowGoal) then
            result[#result + 1] = copy(position)
        end
    end
    return result
end

return class(MovementDangerState, ComponentBase)
