local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Data = require("Source.Data")
local ChildActorComponent = require("Source.Components.ChildActorComponent")
---@type { Special: Source.Configs.GeneralEnum.Special, State: Source.Configs.GeneralEnum.State }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local Battler = require("Source.Battler")
local DefeatSpawns = require("Source.Enemy.DefeatSpawns")
local Effects = require("Source.Gameplay.Effects")
local GeneralDataGraphAbility = require("Source.Gameplay.GeneralDataGraphAbility")
local GameplayScene = require("Source.Gameplay.GameplayScene")
local Player = require("Source.Player")
local MotaBattleAbility = require("Source.Gameplay.MotaBattleAbility")
local SpecialAbilities = require("Source.Gameplay.SpecialAbilities")

local GameplayEffectSpec = GlobalCore.GameplayEffectSpec
local GameplayEventData = GlobalCore.GameplayEventData
local ComponentsFunctions = GlobalFunctions.Components
local Actor = Engine.Actor
local Special = GeneralEnum.Special
local State = GeneralEnum.State

local componentTypes = {}
for name, componentType in pairs(ComponentsFunctions.getComponentTypes(Actor)) do
    componentTypes[name] = componentType
end
componentTypes.childActorComp = ChildActorComponent

local operationExpressions = {
    ["="] = "value",
    ["+"] = "current + value",
    ["-"] = "current - value",
    ["*"] = "current * value",
    ["/"] = "current / value",
    ["//"] = "current // value",
    ["%"] = "current % value",
    ["**"] = "current ^ value"
}

---@return GlobalCore.GameplayEventData
local function createCombatEvent(player, enemy, eventTag, payload)
    return GameplayEventData.new(player, enemy, eventTag, payload or {})
end

---@class Source.Enemy
local Enemy = {}

Enemy.ID = "FILL_IT_BY_YOURSELF"
Enemy.DefeatShatterEffectEnabled = true
Enemy._componentTypes = componentTypes
Enemy.childActorComp = ChildActorComponent.new({
    className = "Source.EnemyDamageText.EnemyDamageText",
    relativePosition = sf.Vector2f.new(0.0, 0.0)
})
Enemy.collisionEnabled = true
Enemy.animatable = true
Enemy.animateWithoutMoving = true
Enemy.afterBattleVarChanges = {}

function Enemy:init(texture, rect, tag)
    Actor.init(self, texture, rect, tag)
    self:_normaliseChildActorComp()
    local attributes = Data.CreateGeneralAttributeSet("Enemy", self.ID)
    Battler.init(self, attributes)
    self._battleCondition = nil
    self._defeatFinalising = false
    self._defeatFinalised = false
    local abilitySystem = self:getAbilitySystemComponent()
    abilitySystem:giveAbility(MotaBattleAbility.new(), "Builtin.MotaBattle")
    for _, specialID in ipairs(table.orderedStringKeys(self.attributes.special)) do
        local effect = SpecialAbilities.CreateEffect(specialID, self.attributes.special[specialID])
        abilitySystem:applyGameplayEffectSpec(
            GameplayEffectSpec.new(
                effect, GameplayEventData.new(self, self, "Event.Special.Initialise"), 1, "Special." .. specialID
            )
        )
    end
end

function Enemy:_normaliseChildActorComp()
    if not Class.hasOwnField(self, "childActorComp") or not Class.isInstance(self.childActorComp, ChildActorComponent) then
        self.childActorComp = ComponentsFunctions.componentFromData(ChildActorComponent, self.childActorComp)
    end
end

function Enemy:_getAfterBattleOperation(key)
    return self.afterBattleVarChanges[key][1], self.afterBattleVarChanges[key][2]
end

---@param instance Source.GameInstance.GameInstance
function Enemy:_evaluateAfterBattleVariableChanges(instance)
    local changes = {}
    for _, key in ipairs(table.orderedStringKeys(self.afterBattleVarChanges)) do
        local operator, value = self:_getAfterBattleOperation(key)
        local expression = operationExpressions[operator]
        assert(expression ~= nil, "Unsupported after-battle variable operator: " .. tostring(operator))
        if operator == "/" or operator == "//" or operator == "%" then
            assert(value ~= 0, "After-battle variable operation cannot divide by zero")
        end
        local defaultValue = operator == "=" and nil or 0
        local current = instance:getVariable(key)
        if current == nil then
            current = defaultValue
        end
        changes[key] = Engine.Eval(expression, { current = current, value = value })
    end
    return changes
end

