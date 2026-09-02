local Ui = require("Source.UI.Ui")

---@class Source.UI.Parts.WindowSaveLoad.WindowSaveSlotRow
local WindowSaveSlotRowUI = {}

function WindowSaveSlotRowUI:bind()
    if self.model.callback ~= nil then
        ---@cast self.root Engine.Canvas
        self.root:addConfirmCallback(self.model.callback)
    end
end

function WindowSaveSlotRowUI:refresh()
    self:setText("Label", self.model.text)
end

return Ui.Define("Parts/WindowSaveLoad/WindowSaveSlotRow", WindowSaveSlotRowUI)
