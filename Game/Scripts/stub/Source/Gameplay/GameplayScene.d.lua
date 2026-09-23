---@meta Source.Gameplay.GameplayScene

---@class Source.Gameplay.GameplayScene: GlobalCore.SceneBase
local GameplayScene = {}

---@return GameMap
function GameplayScene:getGameMap() end

---@return Source.GameInstance.GameInstance
function GameplayScene:getGameInstance() end

---@param actor Engine.Actor
function GameplayScene:recordAddedActor(actor) end

---@param actor Engine.Actor
function GameplayScene:recordDestroyedActor(actor) end

---@param actor    Engine.Actor
---@param position sf.Vector2i
function GameplayScene:recordActorPosition(actor, position) end

--- Synchronously accept one adjacent-floor request from an Actor on this scene's map.
--- A rejected request leaves the player's movement state unchanged.
---@param teleporter Source.MapActors.Teleporter.Teleporter
---@param step       integer
---@return boolean
function GameplayScene:requestFloorStep(teleporter, step) end

--- Synchronously accept one transfer to a chosen map position without finding a stair.
--- A rejected request leaves movement and telepoint records unchanged.
---@param teleporter Source.MapActors.Teleporter.Teleporter
---@param mapPath    string
---@param position   sf.Vector2i
---@param record?    boolean                      Defaults to true; records each endpoint on its own map.
---@return boolean
function GameplayScene:requestMapTransfer(teleporter, mapPath, position, record) end

--- Navigate after the caller's completed damage settlement and animation delay.
--- Duplicate requests for the same active player do not create another timer.
--- Requests from destroyed scenes or replaced maps/players cannot navigate.
---@param player Source.MapActors.Player.Player
---@param delay  number
function GameplayScene:requestGameOver(player, delay) end

return GameplayScene
