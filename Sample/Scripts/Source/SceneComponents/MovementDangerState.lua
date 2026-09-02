local ActorTree = require("Global.ActorTree")
local ComponentBase = require("Global.Components.ComponentBase")
local Enemy = require("Source.Enemy")
local MovementSpecials = require("Source.MovementSpecials")
local MovementDangerGrid = require("Source.SceneComponents.MovementDangerGrid")

---@class (partial) Source.SceneComponents.MovementDangerState
local MovementDangerState = {}

function MovementDangerState:init(gameMap)
    super(MovementDangerState, self).init(gameMap)
    self._player = nil
    self._playerAttributes = nil
    self._playerAbilityRevision = nil
    self._enemySnapshots = {}
    self._enemies = {}
    self._enemyScanRevision = 0
    self._entries = {}
    self._entryGrid = {}
    self._entriesValid = false
    self._entryAreaX = nil
    self._entryAreaY = nil
    self._entryAreaWidth = nil
    self._entryAreaHeight = nil
    self._previewContext = nil
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
    local playerAbilityRevision
    ---@type Source.Configs.GeneralDataTypes.PlayerAttributeSet | nil
    local playerAttributes
    if player ~= nil then
        ---@cast player Source.Player.Player
        playerAbilityRevision = player:getAbilitySystemComponent():getRevision()
        playerAttributes = player.attributes
    end
    if playerAbilityRevision ~= self._playerAbilityRevision or playerAttributes ~= self._playerAttributes then
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
                    local abilityRevision = actor:getAbilitySystemComponent():getRevision()
                    local snapshot = self._enemySnapshots[actor]
                    if snapshot == nil then
                        snapshot = {
                            x = position.x,
                            y = position.y,
                            abilityRevision = abilityRevision,
                            attributes = actor.attributes,
                            scanRevision = enemyScanRevision
                        }
                        self._enemySnapshots[actor] = snapshot
                        dangerChanged = true
                        pathfindingChanged = true
                    elseif snapshot.x ~= position.x or snapshot.y ~= position.y
                        or snapshot.abilityRevision ~= abilityRevision or snapshot.attributes ~= actor.attributes then
                        dangerChanged = true
                        pathfindingChanged = true
                    end
                    snapshot.x = position.x
                    snapshot.y = position.y
                    snapshot.abilityRevision = abilityRevision
                    snapshot.attributes = actor.attributes
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

    self._player = player
    self._playerAttributes = playerAttributes
    self._playerAbilityRevision = playerAbilityRevision
    self._areaX = area.x
    self._areaY = area.y
    self._areaWidth = area.width
    self._areaHeight = area.height
    self._entriesValid = false
    if dangerChanged then
        self._entries = {}
        self._entryGrid = {}
        self._entryAreaX = nil
        self._entryAreaY = nil
        self._entryAreaWidth = nil
        self._entryAreaHeight = nil
        self._previewContext = nil
    end
    self._previewRevision = self._previewRevision + 1
    if pathfindingChanged then
        self._pathfindingRevision = self._pathfindingRevision + 1
    end
end

function MovementDangerState:_getPreviewContext()
    if self._previewContext == nil and self._player ~= nil and bool(self._enemies) then
        self._previewContext = MovementSpecials.CreatePreviewContext(self._enemies, self._player)
    end
    return self._previewContext
end

function MovementDangerState:_ensureEntries()
    if self._entriesValid then
        return
    end
    local areaX = self._areaX
    local areaY = self._areaY
    local areaWidth = self._areaWidth
    local areaHeight = self._areaHeight
    if self._player == nil or not bool(self._enemies) then
        self._entries = {}
        self._entryGrid = {}
        self._entryAreaX = areaX
        self._entryAreaY = areaY
        self._entryAreaWidth = areaWidth
        self._entryAreaHeight = areaHeight
        self._entriesValid = true
        return
    end
    local previewContext = assert(self:_getPreviewContext())
    if self._entryAreaX == nil or self._entryAreaY == nil
        or self._entryAreaWidth == nil or self._entryAreaHeight == nil then
        self._entries, self._entryGrid = MovementDangerGrid.Build(
            self._enemies, self._player, assert(areaX), assert(areaY), assert(areaWidth), assert(areaHeight),
            previewContext
        )
    else
        self._entries, self._entryGrid = MovementDangerGrid.Refresh(
            self._enemies, self._player, assert(areaX), assert(areaY), assert(areaWidth), assert(areaHeight),
            self._entryAreaX, self._entryAreaY, self._entryAreaWidth, self._entryAreaHeight, self._entryGrid,
            previewContext
        )
    end
    self._entryAreaX = areaX
    self._entryAreaY = areaY
    self._entryAreaWidth = areaWidth
    self._entryAreaHeight = areaHeight
    self._entriesValid = true
end

function MovementDangerState:getPathfindingRevision()
    return self._pathfindingRevision
end

function MovementDangerState:getPreviewRevision()
    return self._previewRevision
end

function MovementDangerState:getEntries()
    self:_ensureEntries()
    return self._entries
end

function MovementDangerState:getDamageAt(position, ignoredEnemies)
    if not self._entriesValid then
        if self._player == nil or not bool(self._enemies) then
            return 0
        end
        local result = MovementSpecials.Preview(
            self._enemies, self._player, position, ignoredEnemies, assert(self:_getPreviewContext())
        )
        return result.data.damage
    end
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
    self:_ensureEntries()
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
