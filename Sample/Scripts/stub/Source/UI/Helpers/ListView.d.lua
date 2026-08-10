---@meta Source.UI.Helpers.ListView

---@class Source.UI.Helpers.ListView
---@field model any
---@field root Engine.ListView
---@field _logicalSize sf.Vector2u
---@field _defaultItemHeight integer
---@field _fixItemHeight boolean
---@field _columns integer
---@field _bound boolean
---@field new fun(model: any, logicalSize: sf.Vector2u, defaultItemHeight: integer, fixItemHeight: boolean, columns: integer): Source.UI.Helpers.ListView
local ListViewController = {}

function ListViewController:init(model, logicalSize, defaultItemHeight, fixItemHeight, columns) end

function ListViewController:bind() end

function ListViewController:refresh() end

---@param logicalSize sf.Vector2u | nil
---@return Engine.ListView
function ListViewController:prepare(logicalSize) end

---@return Engine.ListView
function ListViewController:getListView() end

return ListViewController
