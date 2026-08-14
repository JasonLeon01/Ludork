local Engine = require("Engine")
local SourceSystem = require("Source.System")
local EventKeys = require("Source.Configs.EventKeys")
local WindowCommand = require("Source.Windows.WindowCommand")
local Ui = require("Source.UI.Ui")

local SceneTitleUI = {}

SceneTitleUI.refreshEvents = { EventKeys.LocaleChanged }

function SceneTitleUI:bind()
    self._commandModels = {
        {
            localeKey = "TITLE_START",
            callback = function ()
                self.model._startGame()
            end
        },
        {
            localeKey = "TITLE_CONTINUE",
            callback = function ()
                self.model:_onLoadCommand()
            end
        },
        {
            localeKey = "TITLE_CONFIG",
            callback = function ()
                self.model:_onConfigCommand()
            end
        },
        {
            localeKey = "TITLE_EXIT",
            callback = function ()
                self.model._exitGame()
            end
        }
    }
    self._windowCommand = WindowCommand.new(Engine.ToIntRect(0, 0, 256, 160), self._commandModels, nil, nil, nil, nil, 1)
    self._windowCommand:setOrigin(sf.Vector2f.new(128.0, 0))
    self._windowCommand:setPosition(sf.Vector2f.new(320.0, 240.0))
end

function SceneTitleUI:refresh()
    self:setProperty("Background", "texture", "Assets/System/" .. SourceSystem.getTitleBackgroundFile())
    self._windowCommand:refreshRows()
end

function SceneTitleUI:getCommandWindow()
    return self._windowCommand
end

return Ui.define("Title", SceneTitleUI)
