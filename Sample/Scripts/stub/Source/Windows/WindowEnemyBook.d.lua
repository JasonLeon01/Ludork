---@meta Source.Windows.WindowEnemyBook
---@brief Selectable monster handbook for enemies on the current map.
---@class Source.Windows.WindowEnemyBook: Source.Windows.Base.WindowSelectable
---@field new fun(rect: sf.IntRect, player: Source.Player.Player, onClose?: function, onConfirm?: function): Source.Windows.WindowEnemyBook
local WindowEnemyBook = {}

---@brief Construct the enemy handbook window.
---
--- - @param rect Window rectangle.
--- - @param player Player used to calculate displayed damage.
--- - @param onClose Optional callback invoked when the window closes.
--- - @param onConfirm Optional callback invoked when an enemy is confirmed.
---@param rect      sf.IntRect
---@param player    Source.Player.Player
---@param onClose   function | nil
---@param onConfirm function | nil
function WindowEnemyBook:init(rect, player, onClose, onConfirm) end

---@brief Rebind the player used for damage preview.
---
--- - @param player The current player instance.
---@param player Source.Player.Player
function WindowEnemyBook:setPlayer(player) end

---@brief Open the handbook and rescan current-map enemies.
---
--- - @param gameMap Current map to scan.
---@param gameMap GameMap | nil
function WindowEnemyBook:open(gameMap) end

---@brief Close the handbook.
function WindowEnemyBook:close() end

---@brief Refresh localised enemy, stat, and special text without rebuilding rows or previews.
function WindowEnemyBook:refreshLocale() end

---@param deltaTime number
function WindowEnemyBook:onTick(deltaTime) end

---@brief Close the handbook through its cancel path.
function WindowEnemyBook:onReturn() end

return WindowEnemyBook
