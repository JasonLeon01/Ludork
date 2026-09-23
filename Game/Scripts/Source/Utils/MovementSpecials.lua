local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local ActorTree = require("Global.ActorTree")
---@type { Special: Source.Configs.GeneralEnum.Special }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local Effects = require("Source.Gameplay.Effects")
local MotaBattleAbility = require("Source.Gameplay.MotaBattleAbility")
local GameplayScene = require("Source.Gameplay.GameplayScene")
local GameplayConstants = require("Source.Configs.GameplayConstants")

local GameplayAbilityResult = GlobalCore.GameplayAbilityResult
local GameplayEventData = GlobalCore.GameplayEventData
local Special = GeneralEnum.Special

local MovementSpecials = {}

---@param x integer
---@param y integer
---@return tuple<integer>
local function positionKey(x, y)
    return tuple(x, y)
end

---@param enemy          Source.MapActors.Enemy
---@param playerPosition sf.Vector2i
---@param scene          Source.Gameplay.GameplayScene
local function doBlockadeRetreat(enemy, playerPosition, scene)
    local enemyPosition = enemy:getMapPosition()
    local offset = sf.Vector2i.new(
        math.sign(enemyPosition.x - playerPosition.x), math.sign(enemyPosition.y - playerPosition.y)
    )
    ---@cast offset sf.Vector2i
    local moved = enemy:MapMove(offset)
    local newPosition = moved and enemyPosition + offset or enemyPosition
    ---@cast newPosition sf.Vector2i
    if moved then
        scene:recordActorPosition(enemy, newPosition)
    end
    local instance = scene:getGameInstance()
    local tag = enemy.tag or enemy.ID
    instance:setVariable("Blockade_" .. tag .. "_X", newPosition.x)
    instance:setVariable("Blockade_" .. tag .. "_Y", newPosition.y)
end

local function queryEnemy(enemy, player, playerPosition, previewContext)
    if enemy:isDestroyed() or not enemy:isVisibleInHierarchy() then
        return {}
    end
    local enemyPosition = enemy:getMapPosition()
    local damage
    if previewContext == nil then
        damage = MotaBattleAbility.CalculateDamagePerRound(enemy, player)
    else
        assert(previewContext.player == player, "Movement preview context belongs to another player")
        damage = assert(previewContext.damageByEnemy[enemy], "Movement preview context is missing an enemy")
    end
    local eventData = GameplayEventData.new(enemy, player, GameplayConstants.MOVEMENT_QUERY_HAZARD_EVENT, {
        distance = Engine.ManhattanDistance(playerPosition, enemyPosition),
        damagePerRound = damage,
        playerPosition = playerPosition,
        enemyPosition = enemyPosition
    })
    return enemy:getAbilitySystemComponent():handleGameplayEvent(eventData)
end

function MovementSpecials.CreatePreviewContext(enemies, player)
    local damageByEnemy = {}
    for _, enemy in ipairs(enemies) do
        damageByEnemy[enemy] = MotaBattleAbility.CalculateDamagePerRound(enemy, player)
    end
    return { player = player, damageByEnemy = damageByEnemy }
end

