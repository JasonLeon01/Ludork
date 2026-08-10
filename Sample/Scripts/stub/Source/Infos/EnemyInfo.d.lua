---@meta Source.Infos.EnemyInfo
--- @brief Enemy data + logic layer.
---
--- Defines enemy-related blueprint events (onDefeat, onEncounter).
--- Independent of Actor; can be used standalone in battle systems.
---
local EnemyInfo = {}

--- @brief Triggered when the enemy is defeated.
function EnemyInfo:onDefeat() end

return EnemyInfo
