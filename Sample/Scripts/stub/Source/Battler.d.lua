---@meta Source.Battler

---@alias Source.Battler.DamageType integer
local DamageType = {}

---@alias Source.Battler.AttributeValue nil | boolean | number | string | table | userdata

--- @brief Mixin providing combat stats and state management.
---
--- Attach to any Actor via multiple inheritance to give it battle capabilities.
--- Manages a list of active `StateInfo` objects whose blueprint events can
--- drive non-combat behaviours such as walking effects and explicit hooks.
---@class Source.Battler.Battler
---@field DamageType { NORMAL: Source.Battler.DamageType, UNDEFEATABLE: Source.Battler.DamageType }
---@field infoComp Source.Components.BattlerInfoComponent
---@field _loading boolean
---@field _combatRevision integer
---@field _monitoredInfoComp Source.Components.BattlerInfoComponent | nil
---@field _combatMonitorParams table
---@field _states Source.Infos.StateInfo[]
---@field new fun(attrs?: table<string, Source.Battler.AttributeValue>): Source.Battler.Battler
local Battler = {}

--- @brief Construct a battler with optional attribute overrides.
---
--- - @param attrs Optional dictionary of attribute overrides.
---@param attrs? table<string, Source.Battler.AttributeValue>
function Battler:init(attrs) end

--- @brief Ensure `infoComp` matches this battler's expected component type.
function Battler:normaliseInfoComp() end

---@return integer
function Battler:getCombatRevision() end

--- @brief Check whether a state is currently active.
---
--- - @param state State ID string or `StateInfo` instance.
--- - @return True if the state is active.
---@param state string | Source.Infos.StateInfo
---@return boolean
function Battler:hasState(state) end

--- @brief Return the active `StateInfo` matching the given ID.
---
--- - @param stateID State identifier.
--- - @return The matching state, or nil.
---@param stateID string
---@return Source.Infos.StateInfo | nil
function Battler:getStateByID(stateID) end

--- @brief Get a shallow copy of all active states.
---
--- - @return List of currently active `StateInfo` instances.
---@return Source.Infos.StateInfo[]
function Battler:getStates() end

--- @brief Get the IDs of all active states (for serialization).
---
--- - @return List of state ID strings.
---@return string[]
function Battler:getStateIDs() end

--- @brief Get active state IDs mapped to stack counts (for serialization).
---
--- - @return Dictionary of state ID to stack count.
---@return table<string, integer>
function Battler:getStateStacks() end

--- @brief Get the stack count for an active state.
---
--- - @param stateID State identifier.
--- - @return Stack count, or 0 if the state is not active.
---@param stateID string
---@return integer
function Battler:getStateStackCount(stateID) end

--- @brief Check whether this battler has the given special flag.
---
--- - @param specialID Special identifier.
--- - @return True if the battler's info component contains the special.
---@param specialID string
---@return boolean
function Battler:hasSpecial(specialID) end

---@param specialID string
---@param default   integer | nil
---@param minValue  integer | nil
---@return integer
function Battler:getSpecialIntValue(specialID, default, minValue) end

--- @brief Get the battler's effective attack value.
---
--- - @param opponent Optional opposing battler used for special calculations.
--- - @return Current attack value.
---@param opponent Source.Battler.Battler | nil
---@return integer
function Battler:getATK(opponent) end

--- @brief Get the battler's effective defense value.
---
--- - @param attacker Optional opposing battler used for special calculations.
--- - @return Current defense value.
---@param attacker Source.Battler.Battler | nil
---@return integer
function Battler:getDEF(attacker) end

--- @brief Get the names of all active states.
---
--- - @return List of state names.
---@return string[]
function Battler:getStateNames() end

--- @brief Apply a state to this battler with the given stack count.
---
--- Accepts either a state ID (string) or a pre-built `StateInfo`.
--- When given an ID the corresponding `StateInfo` is built from
--- GeneralData. If the state is already active and its GeneralData
--- marks it as stackable, the stack count is increased instead.
---
--- - @param state State ID string or `StateInfo` instance.
--- - @param stacks Stack count to apply or add.
---@param state  string | Source.Infos.StateInfo
---@param stacks integer
function Battler:addState(state, stacks) end

--- @brief Remove an active state by ID or instance.
---
--- - @param state State ID string or `StateInfo` instance.
---@param state string | Source.Infos.StateInfo
function Battler:removeState(state) end

--- @brief Reduce stack count for an active state, removing it at zero.
---
--- - @param state State ID string or `StateInfo` instance.
--- - @param stacks Stack count to reduce.
---@param state  string | Source.Infos.StateInfo
---@param stacks integer
function Battler:reduceStateStacks(state, stacks) end

--- @brief Remove all active states.
function Battler:clearStates() end

--- @brief Replace active states by ID list (used during load).
---
--- Existing states are cleared first. Each state receives one stack.
---
--- - @param stateIDs List of state ID strings.
---@param stateIDs string[]
function Battler:setStateIDs(stateIDs) end

--- @brief Replace active states from an ID-to-stacks map (used during load).
---
--- Existing states are cleared first.
---
--- - @param stateStacks Mapping of state ID to stack count.
---@param stateStacks table<string, integer>
function Battler:setStateStacks(stateStacks) end

--- @brief Trigger the walking event on every active state.
function Battler:triggerStateWalk() end

--- @brief Trigger the developer-controlled hook event on one active state.
---
--- - @param stateKey State ID to trigger.
---@param stateKey string
function Battler:triggerStateHook(stateKey) end

--- @brief Play this battler's attack animation at a world position.
---
--- - @param scene The scene that owns the animation list.
--- - @param targetPosition World position used as the animation anchor.
--- - @return Visual duration in seconds, or 0 when no animation is configured.
---@param scene          GlobalCore.SceneBase
---@param targetPosition sf.Vector2f
---@return number
function Battler:playAttackAnimationAt(scene, targetPosition) end

--- @brief Calculate one attacker-to-defender exchange.
---
--- Combat damage no longer invokes state blueprint events.
---
--- - @param attacker The battler dealing damage.
--- - @param defender The battler receiving damage.
--- - @return Damage per round dealt to the defender.
---@param defender Source.Battler.Battler
---@return integer
function Battler:getDamagePerRound(defender) end

--- @brief Get the number of hits this battler deals per attack.
---
--- - @return Attack hit count.
---@return integer
function Battler:getHitCount() end

--- @brief Calculate accumulated damage taken by `battler` if it fights `self`.
---
--- Models a round-by-round duel where `battler` is the attacker and
--- `self` is the defender; returns how much damage `battler` ultimately
--- suffers from `self`'s counter-attacks before defeating `self`.
---
--- Pipeline:
---     1. attackDamage  = battler.getDamagePerRound(self)
---     2. counterDamage = self.getDamagePerRound(battler)
---     3. counterRounds = max(ceil(self.infoComp.MAXHP / attackDamage) - 1, 0)
---     4. totalDamage = counterRounds * counterDamage
---
--- - @param battler The opposing battler (the attacker).
--- - @return Tuple of (DamageType, accumulated damage on `battler`).
---@param battler Source.Battler.Battler
---@return Source.Battler.DamageType, integer
function Battler:getDamage(battler) end

return Battler
