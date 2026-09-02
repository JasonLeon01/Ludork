---@meta Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell

---@class Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell.Model
---@field entry    Source.UI.WindowEnemyBook.Entry
---@field callback fun(obj: any, kwargs: table) | nil

---@class Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell: Source.UI.UiController
---@field model                Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell.Model
---@field root                 Engine.Canvas
---@field _specialDisplays     Source.UI.WindowEnemyBook.SpecialDisplay[]
---@field _specialDisplayTexts string[]
---@field _icon                Engine.CharacterView
---@field _nameText            Engine.PlainText
---@field _specialIcons        Engine.FunctionalImage[]
---@field _specialTexts        Engine.PlainText[]
---@field new                  fun(model: Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell.Model): Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell
local WindowEnemyBookCellUI = {}

---@param model Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell.Model
function WindowEnemyBookCellUI:init(model) end

function WindowEnemyBookCellUI:bind() end

function WindowEnemyBookCellUI:refresh() end

---@param logicalSize sf.Vector2u
---@return Engine.Canvas
function WindowEnemyBookCellUI:prepare(logicalSize) end

function WindowEnemyBookCellUI:refreshLocale() end

---@return Engine.CharacterView | nil
function WindowEnemyBookCellUI:getIcon() end

---@return sf.IntRect | nil
function WindowEnemyBookCellUI:getTextureRect() end

---@return number
function WindowEnemyBookCellUI:getSwitchTimer() end

return WindowEnemyBookCellUI
