local GlobalCore = require("GlobalCore")

local SceneBase = GlobalCore.SceneBase

local GameplayScene = {}

---@diagnostic disable-next-line: unused
function GameplayScene:getGameMap()
    error("GameplayScene.getGameMap must be implemented")
end

---@diagnostic disable-next-line: unused
function GameplayScene:getGameInstance()
    error("GameplayScene.getGameInstance must be implemented")
end

---@diagnostic disable-next-line: unused
function GameplayScene:recordAddedActor(actor)
    error("GameplayScene.recordAddedActor must be implemented")
end

---@diagnostic disable-next-line: unused
function GameplayScene:recordDestroyedActor(actor)
    error("GameplayScene.recordDestroyedActor must be implemented")
end

---@diagnostic disable-next-line: unused
function GameplayScene:recordActorPosition(actor, position)
    error("GameplayScene.recordActorPosition must be implemented")
end

---@diagnostic disable-next-line: unused
function GameplayScene:requestFloorStep(teleporter, step)
    error("GameplayScene.requestFloorStep must be implemented")
end

---@diagnostic disable-next-line: unused
function GameplayScene:requestMapTransfer(teleporter, mapPath, position, record)
    error("GameplayScene.requestMapTransfer must be implemented")
end

---@diagnostic disable-next-line: unused
function GameplayScene:requestGameOver(player, delay)
    error("GameplayScene.requestGameOver must be implemented")
end

return class(GameplayScene, SceneBase)
