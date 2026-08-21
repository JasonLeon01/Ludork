---@meta Source.SceneComponents.MovementDangerState

---@class Source.SceneComponents.MovementDangerEntry
---@field position sf.Vector2i
---@field damage   integer
---@field sources  Source.MovementSpecials.DangerSource[]

---@class Source.SceneComponents.MovementDangerEnemySnapshot
---@field x?              integer
---@field y?              integer
---@field combatRevision? integer
---@field infoComp?       Source.Components.EnemyInfoComponent
---@field scanRevision?   integer

---@brief Cached movement-special danger grid for the current map and player.
---@class Source.SceneComponents.MovementDangerState: ComponentBase
---@field _parent               GameMap
---@field _player               Source.Player.Player | nil
---@field _playerInfoComp       Source.Components.PlayerInfoComponent | nil
---@field _playerCombatRevision integer | nil
---@field _enemies              Source.Enemy[]
---@field _enemyScanRevision    integer
---@field _entryGrid            table<integer, table<integer, Source.SceneComponents.MovementDangerEntry>>
---@field _mapWidth             integer
---@field _mapHeight            integer
---@field _revision             integer
---@field _entries              Source.SceneComponents.MovementDangerEntry[]
---@field _enemySnapshots       table<Source.Enemy, Source.SceneComponents.MovementDangerEnemySnapshot>
local MovementDangerState = {}

---@param gameMap GameMap
---@return Source.SceneComponents.MovementDangerState
function MovementDangerState.new(...) end

---@param gameMap GameMap
function MovementDangerState:init(gameMap) end

---@param deltaTime number
function MovementDangerState:onTick(deltaTime) end

---@return integer
function MovementDangerState:getRevision() end

---@return Source.SceneComponents.MovementDangerEntry[]
function MovementDangerState:getEntries() end

---@param position       sf.Vector2i
---@param ignoredEnemies Source.Enemy[] | nil
---@return integer
function MovementDangerState:getDamageAt(position, ignoredEnemies) end

---@param goal               sf.Vector2i
---@param ignoredGoalEnemies Source.Enemy[] | nil
---@param allowGoal          boolean
---@return sf.Vector2i[]
function MovementDangerState:getExcludedAnchors(goal, ignoredGoalEnemies, allowGoal) end

return MovementDangerState
