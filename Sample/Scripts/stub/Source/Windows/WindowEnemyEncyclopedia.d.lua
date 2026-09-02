---@meta Source.Windows.WindowEnemyEncyclopedia
---@class Source.Windows.WindowEnemyEncyclopedia: Source.Windows.Base.WindowBase
---@field new              fun(rect: sf.IntRect, onClose?: function): Source.Windows.WindowEnemyEncyclopedia
---@field uiClass          Source.UI.WindowEnemyEncyclopedia
---@field _onCloseCallback function | nil
---@field _portrait        Engine.CharacterView | nil
---@field _nameText        Engine.FunctionalPlainText | nil
---@field _infoTexts       Engine.FunctionalPlainText[]
---@field _ui              Source.UI.WindowEnemyEncyclopedia
local WindowEnemyEncyclopedia = {}

---@brief Construct the enemy encyclopedia window.
---
--- - @param rect Window rectangle.
--- - @param onClose Optional callback invoked when the window closes.
---@param rect    sf.IntRect
---@param onClose function | nil
function WindowEnemyEncyclopedia:init(rect, onClose) end

---@brief Open the detail window for an enemy-book entry.
---
--- - @param entry Prepared enemy display data.
---@param entry table
function WindowEnemyEncyclopedia:open(entry) end

---@brief Close the enemy encyclopedia window.
function WindowEnemyEncyclopedia:close() end

---@brief Refresh localised detail text without resetting the portrait animation or window focus.
function WindowEnemyEncyclopedia:refreshLocale() end

---@brief Close on confirm or cancel.
---
--- - @param kwargs Event data.
---@param kwargs table
function WindowEnemyEncyclopedia:onKeyDown(kwargs) end

---@brief Close on right click.
---@param kwargs table
---@return boolean
function WindowEnemyEncyclopedia:onMouseButtonDown(kwargs) end

---@brief Close the enemy detail through its cancel path.
function WindowEnemyEncyclopedia:onReturn() end

---@brief Update the animated portrait.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function WindowEnemyEncyclopedia:onTick(deltaTime) end

return WindowEnemyEncyclopedia
