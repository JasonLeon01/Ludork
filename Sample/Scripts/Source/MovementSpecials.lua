local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local ActorTree = require("Global.ActorTree")
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult
local GameplayEventData = GlobalCore.GameplayEventData
---@type { Special: Source.Configs.GeneralEnum.Special }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local Effects = require("Source.Gameplay.Effects")
local MotaBattleAbility = require("Source.Gameplay.MotaBattleAbility")
local SpecialAbilities = require("Source.Gameplay.SpecialAbilities")

local Special = GeneralEnum.Special

local MovementSpecials = {}

---@param x integer
---@param y integer
---@return tuple<integer>
local function positionKey(x, y)
    return tuple(x, y)
end

---@param enemy          Source.Enemy
---@param playerPosition sf.Vector2i
local function doBlockadeRetreat(enemy, playerPosition)
    local enemyPosition = enemy:getMapPosition()
    local offset = sf.Vector2i.new(
        Engine.ToInteger(Engine.Clamp(enemyPosition.x - playerPosition.x, -1, 1)),
        Engine.ToInteger(Engine.Clamp(enemyPosition.y - playerPosition.y, -1, 1))
    )
    ---@cast offset sf.Vector2i
    local moved = enemy:MapMove(offset)
    local newPosition = moved and enemyPosition + offset or enemyPosition
    ---@cast newPosition sf.Vector2i
    if moved then
        local gameMap = enemy:getMap()
        ---@cast gameMap GameMap
        local scene = gameMap:getScene()
        ---@cast scene Source.Scenes.SceneMap.SceneMap
        scene:recordActorPosition(enemy, newPosition)
    end
    local NodeUtils = require("Source.NodeFunctions.Utils")

    local tag = enemy.tag or enemy.ID
    NodeUtils.SetGameVariable("Blockade_" .. tag .. "_X", newPosition.x)
    NodeUtils.SetGameVariable("Blockade_" .. tag .. "_Y", newPosition.y)
end

local function queryEnemy(enemy, player, playerPosition, previewContext)
    local enemyPosition = enemy:getMapPosition()
    local damage
    if previewContext == nil then
        damage = MotaBattleAbility.CalculateDamagePerRound(enemy, player)
    else
        assert(previewContext.player == player, "Movement preview context belongs to another player")
        damage = assert(previewContext.damageByEnemy[enemy], "Movement preview context is missing an enemy")
    end
    local eventData = GameplayEventData.new(enemy, player, "Event.Movement.QueryHazard", {
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

---@param enemies        Source.Enemy[]
---@param player         Source.Player.Player
---@param playerPosition sf.Vector2i
---@param ignoredEnemies Source.Enemy[] | nil
---@param previewContext Source.MovementSpecials.PreviewContext | nil
---@return GlobalCore.GameplayAbilityResult
function MovementSpecials.Preview(enemies, player, playerPosition, ignoredEnemies, previewContext)
    ---@type table<Source.Enemy, boolean> | nil
    local ignoredEnemySet = nil
    if bool(ignoredEnemies) then
        ---@cast ignoredEnemies - nil
        ignoredEnemySet = ActorTree.ToSet(ignoredEnemies)
    end
    local totalDamage = 0
    local sources = {}
    ---@type { enemy: Source.Enemy, damage: integer } []
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
    ---@type dict<tuple<integer>, { enemy: Source.Enemy, damage: integer }>
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
            playerPosition = copy(playerPosition)
        }))
end

local function collectEnemies(player)
    local Enemy = require("Source.Enemy")

    local gameMap = player:getMap()
    if gameMap == nil then
        return nil, {}
    end
    ---@cast gameMap GameMap
    local enemies = {}
    for _, actor in ipairs(gameMap:getAllActors()) do
        if Class.isInstance(actor, Enemy) and not actor:isDestroyed() then
            ---@cast actor Source.Enemy
            local abilitySystem = actor:getAbilitySystemComponent()
            if abilitySystem:hasMatchingGameplayTag(SpecialAbilities.MOVEMENT_HAZARD_TAG) then
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
                    playerPosition = copy(playerPosition)
                }
            end
        end
    end
    for _, retreat in ipairs(blockadeRetreats) do
        doBlockadeRetreat(retreat.enemy, retreat.playerPosition)
    end
    if totalDamage <= 0 then
        return assert(GameplayAbilityResult.Success("NoMovementHazard", { damage = 0, sources = allSources }))
    end
    local scene = gameMap:getScene()
    local animationLength = 0.0
    if scene ~= nil then
        ---@cast scene Source.Scenes.SceneMap.SceneMap
        local playerPosition = player:getPosition()
        local seenEnemies = {}
        for _, source in ipairs(allSources) do
            if not seenEnemies[source.enemy] then
                seenEnemies[source.enemy] = true
                animationLength = math.max(animationLength, source.enemy:playAttackAnimationAt(scene, playerPosition))
            end
        end
    end
    local eventData = GameplayEventData.new(
        allSources[1].enemy, player, "Event.Movement.HazardDamage", { sources = allSources }
    )
    Effects.ApplyInstantModifier(player, "Movement.HazardDamage", "HP", "Add", -totalDamage, eventData)
    gameMap:addDamageText(tostring(totalDamage), player:getPosition())
    if player.attributes.HP <= 0 then
        local function gameOver()
            local SceneGameOver = require("Source.Scenes.SceneGameOver")

            GlobalCore.System.setScene(SceneGameOver.new())
        end

        if scene == nil then
            gameOver()
        else
            scene:addTimer(animationLength, gameOver)
        end
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
