---@meta

---@class Source.Windows.WindowEnemyEncyclopedia.Controller: Source.UIBase.UiController
---@field _onCloseCallback function | nil
---@field ui               Source.UI.WindowEnemyEncyclopedia
---@field _entry           Source.Windows.WindowEnemyBook.Entry | nil
---@field _infoRows        Source.UIBase.UiCollection<Source.Windows.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair.Controller>
---@field _specialRows     Source.UIBase.UiCollection<Source.Windows.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow.Controller>
---@field host             Source.Windows.WindowEnemyEncyclopedia
local Controller = {}

---@brief Construct the enemy encyclopedia window.
---
--- - @param onClose Optional callback invoked when the window closes.
---@param onClose function | nil
function Controller:init(onClose) end

---@brief Open the detail window for an enemy-book entry.
---
--- - @param entry Prepared enemy display data.
---@param entry Source.Windows.WindowEnemyBook.Entry
function Controller:open(entry) end

---@brief Close the enemy encyclopedia window.
function Controller:close() end

---@brief Refresh localised detail text without resetting the portrait animation or window focus.
function Controller:refreshLocale() end

---@brief Close on confirm or cancel.
---
--- - @param kwargs Event data.
---@param kwargs Engine.UiInputEventArguments
function Controller:onKeyDown(kwargs) end

---@brief Close on right click.
---@param kwargs Engine.UiInputEventArguments
---@return boolean
function Controller:onMouseButtonDown(kwargs) end

---@brief Close the enemy detail through its cancel path.
function Controller:onReturn() end

function Controller:refresh() end

---@param entry Source.Windows.WindowEnemyBook.Entry
function Controller:buildInfo(entry) end

---@param label string
---@param value string
function Controller:addInfoPair(label, value) end

---@param entry Source.Windows.WindowEnemyBook.Entry
function Controller:buildSpecials(entry) end

function Controller:clearEnemyControls() end
