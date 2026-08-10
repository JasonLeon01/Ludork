local Ui = require("Source.UI.Ui")

local GameOverUI = {}

function GameOverUI:refresh()
    self:setText("Message", "GAME OVER")
end

return Ui.define("GameOver", GameOverUI)
