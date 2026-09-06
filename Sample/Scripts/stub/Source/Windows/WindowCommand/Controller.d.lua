---@meta Source.Windows.WindowCommand.Controller

---@class Source.Windows.WindowCommand.Controller: Source.UI.Helpers.ListView
---@field model           Source.Windows.Base.WindowSelectable
---@field _size           sf.Vector2u
---@field _rowHeight      integer
---@field _columns        integer
---@field _rowWidth       integer
---@field _rowControllers Source.UI.Parts.Shared.CommandRow[]
---@field new             fun(model: Source.Windows.Base.WindowSelectable, size: sf.Vector2u, rowHeight: integer, columns: integer): Source.Windows.WindowCommand.Controller
local WindowCommandController = {}

---@param model     Source.Windows.Base.WindowSelectable
---@param size      sf.Vector2u
---@param rowHeight integer
---@param columns   integer
function WindowCommandController:init(model, size, rowHeight, columns) end

function WindowCommandController:refresh() end

---@return Engine.ListView
function WindowCommandController:prepare() end

---@param commands Source.UI.Parts.Shared.CommandRowModel[]
function WindowCommandController:attach(commands) end

---@param listView Engine.ListView
---@param commands Source.UI.Parts.Shared.CommandRowModel[]
function WindowCommandController:attachTo(listView, commands) end

---@param item Source.UI.Parts.Shared.CommandRowModel
function WindowCommandController:addRow(item) end

function WindowCommandController:refreshRows() end

---@param item Source.UI.Parts.Shared.CommandRowModel
---@return Engine.ControlBase
function WindowCommandController:createRow(item) end

return WindowCommandController