---@param player Source.Player.Player
---@param scene  Source.Gameplay.GameplayScene
function Enemy:_preparePostBattle(player, scene)
    self:_evaluateAfterBattleVariableChanges(scene:getGameInstance())
    local rebornEnemy, droppedActors, spawnLayer = DefeatSpawns.Prepare(self, scene)
    local eventData = GameplayEventData.new(self, player, "Event.Combat.Reward")
    local effectSpecs = {
        Effects.CreateInstantModifierSpec("Combat.Reward.Gold", "GOLD", "Add", self.attributes.GOLD, eventData),
        Effects.CreateInstantModifierSpec("Combat.Reward.Exp", "EXP", "Add", self.attributes.EXP, eventData)
    }
    local stateSpecials = { { Special.Poisoning, State.Poisoned }, { Special.Weaken, State.Weak } }
    for _, stateSpecial in ipairs(stateSpecials) do
        local specialID = stateSpecial[1]
        local stateID = stateSpecial[2]
        local magnitude = SpecialAbilities.GetMagnitude(self:getAbilitySystemComponent(), specialID)
        if magnitude ~= nil then
            assert(math.type(magnitude) == "integer", specialID .. " special magnitude must be an integer")
            if magnitude > 0 then
                effectSpecs[#effectSpecs + 1] = Effects.CreateStateSpec(stateID, magnitude, eventData)
            end
        end
    end
    local playerAbilitySystem = player:getAbilitySystemComponent()
    for _, effectSpec in ipairs(effectSpecs) do
        playerAbilitySystem:validateGameplayEffectSpec(effectSpec)
    end
    return {
        player = player,
        rebornEnemy = rebornEnemy,
        droppedActors = droppedActors,
        spawnLayer = spawnLayer,
        effectSpecs = effectSpecs
    }
end

function Enemy:_executeDropAbility(player, droppedActor)
    local ability = GeneralDataGraphAbility.new("Item", droppedActor.ID, "onDrop")
    ability:activate(
        player:getAbilitySystemComponent(),
        GameplayEventData.new(self, droppedActor, "Event.Item.Drop", { itemID = droppedActor.ID })
    )
end

---@param scene Source.Gameplay.GameplayScene
function Enemy:_finaliseDefeat(scene, prepared)
    if self._defeatFinalised then
        return
    end
    self._defeatFinalised = true
    local variableChanges = self:_evaluateAfterBattleVariableChanges(scene:getGameInstance())
    for _, key in ipairs(table.orderedStringKeys(variableChanges)) do
        scene:getGameInstance():setVariable(key, variableChanges[key])
    end
    scene:recordDestroyedActor(self)
    if bool(Enemy.DefeatShatterEffectEnabled) then
        scene:getGameMap():playActorPixelShatterEffect(self)
    end
    self:destroy()
    if prepared.rebornEnemy ~= nil then
        DefeatSpawns.Spawn(scene, prepared.rebornEnemy, prepared.spawnLayer)
    end
    for _, droppedActor in ipairs(prepared.droppedActors) do
        DefeatSpawns.Spawn(scene, droppedActor, prepared.spawnLayer)
        self:_executeDropAbility(prepared.player, droppedActor)
    end
    for _, effectSpec in ipairs(prepared.effectSpecs) do
        prepared.player:getAbilitySystemComponent():applyGameplayEffectSpec(effectSpec)
    end
end

function Enemy:onCollision(other)
    local gameMap = self:getMap()
    ---@cast gameMap GameMap | nil
    local player = Player.MeetPlayer(other, gameMap ~= nil and gameMap:getPlayer() or nil)
    if player == nil or self._defeatFinalising or (self._battleCondition ~= nil and not self._battleCondition()) then
        return
    end
    assert(gameMap ~= nil, "Enemy combat requires an owning map")
    local scene = gameMap:getScene()
    assert(Class.isInstance(scene, GameplayScene), "Enemy combat requires a GameplayScene on its owning map")
    ---@cast scene Source.Gameplay.GameplayScene
    self._battleCondition = nil
    local battleEvent = createCombatEvent(player, self, "Event.Combat.MotaBattle", { commit = false })
    battleEvent.target = player
    local result = assert(self:getAbilitySystemComponent():tryActivateAbility(MotaBattleAbility.id, battleEvent))
    local won = result.code == MotaBattleAbility.BattleResult.WIN
    local prepared = won and self:_preparePostBattle(player, scene) or nil
    MotaBattleAbility.CommitResult(result)
    if result.data.damage > 0 then
        scene:getGameMap():addDamageText(tostring(result.data.damage), player:getPosition())
    end
    local animationLength = math.max(
        player:playAttackAnimationAt(scene, self:getPosition()), self:playAttackAnimationAt(scene, player:getPosition())
    )
    self._battleCondition = scene:addTimer(animationLength, function ()
        self._battleCondition = nil
        if won then
            self._defeatFinalising = true
            Actor.BlueprintEvent(self, Actor, "onDefeat", {}, function ()
                self:_finaliseDefeat(scene, prepared)
            end)
        else
            player:getAbilitySystemComponent():applyGameplayEffectSpec(result.data.gameOverEffectSpec)
            scene:requestGameOver(player, 0)
        end
    end)
end

---@diagnostic disable-next-line: unused, Blueprint event implementations use colon dispatch
function Enemy:onDefeat() end

return class(Enemy, Actor, Battler)
