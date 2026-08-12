---@meta Source.Enemy
--- @brief Scene enemy entity.
---
--- Bridges Actor (rendering/collision/movement) and EnemyInfo (enemy data + event logic)
--- via multiple inheritance.
---
---@class Source.Enemy: Engine.Actor, Source.Infos.EnemyInfo, Source.Battler.Battler
---@field infoComp Source.Components.EnemyInfoComponent
---@field childActorComp Source.Components.ChildActorComponent
---@field afterBattleVarChanges table<string, { [1]: string, [2]: number }>
---@field _battleCondition fun(): boolean | nil
---@field new fun(texture?: sf.Texture, rect?: sf.IntRect, tag?: string): Source.Enemy
local Enemy = {}

--- @brief Construct an enemy with actor rendering and enemy info.
---
--- - @param texture Optional sf.Texture for the actor sprite.
--- - @param rect Optional sf.IntRect texture rectangle.
--- - @param tag Optional actor tag.
---@param texture sf.Texture | nil
---@param rect    sf.IntRect | nil
---@param tag     string | nil
function Enemy:init(texture, rect, tag) end

--- @brief Perform battle calculations against the player.
---
--- - @return 0 for win, 1 for lose or undefeatable opponent.
---@return integer
function Enemy:battle() end

---@param against Source.Battler.Battler
function Enemy:afterBattle(against) end

---@return table<string, string | number | boolean>
function Enemy:getSpecial() end

---@return string[]
function Enemy:getDrops() end

--- @brief Calculate the next attack threshold for this enemy.
---
--- - @param battler The opposing battler used as the attacker.
--- - @return Attack threshold value, or a negative special marker.
---@param battler Source.Battler.Battler
---@return integer
function Enemy:getCriticalValue(battler) end

---@param other Engine.Actor[]
function Enemy:onCollision(other) end

--- @brief Triggered when the enemy is defeated.
function Enemy:onDefeat() end

return Enemy
