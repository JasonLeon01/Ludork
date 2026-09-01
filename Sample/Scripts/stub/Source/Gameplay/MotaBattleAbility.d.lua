---@meta Source.Gameplay.MotaBattleAbility

---@alias Source.Gameplay.MotaBattleAbility.BattleResult integer
local BattleResult = {}

---@alias Source.Gameplay.MotaBattleAbility.CriticalResult integer
local CriticalResult = {}

---@class Source.Gameplay.MotaBattleData
---@field damage              integer
---@field attackDamage        integer
---@field counterDamage       integer
---@field counterRounds       integer
---@field playerAttack        table<string, integer>
---@field enemyAttack         table<string, integer>
---@field enemy               Source.Enemy
---@field player              Source.Player.Player
---@field committed           boolean
---@field damageEffectSpec?   Global.Gameplay.GameplayEffectSpec
---@field gameOverEffectSpec? Global.Gameplay.GameplayEffectSpec

---@class Source.Gameplay.MotaBattleResult: Global.Gameplay.GameplayAbilityResult
---@field code Source.Gameplay.MotaBattleAbility.BattleResult
---@field data Source.Gameplay.MotaBattleData

---@class Source.Gameplay.MotaCriticalResult: Global.Gameplay.GameplayAbilityResult
---@field code Source.Gameplay.MotaBattleAbility.CriticalResult
---@field data { value?: integer }

---@class Source.Gameplay.MotaBattleAbility: Global.Gameplay.GameplayAbility
---@field id             string
---@field BattleResult   { WIN: Source.Gameplay.MotaBattleAbility.BattleResult, CANNOT_DAMAGE: Source.Gameplay.MotaBattleAbility.BattleResult, LETHAL_COUNTER_DAMAGE: Source.Gameplay.MotaBattleAbility.BattleResult }
---@field CriticalResult { VALUE: Source.Gameplay.MotaBattleAbility.CriticalResult, NOT_NEEDED: Source.Gameplay.MotaBattleAbility.CriticalResult, UNKNOWN: Source.Gameplay.MotaBattleAbility.CriticalResult }
---@field new            fun(): Source.Gameplay.MotaBattleAbility
local MotaBattleAbility = {}

---@param abilitySystem Global.Gameplay.AbilitySystemComponent
---@param eventData     Global.Gameplay.GameplayEventData
---@return Source.Gameplay.MotaBattleResult
function MotaBattleAbility:calculate(abilitySystem, eventData) end

---@param abilitySystem Global.Gameplay.AbilitySystemComponent
---@param eventData     Global.Gameplay.GameplayEventData
---@return Source.Gameplay.MotaBattleResult
function MotaBattleAbility:activate(abilitySystem, eventData) end

---@param attacker Source.Battler.Battler
---@param defender Source.Battler.Battler
---@return integer, table<string, integer>
function MotaBattleAbility.CalculateDamagePerRound(attacker, defender) end

---@param result Source.Gameplay.MotaBattleResult
function MotaBattleAbility.CommitResult(result) end

---@param enemy  Source.Enemy
---@param player Source.Player.Player
---@return Source.Gameplay.MotaCriticalResult
function MotaBattleAbility.CalculateCriticalValue(enemy, player) end

return MotaBattleAbility
