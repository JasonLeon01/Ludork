---@meta Source.Windows.WindowItem.ItemRow.Controller

---@class Source.Windows.WindowItem.ItemRow.Controller.Model
---@field iconTexture sf.Texture | nil
---@field count       integer
---@field cost        boolean
---@field usable      boolean

---@class Source.Windows.WindowItem.ItemRow.Controller: Source.UIBase.UiController
---@field ui          Source.UI.Parts.WindowItem.ItemRow
---@field model       Source.Windows.WindowItem.ItemRow.Controller.Model
---@field new         fun(model: Source.Windows.WindowItem.ItemRow.Controller.Model): Source.Windows.WindowItem.ItemRow.Controller
---@field _iconColour sf.Color
local ItemRowController = {}

function ItemRowController:bind() end

function ItemRowController:refresh() end

return ItemRowController
