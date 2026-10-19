---@meta Internal.UIBase.CommandRow.Controller

---@class Internal.UIBase.CommandRow.Controller.LocalizedModel
---@field localeKey string
---@field text      nil
---@field callback  function | nil

---@class Internal.UIBase.CommandRow.Controller.TextModel
---@field text      string
---@field localeKey nil
---@field callback  function | nil

---@alias Internal.UIBase.CommandRow.Controller.Model Internal.UIBase.CommandRow.Controller.LocalizedModel | Internal.UIBase.CommandRow.Controller.TextModel

---@class Internal.UIBase.CommandRow.Controller: Internal.UIBase.UiController
---@field ui    Internal.UI.Parts.Shared.CommandRow
---@field model Internal.UIBase.CommandRow.Controller.Model
---@field new   fun(model: Internal.UIBase.CommandRow.Controller.Model, ui: Internal.UI.Parts.Shared.CommandRow | nil): Internal.UIBase.CommandRow.Controller
local CommandRowController = {}

function CommandRowController:bind() end

function CommandRowController:refresh() end

---@brief Prepare a row using at least the list's item width, expanding to fit its label.
---@param itemSize sf.Vector2f
function CommandRowController:prepareForList(itemSize) end

return CommandRowController
