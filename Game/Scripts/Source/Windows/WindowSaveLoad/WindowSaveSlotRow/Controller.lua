local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowSaveLoad.WindowSaveSlotRow")

---@class Source.Windows.WindowSaveLoad.WindowSaveSlotRow.Controller
local WindowSaveSlotRowController = {}

function WindowSaveSlotRowController:bind()
    if self.model.callback ~= nil then
        ---@cast self.root Engine.Canvas
        self.root:addConfirmCallback(self.model.callback)
    end
end

function WindowSaveSlotRowController:refresh()
    self:setText("Label", self.model.text)
end

return Ui.Define(View, WindowSaveSlotRowController)
