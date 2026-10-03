---@meta Source.Gameplay.MotaBattleAbility

---@class Source.Gameplay.MotaBattleData
---@field damage              integer
---@field attackDamage        integer
---@field counterDamage       integer
---@field counterRounds       integer | nil
---@field vampireHealing      integer
---@field firstStrikeDamage   integer
---@field fixedDamage         integer
---@field playerAttack        table<string, integer>
---@field enemyAttack         table<string, integer>
---@field enemy               Source.MapActors.Enemy
---@field player              Source.MapActors.Player.Player
---@field committed           boolean
---@field damageEffectSpec?   GlobalCore.GameplayEffectSpec
---@field gameOverEffectSpec? GlobalCore.GameplayEffectSpec

---@class Source.Gameplay.MotaBattleResult: GlobalCore.GameplayAbilityResult
---@field code Enums.BattleResultCode
---@field data Source.Gameplay.MotaBattleData

---@class Source.Gameplay.MotaCriticalResult: GlobalCore.GameplayAbilityResult
---@field code Enums.CriticalResultCode
---@field data { value?: integer }

---@class Source.Gameplay.MotaBattleAbility: GlobalCore.GameplayAbility
---@field id  string
---@field new fun(): Source.Gameplay.MotaBattleAbility
local MotaBattleAbility = {}

---@param abilitySystem GlobalCore.AbilitySystemComponent
---@param eventData     GlobalCore.GameplayEventData
---@return Source.Gameplay.MotaBattleResult
function MotaBattleAbility:calculate(abilitySystem, eventData) end

---@param abilitySystem GlobalCore.AbilitySystemComponent
---@param eventData     GlobalCore.GameplayEventData
---@return Source.Gameplay.MotaBattleResult
function MotaBattleAbility:activate(abilitySystem, eventData) end

---@param attacker Source.Battler.Battler
---@param defender Source.Battler.Battler
---@return integer, table<string, integer>
function MotaBattleAbility.CalculateDamagePerRound(attacker, defender) end

---@param result Source.Gameplay.MotaBattleResult
function MotaBattleAbility.CommitResult(result) end

---@param enemy  Source.MapActors.Enemy
---@param player Source.MapActors.Player.Player
---@return Source.Gameplay.MotaCriticalResult
function MotaBattleAbility.CalculateCriticalValue(enemy, player) end

return MotaBattleAbility
