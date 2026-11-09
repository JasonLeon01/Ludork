---@class Source.Configs.GameplayConstants.Module
---@field MOVEMENT_HAZARD_TAG                  string
---@field BATTLE_RULES_EVENT                   string
---@field MOVEMENT_STEP_EVENT                  string
---@field MOVEMENT_QUERY_HAZARD_EVENT          string
---@field MOVEMENT_QUERY_HAZARD_BOUNDS_EVENT   string
---@field COMBAT_RESOLVE_ATTACK_EVENT          string
---@field COMBAT_RESOLVE_DEFENSE_EVENT         string
---@field COMBAT_RESOLVE_HIT_COUNT_EVENT       string
---@field COMBAT_RESOLVE_DAMAGE_EVENT          string
---@field COMBAT_RESOLVE_INCOMING_DAMAGE_EVENT string
---@field EQUIPMENT_EQUIP_EVENT                string
---@field SPECIAL_PREFIX                       string
---@field STATE_PREFIX                         string
---@field EQUIPMENT_PREFIX                     string
---@field EQUIPMENT_UNEQUIP_EVENT              string
---@field STATE_RESTORE_EVENT                  string
---@field STATE_TRIGGER_EVENT_PREFIX           string
---@field ITEM_USE_EVENT                       string
---@field ITEM_DROP_EVENT                      string
---@field SPECIAL_INITIALISE_EVENT             string
---@field COMBAT_REWARD_EVENT                  string
---@field COMBAT_BATTLE_EVENT                  string
---@field MOVEMENT_HAZARD_DAMAGE_EVENT         string
---@field EQUIPMENT_SLOT_TAG_PREFIX            string
local GameplayConstants = {}

GameplayConstants.MOVEMENT_HAZARD_TAG = "Gameplay.Movement.Hazard"
GameplayConstants.BATTLE_RULES_EVENT = "Event.Combat.ResolveBattleRules"
GameplayConstants.MOVEMENT_STEP_EVENT = "Event.Movement.Step"
GameplayConstants.MOVEMENT_QUERY_HAZARD_EVENT = "Event.Movement.QueryHazard"
GameplayConstants.MOVEMENT_QUERY_HAZARD_BOUNDS_EVENT = "Event.Movement.QueryHazardBounds"
GameplayConstants.COMBAT_RESOLVE_ATTACK_EVENT = "Event.Combat.ResolveAttack"
GameplayConstants.COMBAT_RESOLVE_DEFENSE_EVENT = "Event.Combat.ResolveDefense"
GameplayConstants.COMBAT_RESOLVE_HIT_COUNT_EVENT = "Event.Combat.ResolveHitCount"
GameplayConstants.COMBAT_RESOLVE_DAMAGE_EVENT = "Event.Combat.ResolveDamage"
GameplayConstants.COMBAT_RESOLVE_INCOMING_DAMAGE_EVENT = "Event.Combat.ResolveIncomingDamage"
GameplayConstants.EQUIPMENT_EQUIP_EVENT = "Event.Equipment.Equip"
GameplayConstants.SPECIAL_PREFIX = "Special."
GameplayConstants.STATE_PREFIX = "State."
GameplayConstants.EQUIPMENT_PREFIX = "Equipment."

GameplayConstants.EQUIPMENT_UNEQUIP_EVENT = "Event.Equipment.Unequip"
GameplayConstants.STATE_RESTORE_EVENT = "Event.State.Restore"
GameplayConstants.STATE_TRIGGER_EVENT_PREFIX = "Event.State.Trigger."
GameplayConstants.ITEM_USE_EVENT = "Event.Item.Use"
GameplayConstants.ITEM_DROP_EVENT = "Event.Item.Drop"
GameplayConstants.SPECIAL_INITIALISE_EVENT = "Event.Special.Initialise"
GameplayConstants.COMBAT_REWARD_EVENT = "Event.Combat.Reward"
GameplayConstants.COMBAT_BATTLE_EVENT = "Event.Combat.MotaBattle"
GameplayConstants.MOVEMENT_HAZARD_DAMAGE_EVENT = "Event.Movement.HazardDamage"
GameplayConstants.EQUIPMENT_SLOT_TAG_PREFIX = "Equipment.Slot."

return GameplayConstants
