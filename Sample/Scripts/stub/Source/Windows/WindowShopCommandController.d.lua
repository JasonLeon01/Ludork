---@meta Source.Windows.WindowShopCommandController

---@class Source.Windows.WindowShopCommandController: Source.Windows.WindowCommand.Controller
local WindowShopCommandController = {}

---@param owner Source.Windows.WindowShop
---@return Source.UI.Helpers.CommandRowModel[]
function WindowShopCommandController.CreateCommands(owner) end

return WindowShopCommandController
