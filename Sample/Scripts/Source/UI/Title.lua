local Engine = require("Engine")
local SourceSystem = require("Source.System")
local LocaleCore = require("Source.Locale.Core")
local EventKeys = require("Source.Configs.EventKeys")
local WindowCommand = require("Source.Windows.WindowCommand")
local Ui = require("Source.UI.Ui")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local SceneTitleUI = {}

SceneTitleUI.refreshEvents = { EventKeys.LocaleChanged }

function SceneTitleUI:bind()
    self._commandModels = {
        {
            localeKey = "TITLE_START",
            text = "",
            callback = function ()
                self.model._startGame()
            end
        },
        {
            localeKey = "TITLE_CONTINUE",
            text = "",
            callback = function ()
                self.model:_onLoadCommand()
            end
        },
        {
            localeKey = "TITLE_CONFIG",
            text = "",
            callback = function ()
                self.model:_onConfigCommand()
            end
        },
        {
            localeKey = "TITLE_EXIT",
            text = "",
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
    for _, command in ipairs(self._commandModels) do
        command.text = LOC(command.localeKey)
    end
    self._windowCommand:refreshRows()
end

function SceneTitleUI:getCommandWindow()
    return self._windowCommand
end

return Ui.define("Title", SceneTitleUI)
