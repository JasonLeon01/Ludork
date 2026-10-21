---@meta Source.Configs.Gameplay

---@class Source.Configs.Gameplay.Module
local GameplayConfig = {}

--- Apply non-null project Gameplay overrides before creating gameplay objects.
--- Null values preserve the defaults declared by their owning scripts.
function GameplayConfig.Apply() end

return GameplayConfig
