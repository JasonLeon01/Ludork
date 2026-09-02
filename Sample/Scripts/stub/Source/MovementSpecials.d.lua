---@meta Source.MovementSpecials

---@class Source.MovementSpecials.DangerSource
---@field enemy   Source.Enemy
---@field special string
---@field damage  integer

---@class Source.MovementSpecials.PreviewContext
---@field player        Source.Player.Player
---@field damageByEnemy table<Source.Enemy, integer>

local MovementSpecials = {}

---@param enemies Source.Enemy[]
---@param player  Source.Player.Player
---@return Source.MovementSpecials.PreviewContext
function MovementSpecials.CreatePreviewContext(enemies, player) end

---@param enemies         Source.Enemy[]
---@param player          Source.Player.Player
---@param playerPosition  sf.Vector2i
---@param ignoredEnemies? Source.Enemy[]
---@param previewContext? Source.MovementSpecials.PreviewContext
---@return GlobalCore.GameplayAbilityResult
function MovementSpecials.Preview(enemies, player, playerPosition, ignoredEnemies, previewContext) end

---@param player        Source.Player.Player
---@param pathPositions sf.Vector2i[]
---@return GlobalCore.GameplayAbilityResult
function MovementSpecials.Commit(player, pathPositions) end

---@param player         Source.Player.Player
---@param pathPositions? sf.Vector2i[]
---@return GlobalCore.GameplayAbilityResult
function MovementSpecials.NotifyPlayerMovementFinished(player, pathPositions) end

return MovementSpecials
