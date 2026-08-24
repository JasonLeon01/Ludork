local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Logging = require("Global.Utils.Logging")
local Data = require("Source.Data")
local ChildActorComponent = require("Source.Components.ChildActorComponent")
local EnemyInfoComponent = require("Source.Components.EnemyInfoComponent")
---@type { Special: Source.Configs.GeneralEnum.Special, State: Source.Configs.GeneralEnum.State }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local EnemyInfo = require("Source.Infos.EnemyInfo")
local Item = require("Source.Item")
local Battler = require("Source.Battler")

local ComponentsFunctions = GlobalFunctions.Components
local Actor = Engine.Actor
local actorComponentOwner = Actor
---@cast actorComponentOwner + { _componentTypes: table<string, table> }
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
---@return table context
local function createDefeatSpawnContext(enemy, scene)
    local gameMap = scene:getGameMap()
    local layerName = assert(gameMap:getActorLayer(enemy), "Defeated enemy is not on a map layer")
    local originalTag = enemy:getMapTag()
    assert(bool(originalTag), "Defeated enemy requires a non-empty map-placement tag")
    return {
        gameMap = gameMap,
        layerName = layerName,
        originalTag = originalTag,
        position = copy(enemy:getMapPosition()),
        reservedTags = {}
    }
end

---@param context table
---@param suffix  string
---@return string
local function reserveDefeatSpawnTag(context, suffix)
    local baseTag = context.originalTag .. "_" .. suffix
    local mapTag = baseTag
    local tagSuffix = 2
    while context.gameMap:getActorByTag(mapTag) ~= nil or context.reservedTags[mapTag] do
        mapTag = baseTag .. "_" .. tostring(tagSuffix)
        tagSuffix = tagSuffix + 1
    end
    context.reservedTags[mapTag] = true
    return mapTag
end

---@param context       table
---@param blueprintPath string
---@param kind          string
---@param tagSuffix     string
---@return Engine.Actor
local function prepareDefeatSpawnActor(context, blueprintPath, kind, tagSuffix)
    assert(
        type(blueprintPath) == "string" and bool(blueprintPath), "Enemy " .. kind .. " requires a Blueprint class path"
    )
    local actor = assert(
        Data.GenActorFromClassPath(blueprintPath), "Enemy " .. kind .. " Blueprint class not found: " .. blueprintPath
    )
    actor:setMapTag(reserveDefeatSpawnTag(context, tagSuffix))
    actor:setMapPosition(copy(context.position))
    return actor
end

---@param blueprintPath string
---@param position      sf.Vector2i
---@return string
local function createDropMapTag(blueprintPath, position)
    local prefix = blueprintPath:gsub("^Data%.Blueprints%.", ""):gsub("%.", "_")
    return prefix .. "_default_" .. tostring(position.x) .. "_" .. tostring(position.y)
end

---@param context       table
---@param blueprintPath string
---@param offset        sf.Vector2i
---@return Source.Item | nil
local function prepareDropActor(context, blueprintPath, offset)
    assert(type(blueprintPath) == "string" and bool(blueprintPath), "Enemy drop requires a Blueprint class path")
    local resolvedPath = Data.ResolveClassPath(blueprintPath)
    local itemClass = assert(Data.GetClass(resolvedPath), "Enemy drop Blueprint class not found: " .. resolvedPath)
    assert(Class.isSubclass(itemClass, Item), "Enemy drop Blueprint must derive from Source.Item: " .. resolvedPath)
    local position = context.position + offset
    local mapTag = createDropMapTag(resolvedPath, position)
    if context.gameMap:getActorByTag(mapTag) ~= nil or context.reservedTags[mapTag] then
        Logging.warning(
            "Skipping enemy drop %s at (%d, %d): map tag already exists: %s", resolvedPath, position.x, position.y,
            mapTag
        )
        return nil
    end
    local actor = assert(
        Data.GenActorFromClassPath(resolvedPath), "Enemy drop Blueprint class not found: " .. resolvedPath
    )
    context.reservedTags[mapTag] = true
    actor:setMapTag(mapTag)
    actor:setMapPosition(position)
    return actor
end

