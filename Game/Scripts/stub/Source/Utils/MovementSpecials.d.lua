---@meta Source.Utils.MovementSpecials

---@class Source.Utils.MovementSpecials.DangerSource
---@field enemy   Source.MapActors.Enemy
---@field special string
---@field damage  integer

---@class Source.Utils.MovementSpecials.PreviewContext
---@field player        Source.MapActors.Player.Player
---@field damageByEnemy table<Source.MapActors.Enemy, integer>

local MovementSpecials = {}

---@param enemies Source.MapActors.Enemy[]
---@param player  Source.MapActors.Player.Player
---@return Source.Utils.MovementSpecials.PreviewContext
function MovementSpecials.CreatePreviewContext(enemies, player) end

---@param enemies         Source.MapActors.Enemy[]
---@param player          Source.MapActors.Player.Player
---@param playerPosition  sf.Vector2i
---@param ignoredEnemies? Source.MapActors.Enemy[]
---@param previewContext? Source.Utils.MovementSpecials.PreviewContext
---@return GlobalCore.GameplayAbilityResult
function MovementSpecials.Preview(enemies, player, playerPosition, ignoredEnemies, previewContext) end

---@param player        Source.MapActors.Player.Player
---@param pathPositions sf.Vector2i[]
---@return GlobalCore.GameplayAbilityResult
function MovementSpecials.Commit(player, pathPositions) end

---@param player         Source.MapActors.Player.Player
---@param pathPositions? sf.Vector2i[]
---@return GlobalCore.GameplayAbilityResult
function MovementSpecials.NotifyPlayerMovementFinished(player, pathPositions) end

return MovementSpecials
