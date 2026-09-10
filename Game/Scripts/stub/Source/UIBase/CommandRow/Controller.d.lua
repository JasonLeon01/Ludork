---@meta Source.UIBase.CommandRow.Controller

---@class Source.UIBase.CommandRow.Controller.LocalizedModel
---@field localeKey string
---@field text      nil
---@field callback  function | nil

---@class Source.UIBase.CommandRow.Controller.TextModel
---@field text      string
---@field localeKey nil
---@field callback  function | nil

---@alias Source.UIBase.CommandRow.Controller.Model Source.UIBase.CommandRow.Controller.LocalizedModel | Source.UIBase.CommandRow.Controller.TextModel

---@class Source.UIBase.CommandRow.Controller: Source.UIBase.UiController
---@field ui    Source.UI.Parts.Shared.CommandRow
---@field model Source.UIBase.CommandRow.Controller.Model
---@field new   fun(model: Source.UIBase.CommandRow.Controller.Model): Source.UIBase.CommandRow.Controller
local CommandRowController = {}

---@param model Source.UIBase.CommandRow.Controller.Model
function CommandRowController:init(model) end

function CommandRowController:bind() end

function CommandRowController:refresh() end

return CommandRowController
