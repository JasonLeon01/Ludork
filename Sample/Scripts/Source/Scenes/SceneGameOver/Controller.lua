local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.GameOver")

local GameOverController = {}

function GameOverController:refresh()
    self:setText("Message", "GAME OVER")
end

return Ui.Define(View, GameOverController)
