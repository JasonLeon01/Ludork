---@meta Source.Infos.EnemyInfo
--- @brief Enemy data + logic layer.
---
--- Defines enemy-related blueprint events (onDefeat).
--- Independent of Actor; can be used standalone in battle systems.
---
---@class Source.Infos.EnemyInfo: Engine.InfoBase
---@field new fun(): Source.Infos.EnemyInfo
local EnemyInfo = {}

--- @brief Triggered when the enemy is defeated.
function EnemyInfo:onDefeat() end

return EnemyInfo
