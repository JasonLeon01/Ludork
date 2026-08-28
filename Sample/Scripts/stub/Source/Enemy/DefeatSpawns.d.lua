---@meta Source.Enemy.DefeatSpawns

local DefeatSpawns = {}

---@param enemy Source.Enemy
---@param scene Source.Scenes.SceneMap.SceneMap
---@return Engine.Actor | nil rebornActor
---@return Source.Item[] droppedActors
---@return string | nil layerName
function DefeatSpawns.Prepare(enemy, scene) end

---@param scene     Source.Scenes.SceneMap.SceneMap
---@param actor     Engine.Actor
---@param layerName string
function DefeatSpawns.Spawn(scene, actor, layerName) end

function DefeatSpawns.GameOver() end

return DefeatSpawns
