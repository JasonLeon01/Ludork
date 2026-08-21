---@meta Source.UI.WindowEnemyEncyclopedia

---@class Source.UI.WindowEnemyEncyclopedia: Source.UI.UiController
---@field model Source.Windows.WindowEnemyEncyclopedia
---@field _logicalSize sf.Vector2u
---@field _infoPairControllers Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair[]
---@field _specialRowControllers Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow[]
---@field _entry Source.UI.WindowEnemyBook.Entry | nil
---@field _previewController Source.UI.Parts.Shared.ActorPreviewController
---@field _windowFrame Engine.Window
---@field _content Engine.Canvas
---@field _portraitControl Engine.FunctionalImage
---@field _nameControl Engine.FunctionalPlainText
---@field _infoLayer Engine.Canvas
---@field _descriptionControl Engine.FunctionalPlainText
---@field _specialList Engine.ListView
---@field new fun(model: Source.Windows.WindowEnemyEncyclopedia, size: sf.Vector2i): Source.UI.WindowEnemyEncyclopedia
local WindowEnemyEncyclopediaUI = {}

---@param model Source.Windows.WindowEnemyEncyclopedia
---@param size  sf.Vector2i
function WindowEnemyEncyclopediaUI:init(model, size) end

function WindowEnemyEncyclopediaUI:bind() end

function WindowEnemyEncyclopediaUI:refresh() end

---@return Engine.Canvas
function WindowEnemyEncyclopediaUI:prepare() end

function WindowEnemyEncyclopediaUI:attach() end

---@return Engine.Window
function WindowEnemyEncyclopediaUI:getWindowFrame() end

---@return Engine.Canvas
function WindowEnemyEncyclopediaUI:getContent() end

---@param entry Source.UI.WindowEnemyBook.Entry
function WindowEnemyEncyclopediaUI:open(entry) end

function WindowEnemyEncyclopediaUI:refreshLocale() end

---@param entry Source.UI.WindowEnemyBook.Entry
---@param infoY number
function WindowEnemyEncyclopediaUI:buildInfo(entry, infoY) end

---@param label       string
---@param value       string
---@param columnIndex integer
---@param y           number
function WindowEnemyEncyclopediaUI:addInfoPair(label, value, columnIndex, y) end

---@param entry Source.UI.WindowEnemyBook.Entry
---@param y     number
function WindowEnemyEncyclopediaUI:buildSpecials(entry, y) end

---@param criticalValue integer | nil
---@return string
function WindowEnemyEncyclopediaUI.formatCriticalText(criticalValue) end

---@param hitCount integer | nil
---@return string
function WindowEnemyEncyclopediaUI.formatHitCount(hitCount) end

---@param text     string
---@param maxLines integer
---@param maxWidth integer
---@return string
function WindowEnemyEncyclopediaUI.limitLines(text, maxLines, maxWidth) end

---@param deltaTime number
function WindowEnemyEncyclopediaUI:tick(deltaTime) end

function WindowEnemyEncyclopediaUI:clearEnemyControls() end

return WindowEnemyEncyclopediaUI
