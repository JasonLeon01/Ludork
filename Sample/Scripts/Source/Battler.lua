local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Data = require("Source.Data")
local LocaleCore = require("Source.Locale.Core")
---@type { Special: Source.Configs.GeneralEnum.Special, State: Source.Configs.GeneralEnum.State }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local BattlerInfoComponent = require("Source.Components.BattlerInfoComponent")
local StateInfo = require("Source.Infos.StateInfo")

local ComponentsFunctions = GlobalFunctions.Components
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat
local Special = GeneralEnum.Special
local State = GeneralEnum.State

local DamageType = { NORMAL = 0, UNDEFEATABLE = 1 }

---@param old     Source.Battler.AttributeValue | table
---@param new     Source.Battler.AttributeValue
---@param battler Source.Battler.Battler
local function onCombatAttributeChange(old, new, battler)
    if battler == nil then
        return
    end
    if old ~= new then
        battler:_incrementCombatRevision()
    end
end

---@class Source.Battler.Battler
local Battler = {}

Battler.DamageType = DamageType
Battler._componentTypes = { infoComp = BattlerInfoComponent }
Battler.infoComp = BattlerInfoComponent.new()

function Battler:init(attrs)
    self:_normaliseInfoComp()
    self._combatRevision = 0
    self._monitoredInfoComp = nil
    self._combatMonitorParams = setmetatable({ self }, {
        __mode = "v"
    })
    self:_ensureCombatMonitors()
    if bool(attrs) then
        ---@cast attrs table<string, Source.Battler.AttributeValue>
        for key, value in pairs(attrs) do
            if not ComponentsFunctions.setComponentFieldValue(self, key, value) then
                self[key] = value
            end
        end
    end
    self._states = {}
end

function Battler:_normaliseInfoComp()
    local value = self.infoComp
    local componentType = self:_getInfoCompType()
    if not Class.hasOwnField(self, "infoComp") or not Class.isInstance(value, componentType) then
        self.infoComp = ComponentsFunctions.componentFromData(componentType, value)
    end
end

function Battler:_incrementCombatRevision()
    self._combatRevision = self._combatRevision + 1
end

function Battler:getCombatRevision()
    return self._combatRevision
end

function Battler:_ensureCombatMonitors()
    local infoComp = self.infoComp
    if self._monitoredInfoComp == infoComp then
        return
    end
    self._monitoredInfoComp = infoComp
    Class.monitor(infoComp, "MAXHP", onCombatAttributeChange, self._combatMonitorParams)
    Class.monitor(infoComp, "ATK", onCombatAttributeChange, self._combatMonitorParams)
    Class.monitor(infoComp, "DEF", onCombatAttributeChange, self._combatMonitorParams)
    if infoComp.special ~= nil then
        Class.monitor(infoComp, "special", onCombatAttributeChange, self._combatMonitorParams)
    end
end

---@return { new: fun(values?: table<string, Source.Battler.AttributeValue>): Source.Components.BattlerInfoComponent }
function Battler:_getInfoCompType()
    local componentTypes = Class.type(self)._componentTypes
    if componentTypes == nil then
        componentTypes = {}
    end
    local componentType = componentTypes.infoComp or BattlerInfoComponent
    if not Class.isSubclass(componentType, BattlerInfoComponent) then
        return BattlerInfoComponent
    end
    return componentType
end

---@generic T
---@param key     string
---@param default T
---@return T
function Battler:_getInfoField(key, default)
    local value = self.infoComp[key]
    return value == nil and default or value
end

---@generic T
---@param key   string
---@param value T
function Battler:_setInfoField(key, value)
    if self.infoComp[key] ~= nil then
        self.infoComp[key] = value
    end
end

function Battler:hasState(state)
    return self:getStateByID(Battler._resolveStateID(state)) ~= nil
end

function Battler:getStateByID(stateID)
    if not bool(stateID) then
        return nil
    end
    for _, state in ipairs(self._states) do
        if state.ID == stateID then
            return state
        end
    end
    return nil