---@param enemies        Source.MapActors.Enemy[]
---@param player         Source.MapActors.Player.Player
---@param playerPosition sf.Vector2i
---@param ignoredEnemies Source.MapActors.Enemy[] | nil
---@param previewContext Source.Utils.MovementSpecials.PreviewContext | nil
---@return GlobalCore.GameplayAbilityResult
function MovementSpecials.Preview(enemies, player, playerPosition, ignoredEnemies, previewContext)
    ---@type table<Source.MapActors.Enemy, boolean> | nil
    local ignoredEnemySet = nil
    if bool(ignoredEnemies) then
        ---@cast ignoredEnemies - nil
        ignoredEnemySet = ActorTree.ToSet(ignoredEnemies)
    end
    local totalDamage = 0
    local sources = {}
    ---@type { enemy: Source.MapActors.Enemy, damage: integer } []
    local flankEnemies = {}
    for _, enemy in ipairs(enemies) do
        for _, result in ipairs(queryEnemy(enemy, player, playerPosition, previewContext)) do
            if result.code == "MovementHazard" and result.data.active then
                if result.data.special == Special.Flank then
                    flankEnemies[#flankEnemies + 1] = { enemy = enemy, damage = result.data.damage }
                elseif ignoredEnemySet == nil or not ignoredEnemySet[enemy] then
                    totalDamage = totalDamage + result.data.damage
                    sources[#sources + 1] = {
                        enemy = enemy,
                        special = result.data.special,
                        damage = result.data.damage
                    }
                end
            end
        end
    end
    ---@type dict<tuple<integer>, { enemy: Source.MapActors.Enemy, damage: integer }>
    local positionMap = dict()
    for _, candidate in ipairs(flankEnemies) do
        local enemyPosition = candidate.enemy:getMapPosition()
        local offsetX = enemyPosition.x - playerPosition.x
        local offsetY = enemyPosition.y - playerPosition.y
        ---@cast offsetX integer
        ---@cast offsetY integer
        local key = positionKey(offsetX, offsetY)
        positionMap[key] = candidate
    end
    local flankPairs = { { positionKey(-1, 0), positionKey(1, 0) }, { positionKey(0, -1), positionKey(0, 1) } }
    for _, pair in ipairs(flankPairs) do
        local first = positionMap:get(pair[1])
        local second = positionMap:get(pair[2])
        if first ~= nil and second ~= nil then
            for _, candidate in ipairs({ first, second }) do
                if ignoredEnemySet == nil or not ignoredEnemySet[candidate.enemy] then
                    totalDamage = totalDamage + candidate.damage
                    sources[#sources + 1] = {
                        enemy = candidate.enemy,
                        special = Special.Flank,
                        damage = candidate.damage
                    }
                end
            end
        end
    end
    return assert(GameplayAbilityResult.Success("MovementHazard", {
            damage = totalDamage,
            sources = sources,
            playerPosition = playerPosition:copy()
        }))
end

local function collectEnemies(player)
    local Enemy = require("Source.MapActors.Enemy")

    local gameMap = player:getMap()
    if gameMap == nil then
        return nil, {}
    end
    ---@cast gameMap GameMap
    local enemies = {}
    for _, actor in ipairs(gameMap:getAllActors()) do
        if Class.isInstance(actor, Enemy) and not actor:isDestroyed() and actor:isVisibleInHierarchy() then
            ---@cast actor Source.MapActors.Enemy
            local abilitySystem = actor:getAbilitySystemComponent()
            if abilitySystem:hasMatchingGameplayTag(GameplayConstants.MOVEMENT_HAZARD_TAG) then
                enemies[#enemies + 1] = actor
            end
        end
    end
    return gameMap, enemies
end

function MovementSpecials.Commit(player, pathPositions)
    local gameMap, enemies = collectEnemies(player)
    if gameMap == nil or not bool(enemies) then
        return assert(GameplayAbilityResult.Success("NoMovementHazard", { damage = 0, sources = {} }))
    end
    local totalDamage = 0
    local allSources = {}
    local blockadeRetreats = {}
    local previewContext = MovementSpecials.CreatePreviewContext(enemies, player)
    for _, playerPosition in ipairs(pathPositions) do
        local result = MovementSpecials.Preview(enemies, player, playerPosition, nil, previewContext)
        totalDamage = totalDamage + result.data.damage
        for _, source in ipairs(result.data.sources) do
            allSources[#allSources + 1] = source
            if source.special == Special.Blockade then
                blockadeRetreats[#blockadeRetreats + 1] = {
                    enemy = source.enemy,
                    playerPosition = playerPosition:copy()
                }
            end
        end
    end
    if totalDamage <= 0 and not bool(blockadeRetreats) then
        return assert(GameplayAbilityResult.Success("NoMovementHazard", { damage = 0, sources = allSources }))
    end
    local scene = gameMap:getScene()
    assert(
        Class.isInstance(scene, GameplayScene), "Movement hazard settlement requires a GameplayScene on its owning map"
    )
    ---@cast scene Source.Gameplay.GameplayScene
    for _, retreat in ipairs(blockadeRetreats) do
        doBlockadeRetreat(retreat.enemy, retreat.playerPosition, scene)
    end
    if totalDamage <= 0 then
        return assert(GameplayAbilityResult.Success("NoMovementHazard", { damage = 0, sources = allSources }))
    end
    local animationLength = 0.0
    local playerPosition = player:getPosition()
    local seenEnemies = {}
    for _, source in ipairs(allSources) do
        if not seenEnemies[source.enemy] then
            seenEnemies[source.enemy] = true
            local animation = source.enemy:playAttackAnimationAt(scene, playerPosition)
            if animation ~= nil then
                animationLength = math.max(animationLength, animation:getVisualDuration())
            end
        end
    end
    local eventData = GameplayEventData.new(
        allSources[1].enemy, player, "Event.Movement.HazardDamage", { sources = allSources }
    )
    Effects.ApplyInstantModifier(player, "Movement.HazardDamage", "HP", "Add", -totalDamage, eventData)
    gameMap:addDamageText(tostring(totalDamage), player:getPosition(), player)
    if player.attributes.HP <= 0 then
        scene:requestGameOver(player, animationLength)
    end
    return assert(GameplayAbilityResult.Success("MovementHazardCommitted", {
            damage = totalDamage,
            sources = allSources
        }))
end

function MovementSpecials.NotifyPlayerMovementFinished(player, pathPositions)
    if not bool(pathPositions) then
        pathPositions = { player:getMapPosition() }
    end
    return MovementSpecials.Commit(player, pathPositions)
end

return MovementSpecials
