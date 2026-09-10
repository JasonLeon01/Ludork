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
---@param teleporter Source.Teleporter.Teleporter
---@param step       integer
---@return boolean
function GameplayScene:requestFloorStep(teleporter, step) end

--- Navigate after the caller's completed damage settlement and animation delay.
--- Duplicate requests for the same active player do not create another timer.
--- Requests from destroyed scenes or replaced maps/players cannot navigate.
---@param player Source.Player.Player
---@param delay  number
function GameplayScene:requestGameOver(player, delay) end

return GameplayScene
