---@meta Source.Configs.GeneralEnum
--- @brief Auto-generated General Data key constants.
---

--- @brief General Data table keys.
---@class Source.Configs.GeneralEnum.GeneralDataKey
---@field Class string
---@field Enemy string
---@field Equip string
---@field Item string
---@field Player string
---@field Special string
---@field State string
---@type Source.Configs.GeneralEnum.GeneralDataKey
local GeneralDataKey = {}

--- @brief Class member keys.
---@class Source.Configs.GeneralEnum.Class
---@field Warrior string
---@type Source.Configs.GeneralEnum.Class
local ClassKeys = {}

--- @brief Enemy member keys.
---@class Source.Configs.GeneralEnum.Enemy
---@field Bat string
---@field BigWizard string
---@field GreatIron string
---@field Knight string
---@field Mage string
---@field Rock string
---@field Skeleton string
---@field Slime string
---@field WhiteKing string
---@field Wizard string
---@type Source.Configs.GeneralEnum.Enemy
local Enemy = {}

--- @brief Equip member keys.
---@class Source.Configs.GeneralEnum.Equip
---@field Shield_A string
---@field Sword_A string
---@type Source.Configs.GeneralEnum.Equip
local Equip = {}

--- @brief Item member keys.
---@class Source.Configs.GeneralEnum.Item
---@field BreakIce string
---@field BreakLava string
---@field BreakWall string
---@field ClearWall string
---@field EnemyBook string
---@field KEY_B string
---@field KEY_R string
---@field KEY_Y string
---@field PoisonedEase string
---@field PoisonedRelease string
---@field Teleport string
---@field WeakEase string
---@field WeakRelease string
---@type Source.Configs.GeneralEnum.Item
local Item = {}

--- @brief Player member keys.
---@class Source.Configs.GeneralEnum.Player
---@field Bravor string
---@type Source.Configs.GeneralEnum.Player
local Player = {}

--- @brief Special member keys.
---@class Source.Configs.GeneralEnum.Special
---@field Blockade string
---@field Compete string
---@field Domain string
---@field Flank string
---@field Hard string
---@field Magic string
---@field MultiHit string
---@field Poisoning string
---@field Weaken string
---@type Source.Configs.GeneralEnum.Special
local Special = {}

--- @brief State member keys.
---@class Source.Configs.GeneralEnum.State
---@field Poisoned string
---@field Weak string
---@type Source.Configs.GeneralEnum.State
local State = {}

return {
    GeneralDataKey = GeneralDataKey,
    Class = ClassKeys,
    Enemy = Enemy,
    Equip = Equip,
    Item = Item,
    Player = Player,
    Special = Special,
    State = State,
}
