local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Data = require("Source.Data")
local ChildActorComponent = require("Source.Components.ChildActorComponent")
local EnemyInfoComponent = require("Source.Components.EnemyInfoComponent")
---@type { Special: Source.Configs.GeneralEnum.Special, State: Source.Configs.GeneralEnum.State }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local EnemyInfo = require("Source.Infos.EnemyInfo")
local Battler = require("Source.Battler")

local ComponentsFunctions = GlobalFunctions.Components
local Actor = Engine.Actor
local actorComponentOwner = Actor
---@cast actorComponentOwner +{ _componentTypes: table<string, table> }
local Special = GeneralEnum.Special
local State = GeneralEnum.State
local DamageType = Battler.DamageType

local componentTypes = {}
for name, componentType in pairs(actorComponentOwner._componentTypes or {}) do
    componentTypes[name] = componentType
end
componentTypes.childActorComp = ChildActorComponent
componentTypes.infoComp = EnemyInfoComponent

---@param enemy Source.Enemy
---@param scene Source.Scenes.SceneMap.SceneMap
---@return Engine.Actor | nil actor
---@return string | nil layerName
local function createRebornEnemy(enemy, scene)
    local blueprintPath = enemy.infoComp.special[Special.Reborn]
    if blueprintPath == nil then
        return nil, nil
    end
    assert(type(blueprintPath) == "string" and bool(blueprintPath),
        "Enemy Reborn special requires a Blueprint class path")
    local gameMap = scene:getGameMap()
    local layerName = assert(gameMap:getActorLayer(enemy), "Reborn enemy is not on a map layer")
    local originalTag = enemy:getMapTag()
    assert(bool(originalTag), "Reborn enemy requires a non-empty map-placement tag")
    local mapTag = originalTag .. "_Reborn"
    local tagSuffix = 2
    while gameMap:getActorByTag(mapTag) ~= nil do
        mapTag = originalTag .. "_Reborn_" .. tostring(tagSuffix)
        tagSuffix = tagSuffix + 1
    end
    local actor = assert(Data.genActorFromClassPath(blueprintPath),
        "Reborn enemy Blueprint class not found: " .. blueprintPath)
    actor:setMapTag(mapTag)
    actor:setMapPosition(copy(enemy:getMapPosition()))
    return actor, layerName
end

---@class Source.Enemy
local Enemy = {}

Enemy.ID = "FILL_IT_BY_YOURSELF"
Enemy.DefeatShatterEffectEnabled = true
Enemy._componentTypes = componentTypes
Enemy.infoComp = EnemyInfoComponent.new()
Enemy.childActorComp = ChildActorComponent.new({
    className = "Source.EnemyDamageText.EnemyDamageText",
    relativePosition = sf.Vector2f.new(0.0, 0.0)
})
Enemy.tickable = true
Enemy.collisionEnabled = true
Enemy.animatable = true
Enemy.animateWithoutMoving = true
Enemy.afterBattleVarChanges = {}

function Enemy:init(texture, rect, tag)
    Actor.init(self, texture, rect, tag)
    self:_normaliseChildActorComp()
    Battler.init(self)
    self._battleCondition = nil
    self._defeatFinalising = false
    self._defeatFinalised = false
    self:initInfo(Data)
    self:_syncInitialHP()
end

function Enemy:_normaliseChildActorComp()
    self.childActorComp = ComponentsFunctions.componentFromData(ChildActorComponent, self.childActorComp)
end

function Enemy:battle()
    local scene = GlobalCore.System.getScene()
    ---@cast scene Source.Scenes.SceneMap.SceneMap
    local player = scene.inst:getPlayer()
    local damageType, damage = self:getDamage(player)
    if damageType == DamageType.UNDEFEATABLE then
        return 1
    end
    local won = damage < player.infoComp.HP
    player.infoComp.HP = Engine.ToInteger(player.infoComp.HP - damage)
    scene:getGameMap():addDamageText(tostring(damage), player:getPosition())
    if not won then
        return 1
    end
    return 0
end

function Enemy:afterBattle(against)
    local player = against
    for specialKey, stackValue in pairs(self:getSpecial()) do
        ---@type string | nil
        local specialType = nil
        if specialKey == Special.Poisoning then
            specialType = State.Poisoned
        elseif specialKey == Special.Weaken then
            specialType = State.Weak
        end
        if specialType ~= nil then
            local stacks = Enemy._resolveSpecialStacks(stackValue)
            if stacks > 0 then
                player:addState(specialType, stacks)
            end
        end
    end