end

function Battler:getStates()
    local result = {}
    for index, state in ipairs(self._states) do
        result[index] = state
    end
    return result
end

function Battler:getStateIDs()
    local result = {}
    for _, state in ipairs(self._states) do
        result[#result + 1] = state.ID
    end
    return result
end

function Battler:getStateStacks()
    local result = {}
    for _, state in ipairs(self._states) do
        result[state.ID] = state.stacks
    end
    return result
end

---@param stateID string
---@return integer
function Battler:_getStateStackCount(stateID)
    local state = self:getStateByID(stateID)
    if state == nil then
        return 0
    end
    return state.stacks
end

function Battler:getStateStackCount(stateID)
    return self:_getStateStackCount(stateID)
end

function Battler:hasSpecial(specialID)
    local special = self.infoComp.special
    if not bool(special) then
        return false
    end
    ---@cast special table<string, Source.Battler.AttributeValue>
    return special[specialID] ~= nil
end

function Battler:getSpecialIntValue(specialID, default, minValue)
    default = default == nil and 0 or default
    minValue = minValue == nil and 0 or minValue
    ---@cast default integer
    ---@cast minValue integer
    local special = self.infoComp.special
    if not bool(special) then
        return default < minValue and minValue or default
    end
    ---@cast special table<string, Source.Battler.AttributeValue>
    if special[specialID] == nil then
        return default < minValue and minValue or default
    end
    local value = special[specialID]
    assert(math.type(value) == "integer", "Battler special " .. specialID .. " must be an integer")
    ---@cast value integer
    return value < minValue and minValue or value
end

function Battler:getATK(opponent)
    local attackerATK = self.infoComp.ATK
    if self:hasState(State.Weak) then
        attackerATK = math.max(0, attackerATK - 2 * self:_getStateStackCount(State.Weak))
        ---@cast attackerATK integer
    end
    if opponent ~= nil and self:hasSpecial(Special.Compete) then
        attackerATK = math.max(attackerATK, opponent:getATK())
        ---@cast attackerATK integer
    end
    return attackerATK
end

function Battler:getDEF(attacker)
    local defenderDEF = self.infoComp.DEF
    if self:hasState(State.Weak) then
        defenderDEF = math.max(0, defenderDEF - 2 * self:_getStateStackCount(State.Weak))
        ---@cast defenderDEF integer
    end
    if attacker ~= nil and self:hasSpecial(Special.Hard) then
        defenderDEF = math.max(defenderDEF, attacker:getATK(self) - 1)
        ---@cast defenderDEF integer
    end
    return defenderDEF
end

function Battler:getStateNames()
    local result = {}
    for _, state in ipairs(self._states) do
        result[#result + 1] = LOC(state.name)
    end
    return result
end

function Battler:addState(state, stacks)
    local info = Battler._buildStateInfo(state)
    if info == nil then
        return
    end
    local stackCount = math.max(0, stacks)
    if stackCount <= 0 then
        return
    end
    local existing = self:getStateByID(info.ID)
    if existing ~= nil then
        if bool(Data.getGeneralStateData(info.ID).stackable) then
            existing.stacks = existing.stacks + stackCount
            self:_incrementCombatRevision()
        end
        return
    end
    info.stacks = stackCount
    info:setOwner(self)
    self._states[#self._states + 1] = info
    self:_incrementCombatRevision()
end

function Battler:removeState(state)
    local existing = self:getStateByID(Battler._resolveStateID(state))
    if existing == nil then
        return
    end
    existing:setOwner(nil)
    for index, value in ipairs(self._states) do
        if value == existing then
            table.remove(self._states, index)
            self:_incrementCombatRevision()
            return
        end
    end
end

function Battler:reduceStateStacks(state, stacks)
    local existing = self:getStateByID(Battler._resolveStateID(state))
    if existing == nil then
        return
    end
    stacks = stacks == nil and 1 or stacks
    local stackCount = math.max(0, stacks)
    if stackCount <= 0 then
        return
    end
    existing.stacks = math.max(0, existing.stacks - stackCount)
    if existing.stacks <= 0 then
        self:removeState(existing)
    else
        self:_incrementCombatRevision()
    end
end

function Battler:clearStates()
    local hadStates = #self._states > 0
    for _, state in ipairs(self._states) do
        state:setOwner(nil)
    end
    self._states = {}
    if hadStates then
        self:_incrementCombatRevision()
    end
end

function Battler:setStateIDs(stateIDs)
    self:clearStates()
    for _, stateID in ipairs(stateIDs or {}) do
        self:addState(stateID, 1)
    end
end

function Battler:setStateStacks(stateStacks)
    self:clearStates()
    for stateID, stacks in pairs(stateStacks or {}) do
        self:addState(stateID, stacks)
    end
end

---@param eventName string
---@param kwargs    table<string, any> | nil
function Battler:_triggerStateEvent(eventName, kwargs)
    kwargs = kwargs or {}
    kwargs.battler = self
    for _, state in ipairs(self:getStates()) do
        state:triggerEvent(eventName, kwargs)
    end
end

function Battler:triggerStateWalk()
    self:_triggerStateEvent("onWalk")
end

function Battler:triggerStateHook(stateKey)
    local state = self:getStateByID(stateKey)
    if state ~= nil then
        state:triggerEvent("onHookTriggered", {
            battler = self
        })
    end
end

function Battler:playAttackAnimationAt(scene, targetPosition)
    local animationKey = self.infoComp.ANIMATION_KEY
    if not bool(animationKey) then
        return 0.0
    end
    local animationData = Data.getAnimation(animationKey)
    local Animation = GlobalCore.Animation
    local animation = Animation.new(animationData, true)
    local halfCell = Engine.CellSize * 0.5
    animation:setPosition(sf.Vector2f.new(targetPosition.x + halfCell, targetPosition.y + halfCell))
    scene:addAnim(animation)
    return animation:getVisualDuration()
end

function Battler:getDamagePerRound(defender)
    local attackerATK = self:getATK(defender)
    local defenderDEF = defender:getDEF(self)
    if self:hasSpecial(Special.Magic) then
        defenderDEF = 0
    end
    local basicDamage = math.max(0, attackerATK - defenderDEF) * self:getHitCount()
    if defender:hasState(State.Poisoned) then
        basicDamage = basicDamage + 10 * defender:getStateStackCount(State.Poisoned)
    end
    ---@cast basicDamage integer
    return basicDamage
end

function Battler:getHitCount()
    return self:getSpecialIntValue(Special.MultiHit, 1, 1)
end

function Battler:getDamage(battler)
    local attackDamage = battler:getDamagePerRound(self)
    local counterDamage = self:getDamagePerRound(battler)
    if attackDamage <= 0 then
        return DamageType.UNDEFEATABLE, -1
    end
    local counterRounds = math.max(0, math.ceil(self.infoComp.MAXHP / attackDamage) - 1)
    return DamageType.NORMAL, math.max(0, counterRounds * counterDamage)
end

---@param state string | Source.Infos.StateInfo | nil
---@return string
function Battler._resolveStateID(state)
    if state == nil then
        return ""
    end
    if type(state) == "string" then
        return state
    end
    return state.ID
end

---@param state string | Source.Infos.StateInfo | nil
---@return Source.Infos.StateInfo | nil
function Battler._buildStateInfo(state)
    if state == nil then
        return nil
    end
    if Class.isInstance(state, StateInfo) then
        ---@cast state Source.Infos.StateInfo
        if not state:hasInfoGraph() and bool(state.ID) then
            state:initInfo(Data)
        end
        return state
    end
    if type(state) == "string" and bool(state) then
        local info = StateInfo.new()
        info.ID = state
        info:initInfo(Data)
        return info
    end
    return nil
end

return class(Battler)
