local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.GameOver")

local GameOverController = {}

function GameOverController:refresh()
    self:setText("Message", "GAME OVER")
end

return Ui.Define(View, GameOverController)
