local BattlerInfoComponent = require("Source.Components.BattlerInfoComponent")

---@class Source.Components.EnemyInfoComponent
local EnemyInfoComponent = {}

EnemyInfoComponent.name = ""
EnemyInfoComponent.desc = ""
EnemyInfoComponent.special = {}
EnemyInfoComponent.drops = {}

return class(EnemyInfoComponent, BattlerInfoComponent)
