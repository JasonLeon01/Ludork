---@meta Source.UI.WindowEnemyEncyclopedia

---@class Source.UI.WindowEnemyEncyclopedia: Source.UI.UiController
---@field model                  Source.Windows.WindowEnemyEncyclopedia
---@field _logicalSize           sf.Vector2u
---@field _infoPairControllers   Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair[]
---@field _specialRowControllers Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow[]
---@field _entry                 Source.UI.WindowEnemyBook.Entry | nil
---@field _windowFrame           Engine.Window
---@field _content               Engine.Canvas
---@field _portraitControl       Engine.CharacterView
---@field _nameControl           Engine.FunctionalPlainText
---@field _infoLayer             Engine.ListView
---@field _descriptionControl    Engine.PlainText
---@field _specialScrollBox      Engine.ScrollBox
---@field _specialList           Engine.ListView
---@field new                    fun(model: Source.Windows.WindowEnemyEncyclopedia, size: sf.Vector2i): Source.UI.WindowEnemyEncyclopedia
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
function WindowEnemyEncyclopediaUI:buildInfo(entry) end

---@param label string
---@param value string
function WindowEnemyEncyclopediaUI:addInfoPair(label, value) end

---@param entry Source.UI.WindowEnemyBook.Entry
---@param y     number
function WindowEnemyEncyclopediaUI:buildSpecials(entry, y) end

function WindowEnemyEncyclopediaUI:clearEnemyControls() end

return WindowEnemyEncyclopediaUI
