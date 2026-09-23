---@meta Source.Windows.WindowEnemyBook.WindowEnemyBookCell.Controller

---@class Source.Windows.WindowEnemyBook.WindowEnemyBookCell.Controller.Model
---@field entry    Source.Windows.WindowEnemyBook.Entry
---@field callback fun(obj: any, kwargs: Engine.UiInputEventArguments) | nil

---@class Source.Windows.WindowEnemyBook.WindowEnemyBookCell.Controller: Internal.UIBase.UiController
---@field ui                   Internal.UI.Parts.WindowEnemyBook.WindowEnemyBookCell
---@field model                Source.Windows.WindowEnemyBook.WindowEnemyBookCell.Controller.Model
---@field root                 Engine.Canvas
---@field _specialDisplays     Source.Windows.WindowEnemyBook.SpecialDisplay[]
---@field _specialDisplayTexts string[]
---@field _specialViews        Internal.UI.Parts.WindowEnemyBook.SpecialDisplay[]
---@field _specialPadding      number[]
---@field _specialNameWidths   number[]
---@field _statColours         table<string, sf.Color>
---@field new                  fun(model: Source.Windows.WindowEnemyBook.WindowEnemyBookCell.Controller.Model): Source.Windows.WindowEnemyBook.WindowEnemyBookCell.Controller
local WindowEnemyBookCellController = {}

---@param model Source.Windows.WindowEnemyBook.WindowEnemyBookCell.Controller.Model
function WindowEnemyBookCellController:init(model) end

function WindowEnemyBookCellController:bind() end

function WindowEnemyBookCellController:refresh() end

---@param logicalSize sf.Vector2u | nil
---@return Engine.Canvas
function WindowEnemyBookCellController:prepare(logicalSize) end

function WindowEnemyBookCellController:refreshLocale() end

---@return Engine.CharacterView | nil
function WindowEnemyBookCellController:getIcon() end

---@return sf.IntRect | nil
function WindowEnemyBookCellController:getTextureRect() end

---@return number
function WindowEnemyBookCellController:getSwitchTimer() end

return WindowEnemyBookCellController
