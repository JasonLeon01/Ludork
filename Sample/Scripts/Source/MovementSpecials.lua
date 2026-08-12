local Engine = require("Engine")
local EventKeys = require("Source.Configs.EventKeys")
---@type { Special: Source.Configs.GeneralEnum.Special }
local GeneralEnum = require("Source.Configs.GeneralEnum")

local Special = GeneralEnum.Special

local MovementSpecials = {}
local handlersRegistered = false

---@param value number
---@return integer
local function getSign(value)
    if value > 0 then
        return 1
    elseif value < 0 then
        return -1
    end
    return 0
end

---@param a sf.Vector2i
---@param b sf.Vector2i
---@return integer
local function getManhattanDistance(a, b)
    return math.abs(a.x - b.x) + math.abs(a.y - b.y)
end

---@param x integer
---@param y integer
---@return tuple<any>
local function positionKey(x, y)
    return tuple { x, y }
end

---@param enemy          Source.Enemy
---@param playerPosition sf.Vector2i
local function doBlockadeRetreat(enemy, playerPosition)
    local enemyPosition = enemy:getMapPosition()
    local offset = sf.Vector2i.new(
        getSign(enemyPosition.x - playerPosition.x), getSign(enemyPosition.y - playerPosition.y)
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

    local SetGameVariable = NodeUtils.SetGameVariable
    local tag = enemy.tag or enemy.ID
    SetGameVariable("Blockade_" .. tostring(tag) .. "_X", newPosition.x)
    SetGameVariable("Blockade_" .. tostring(tag) .. "_Y", newPosition.y)
end

---@param flankEnemies   list<Source.Enemy>
---@param player         Source.Player.Player
---@param playerPosition sf.Vector2i
---@return integer, Source.MovementSpecials.DangerSource[]
local function checkFlankDamage(flankEnemies, player, playerPosition)
    ---@type dict<tuple<any>, Source.Enemy>
    local positionMap = dict()
    for _, enemy in ipairs(flankEnemies) do
        local enemyPosition = enemy:getMapPosition()
        local key = positionKey(enemyPosition.x - playerPosition.x, enemyPosition.y - playerPosition.y)
        positionMap[key] = enemy
    end

    local totalDamage = 0
    local sources = {}
    local left = positionMap:get(positionKey(-1, 0))
    local right = positionMap:get(positionKey(1, 0))
    if left ~= nil and right ~= nil then
        local leftDamage = left:getDamagePerRound(player)
        local rightDamage = right:getDamagePerRound(player)
        totalDamage = totalDamage + leftDamage
        ---@cast totalDamage integer
        totalDamage = totalDamage + rightDamage
        ---@cast totalDamage integer
        sources[#sources + 1] = {
            enemy = left,
            special = Special.Flank,
            damage = leftDamage
        }
        sources[#sources + 1] = {
            enemy = right,
            special = Special.Flank,
            damage = rightDamage
        }
    end
    local up = positionMap:get(positionKey(0, -1))
    local down = positionMap:get(positionKey(0, 1))
    if up ~= nil and down ~= nil then
        local upDamage = up:getDamagePerRound(player)
        local downDamage = down:getDamagePerRound(player)
        totalDamage = totalDamage + upDamage
        ---@cast totalDamage integer
        totalDamage = totalDamage + downDamage
        ---@cast totalDamage integer
        sources[#sources + 1] = {
            enemy = up,
            special = Special.Flank,
            damage = upDamage
        }
        sources[#sources + 1] = {
            enemy = down,
            special = Special.Flank,
            damage = downDamage
        }
    end
    return totalDamage, sources
end

---@param enemies        list<Source.Enemy>
---@param player         Source.Player.Player
---@param playerPosition sf.Vector2i
---@param ignoredEnemies Source.Enemy[] | nil
---@param applyBlockade  boolean
---@return integer, Source.MovementSpecials.DangerSource[]
local function calculateDangerAtPosition(enemies, player, playerPosition, ignoredEnemies, applyBlockade)
    local ignoredEnemySet = nil
    if bool(ignoredEnemies) then
        ---@cast ignoredEnemies -nil
        ignoredEnemySet = {}
        for _, enemy in ipairs(ignoredEnemies) do
            ignoredEnemySet[enemy] = true
        end
    end
    local totalDamage = 0
    local sources = {}
    for _, enemy in ipairs(enemies) do
        local enemyPosition = enemy:getMapPosition()
        local distance = getManhattanDistance(playerPosition, enemyPosition)
        if enemy:hasSpecial(Special.Domain)
            and (ignoredEnemySet == nil or not ignoredEnemySet[enemy]) then
            local domainRange = enemy:getSpecialIntValue(Special.Domain, 0, 1)
            if distance < domainRange then
                local damage = enemy:getDamagePerRound(player)
                ---@cast damage integer
                totalDamage = totalDamage + damage
                sources[#sources + 1] = {
                    enemy = enemy,
                    special = Special.Domain,
                    damage = damage
                }
            end
        end
        if enemy:hasSpecial(Special.Blockade) and distance == 1
            and (ignoredEnemySet == nil or not ignoredEnemySet[enemy]) then
            local damage = enemy:getDamagePerRound(player)
            ---@cast damage integer
            totalDamage = totalDamage + damage
            sources[#sources + 1] = {
                enemy = enemy,
                special = Special.Blockade,
                damage = damage
            }
            if applyBlockade then
                doBlockadeRetreat(enemy, playerPosition)
            end
        end
    end

    local flankEnemies = list()
    for _, enemy in ipairs(enemies) do
        if enemy:hasSpecial(Special.Flank) then
            flankEnemies:append(enemy)
        end
    end
    if #flankEnemies >= 2 then
        local flankDamage, flankSources = checkFlankDamage(flankEnemies, player, playerPosition)
        if ignoredEnemySet == nil then
            totalDamage = totalDamage + flankDamage
        end
        for _, source in ipairs(flankSources) do
            if ignoredEnemySet == nil or not ignoredEnemySet[source.enemy] then
                if ignoredEnemySet ~= nil then
                    totalDamage = totalDamage + source.damage
                end
                sources[#sources + 1] = source
            end
        end
    end
    return totalDamage, sources
end

function MovementSpecials.CalculateDangerAtPosition(enemies, player, playerPosition, ignoredEnemies)
    return calculateDangerAtPosition(enemies, player, playerPosition, ignoredEnemies, false)
end

---@param player        Source.Player.Player
---@param pathPositions sf.Vector2i[]
local function applyMovementSpecials(player, pathPositions)
    local Enemy = require("Source.Enemy")

    local gameMap = player:getMap()
    if gameMap == nil then
        return
    end
    ---@cast gameMap GameMap
    local enemies = list()
    for _, actor in ipairs(gameMap:getAllActors()) do
        if Class.isInstance(actor, Enemy) and not actor:isDestroyed() then
            ---@cast actor Source.Enemy
            if bool(actor:getSpecial()) then
                enemies:append(actor)
            end
        end
    end
    if not bool(enemies) then
        return
    end

    local totalDamage = 0
    local damagingEnemies = list()
    for _, playerPosition in ipairs(pathPositions) do
        local stepDamage, stepSources = calculateDangerAtPosition(enemies, player, playerPosition, nil, true)
        totalDamage = totalDamage + stepDamage
        for _, source in ipairs(stepSources) do
            damagingEnemies:append(source.enemy)
        end
    end
    if totalDamage <= 0 then
        return
    end

    local scene = gameMap:getScene()
    local animationLength = 0.0
    if scene ~= nil then
        ---@cast scene Source.Scenes.SceneMap.SceneMap
        local playerPosition = player:getPosition()
        local seenEnemies = {}
        for _, enemy in ipairs(damagingEnemies) do
            if not seenEnemies[enemy] then
                seenEnemies[enemy] = true
                animationLength = math.max(
                    animationLength, enemy:playAttackAnimationAt(scene, playerPosition)
                )
            end
        end
    end

    player.infoComp.HP = player.infoComp.HP - totalDamage
    gameMap:addDamageText(tostring(totalDamage), player:getPosition())
    if player.infoComp.HP <= 0 then
        local function gameOver()
            local SceneGameOver = require("Source.Scenes.SceneGameOver")
            local GlobalCore = require("GlobalCore")

            local GlobalSystem = GlobalCore.System
            GlobalSystem.setScene(SceneGameOver.new())
        end

        if scene == nil then
            gameOver()
        else
            scene:addTimer(animationLength, gameOver)
        end
    end
end

function MovementSpecials.registerHandlers()
    if handlersRegistered then
        return
    end
    Engine.subscribe(EventKeys.PlayerMovementFinished, function (payload)
        local player = payload.player
        if player == nil then
            return
        end
        local pathPositions = payload.pathPositions
        if pathPositions == nil then
            pathPositions = player:consumeMovementSpecialPath()
        else
            player:consumeMovementSpecialPath()
        end
        if not bool(pathPositions) then
            pathPositions = { player:getMapPosition() }
        end
        applyMovementSpecials(player, pathPositions)
    end)
    handlersRegistered = true
end

function MovementSpecials.notifyPlayerMovementFinished(player, pathPositions)
    MovementSpecials.registerHandlers()
    Engine.publish(EventKeys.PlayerMovementFinished, {
        player = player,
        pathPositions = pathPositions
    })
end

return MovementSpecials