end

---@param stackValue boolean | string | number
---@return integer
function Enemy._resolveSpecialStacks(stackValue)
    local resolved = stackValue
    if type(stackValue) == "string" then
        resolved = Engine.Eval(stackValue)
    end
    local value = tonumber(resolved)
    if value == nil then
        return 0
    end
    return math.max(0, math.tointeger(value) or math.floor(value))
end

function Enemy:getSpecial()
    local result = {}
    for key, value in pairs(self.infoComp.special) do
        result[key] = value
    end
    return result
end

function Enemy:getDrops()
    local result = {}
    for index, value in ipairs(self.infoComp.drops or {}) do
        result[index] = value
    end
    return result
end

function Enemy:getCriticalValue(battler)
    self:_normaliseInfoComp()
    battler:normaliseInfoComp()
    local attackerATK = Engine.ToInteger(battler:getATK(self))
    local enemyDEF = Engine.ToInteger(self:getDEF(battler))
    local enemyHP = Engine.ToInteger(self.infoComp.MAXHP)
    if attackerATK <= enemyDEF then
        return enemyDEF + 1
    end
    if attackerATK >= enemyHP + enemyDEF then
        return -2
    end
    if self:hasSpecial(Special.Hard) then
        return -1
    end
    local damage = attackerATK - enemyDEF
    local turns = math.max(math.ceil(enemyHP / damage) - 1, 0)
    return math.ceil(enemyHP / turns) + enemyDEF
end

function Enemy:onCollision(other)
    local scene = GlobalCore.System.getScene()
    ---@cast scene Source.Scenes.SceneMap.SceneMap
    local PlayerFunctions = require("Source.NodeFunctions.Player")

    local player = PlayerFunctions.MeetPlayer(other)
    if player ~= nil and not bool(self._defeatFinalising)
        and (self._battleCondition == nil or self._battleCondition()) then
        self._battleCondition = nil
        local result = self:battle()
        local animationLength = math.max(
            player:playAttackAnimationAt(scene, self:getPosition()),
            self:playAttackAnimationAt(scene, player:getPosition())
        )
        self._battleCondition = scene:addTimer(animationLength, function ()
            self._battleCondition = nil
            if result == 0 then
                self._defeatFinalising = true
                local gold = self.infoComp.GOLD
                local exp = self.infoComp.EXP
                self:triggerEvent("onDefeat", nil, function ()
                    if bool(self._defeatFinalised) then
                        return
                    end
                    local rebornEnemy, rebornLayer = createRebornEnemy(self, scene)
                    self._defeatFinalised = true
                    scene:recordDestroyedActor(self)
                    if bool(Enemy.DefeatShatterEffectEnabled) then
                        scene:getGameMap():playActorPixelShatterEffect(self)
                    end
                    self:destroy()
                    if rebornEnemy ~= nil then
                        ---@cast rebornLayer string
                        scene:getGameMap():spawnActor(rebornEnemy, rebornLayer)
                        scene:recordAddedActor(rebornEnemy)
                    end
                    player.infoComp.GOLD = player.infoComp.GOLD + gold
                    player.infoComp.EXP = player.infoComp.EXP + exp
                    self:afterBattle(player)
                end)
            elseif result == 1 then
                player.infoComp.HP = 0
                Enemy._gameOver()
            end
        end)
    end
end

function Enemy:onDefeat()
    if not bool(self.afterBattleVarChanges) then
        return
    end
    local Utils = require("Source.NodeFunctions.Utils")

    for key, operation in pairs(self.afterBattleVarChanges) do
        local operator = operation[1]
        local value = operation[2]
        local defaultValue = operator == "=" and nil or 0
        local originValue = Utils.GetGameVariable(key, defaultValue)
        local newValue = value
        if operator == "+" then
            newValue = originValue + value
        elseif operator == "-" then
            newValue = originValue - value
        elseif operator == "*" then
            newValue = originValue * value
        elseif operator == "/" then
            newValue = originValue / value
        elseif operator == "//" then
            newValue = originValue // value
        elseif operator == "%" then
            newValue = originValue % value
        elseif operator == "**" then
            newValue = originValue ^ value
        end
        Utils.SetGameVariable(key, newValue)
    end
end

function Enemy._gameOver()
    local SceneGameOver = require("Source.Scenes.SceneGameOver")

    GlobalCore.System.setScene(SceneGameOver.new())
end

return class(Enemy, Actor, EnemyInfo, Battler)