---@param enemy Source.Enemy
---@param scene Source.Scenes.SceneMap.SceneMap
---@return Engine.Actor | nil rebornActor
---@return table[] droppedActors
---@return string | nil layerName
local function prepareDefeatSpawns(enemy, scene)
    ---@type string | nil
    local blueprintPath = enemy.infoComp.special[Special.Reborn]
    local drops = enemy:getDrops()
    if blueprintPath == nil and not bool(drops) then
        return nil, {}, nil
    end
    local context = createDefeatSpawnContext(enemy, scene)
    local rebornActor = nil
    if blueprintPath ~= nil then
        rebornActor = prepareDefeatSpawnActor(context, blueprintPath, "Reborn special", "Reborn")
    end
    local droppedActors = {}
    for _, dropPath in ipairs(table.orderedStringKeys(drops)) do
        local actor = prepareDropActor(context, dropPath, drops[dropPath])
        if actor ~= nil then
            droppedActors[#droppedActors + 1] = actor
        end
    end
    return rebornActor, droppedActors, context.layerName
end

---@param scene     Source.Scenes.SceneMap.SceneMap
---@param actor     Engine.Actor
---@param layerName string
local function spawnPersistentActor(scene, actor, layerName)
    scene:getGameMap():spawnActor(actor, layerName)
    scene:recordAddedActor(actor)
end

---@type function
local gameOver
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
end

function Enemy:_normaliseChildActorComp()
    if not Class.hasOwnField(self, "childActorComp") or not Class.isInstance(self.childActorComp, ChildActorComponent) then
        self.childActorComp = ComponentsFunctions.componentFromData(ChildActorComponent, self.childActorComp)
    end
end

function Enemy:battle()
    local scene = GlobalCore.System.getScene()
    ---@cast scene Source.Scenes.SceneMap.SceneMap
    local player = scene.inst:getPlayer()
    local damageType, damage = self:getDamage(player)
    ---@cast damage integer
    if damageType == DamageType.UNDEFEATABLE then
        return 1
    end
    local won = damage < player.infoComp.HP
    player.infoComp.HP = player.infoComp.HP - damage
    scene:getGameMap():addDamageText(tostring(damage), player:getPosition())
    if not won then
        return 1
    end
    return 0
end

function Enemy:afterBattle(against)
    local player = against
    for specialKey in pairs(self:getSpecial()) do
        ---@type string | nil
        local specialType = nil
        if specialKey == Special.Poisoning then
            specialType = State.Poisoned
        elseif specialKey == Special.Weaken then
            specialType = State.Weak
        end
        if specialType ~= nil then
            local stacks = self:getSpecialIntValue(specialKey)
            if stacks > 0 then
                player:addState(specialType, stacks)
            end
        end
    end
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
    for blueprintPath, offset in pairs(self.infoComp.drops) do
        result[blueprintPath] = copy(offset)
    end
    return result
end

function Enemy:getCriticalValue(battler)
    local attackerATK = battler:getATK(self)
    local enemyDEF = self:getDEF(battler)
    if attackerATK <= enemyDEF then
        return enemyDEF + 1
    end
    if attackerATK >= self.infoComp.MAXHP + enemyDEF then
        return -2
    end
    if self:hasSpecial(Special.Hard) then
        return -1
    end
    local damage = attackerATK - enemyDEF
    local turns = math.max(math.ceil(self.infoComp.MAXHP / damage) - 1, 0)
    return math.ceil(self.infoComp.MAXHP / turns) + enemyDEF
end

function Enemy:onCollision(other)
    local scene = GlobalCore.System.getScene()
    ---@cast scene Source.Scenes.SceneMap.SceneMap
    local PlayerFunctions = require("Source.NodeFunctions.Player")

    local player = PlayerFunctions.MeetPlayer(other)
    if player ~= nil and not bool(self._defeatFinalising) and (self._battleCondition == nil or self._battleCondition()) then
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
                local rewards = { self.infoComp.GOLD, self.infoComp.EXP }
                self:triggerEvent("onDefeat", nil, function ()
                    if bool(self._defeatFinalised) then
                        return
                    end
                    local rebornEnemy, droppedActors, spawnLayer = prepareDefeatSpawns(self, scene)
                    self._defeatFinalised = true
                    scene:recordDestroyedActor(self)
                    if bool(Enemy.DefeatShatterEffectEnabled) then
                        scene:getGameMap():playActorPixelShatterEffect(self)
                    end
                    self:destroy()
                    if rebornEnemy ~= nil then
                        ---@cast spawnLayer string
                        spawnPersistentActor(scene, rebornEnemy, spawnLayer)
                    end
                    for _, droppedActor in ipairs(droppedActors) do
                        ---@cast spawnLayer string
                        spawnPersistentActor(scene, droppedActor, spawnLayer)
                        droppedActor:triggerEvent("onDrop")
                    end
                    player.infoComp.GOLD = player.infoComp.GOLD + rewards[1]
                    player.infoComp.EXP = player.infoComp.EXP + rewards[2]
                    self:afterBattle(player)
                end)
            elseif result == 1 then
                player.infoComp.HP = 0
                gameOver()
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

function gameOver()
    local SceneGameOver = require("Source.Scenes.SceneGameOver")

    GlobalCore.System.setScene(SceneGameOver.new())
end

return class(Enemy, Actor, EnemyInfo, Battler)
