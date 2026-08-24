---@meta Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell

---@class Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell.Model
---@field entry    Source.UI.WindowEnemyBook.Entry
---@field callback fun(obj: any, kwargs: table) | nil

---@class Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell: Source.UI.UiController
---@field model                Source.UI.Parts.WindowEnemyBook.WindowEnemyBookCell.Model
---@field root                 Engine.Canvas
---@field _previewController   Source.UI.Parts.Shared.ActorPreviewController
---@field _specialDisplays     Source.UI.WindowEnemyBook.SpecialDisplay[]
---@field _specialDisplayTexts string[]
---@field _icon                Engine.FunctionalImage
---@field _nameText            Engine.FunctionalPlainText
---@field _specialIcons        Engine.FunctionalImage[]
---@field _specialTexts        Engine.FunctionalPlainText[]
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

---@param deltaTime number
function WindowEnemyBookCellUI:tick(deltaTime) end

---@return Engine.FunctionalImage | nil
function WindowEnemyBookCellUI:getIcon() end

---@return sf.IntRect | nil
function WindowEnemyBookCellUI:getTextureRect() end

---@return number
function WindowEnemyBookCellUI:getSwitchTimer() end

return WindowEnemyBookCellUI
