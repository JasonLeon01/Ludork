local Engine = require("Engine")
local EventKeys = require("Source.Configs.EventKeys")
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
    local moved = enemy:MapMove(offset)
    local newPosition = moved and enemyPosition + offset or enemyPosition
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
---@return integer, list<Source.Enemy>
local function checkFlankDamage(flankEnemies, player, playerPosition)
    ---@type dict<tuple<any>, Source.Enemy>
    local positionMap = dict()
    for _, enemy in ipairs(flankEnemies) do
        local enemyPosition = enemy:getMapPosition()
        local key = positionKey(enemyPosition.x - playerPosition.x, enemyPosition.y - playerPosition.y)
        positionMap[key] = enemy
    end

    local totalDamage = 0
    local attackers = list()
    local left = positionMap:get(positionKey(-1, 0))
    local right = positionMap:get(positionKey(1, 0))
    if left ~= nil and right ~= nil then
        totalDamage = totalDamage + left:getDamagePerRound(player)
        ---@cast totalDamage integer
        totalDamage = totalDamage + right:getDamagePerRound(player)
        ---@cast totalDamage integer
        attackers:append(left)
        attackers:append(right)
    end
    local up = positionMap:get(positionKey(0, -1))
    local down = positionMap:get(positionKey(0, 1))
    if up ~= nil and down ~= nil then
        totalDamage = totalDamage + up:getDamagePerRound(player)
        ---@cast totalDamage integer
        totalDamage = totalDamage + down:getDamagePerRound(player)
        ---@cast totalDamage integer
        attackers:append(up)
        attackers:append(down)
    end
    return totalDamage, attackers
end

---@param enemies        list<Source.Enemy>
---@param player         Source.Player.Player
---@param playerPosition sf.Vector2i
---@return integer, list<Source.Enemy>
local function collectSpecialsAtPosition(enemies, player, playerPosition)
    local totalDamage = 0
    local damagingEnemies = list()
    for _, enemy in ipairs(enemies) do
        local enemyPosition = enemy:getMapPosition()
        local distance = getManhattanDistance(playerPosition, enemyPosition)
        if enemy:hasSpecial(Special.Domain) then
            local domainRange = enemy:getSpecialIntValue(Special.Domain, 0, 1)
            if distance < domainRange then
                totalDamage = totalDamage + enemy:getDamagePerRound(player)
                damagingEnemies:append(enemy)
            end
        end
        if enemy:hasSpecial(Special.Blockade) and distance == 1 then
            totalDamage = totalDamage + enemy:getDamagePerRound(player)
            damagingEnemies:append(enemy)
            doBlockadeRetreat(enemy, playerPosition)
        end
    end

    local flankEnemies = list()
    for _, enemy in ipairs(enemies) do
        if enemy:hasSpecial(Special.Flank) then
            flankEnemies:append(enemy)
        end
    end
    if #flankEnemies >= 2 then
        local flankDamage, flankAttackers = checkFlankDamage(flankEnemies, player, playerPosition)
        totalDamage = totalDamage + flankDamage
        for _, enemy in ipairs(flankAttackers) do
            damagingEnemies:append(enemy)
        end
    end
    return totalDamage, damagingEnemies
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
        if Class.isInstance(actor, Enemy) then
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
        local stepDamage, stepAttackers = collectSpecialsAtPosition(enemies, player, playerPosition)
        totalDamage = totalDamage + stepDamage
        for _, enemy in ipairs(stepAttackers) do
            damagingEnemies:append(enemy)
        end
    end
    if totalDamage <= 0 then
        return
    end

    local scene = gameMap:getScene()
    if scene ~= nil then
        ---@cast scene Source.Scenes.SceneMap.SceneMap
        local playerPosition = player:getPosition()
        local seenEnemies = {}
        for _, enemy in ipairs(damagingEnemies) do
            if not seenEnemies[enemy] then
                seenEnemies[enemy] = true
                enemy:playAttackAnimationAt(scene, playerPosition)
            end
        end
    end

    player.infoComp.HP = player.infoComp.HP - totalDamage
    gameMap:addDamageText(tostring(totalDamage), player:getPosition())
    if player.infoComp.HP <= 0 then
        local SceneGameOver = require("Source.Scenes.SceneGameOver")
        local GlobalCore = require("GlobalCore")

        local GlobalSystem = GlobalCore.System
        GlobalSystem.setScene(SceneGameOver.new())
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
