local Engine = require("Engine")
local SourceSystem = require("Source.System")
local EventKeys = require("Source.Configs.EventKeys")
local WindowCommand = require("Source.Windows.WindowCommand")
local Ui = require("Source.UI.Ui")

local SceneTitleUI = {}

SceneTitleUI.refreshEvents = { EventKeys.LocaleChanged }

function SceneTitleUI:bind()
    self._commandWindowFrame = self:requireControl("CommandWindowFrame")
    self._commandContent = self:requireControl("CommandContent")
    self._commandScrollBox = self:requireControl("CommandScrollBox")
    self._commandList = self:requireControl("CommandList")
    self._commandList:clearChildren()
    self._commandModels = {
        {
            localeKey = "TITLE_START",
            callback = function ()
                self.model:_startGame()
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
                self.model:_exitGame()
            end
        }
    }
    self._windowCommand = WindowCommand.new(Engine.ToIntRect(192, 240, 256, 160), self._commandModels, 224, 32, nil, nil, 1, {
        windowFrame = self._commandWindowFrame,
        content = self._commandContent,
        scrollBox = self._commandScrollBox,
        listView = self._commandList
    })
end

function SceneTitleUI:refresh()
    self:setProperty("Background", "texture", SourceSystem.GetTitleBackgroundFile())
    self._windowCommand:refreshRows()
end

function SceneTitleUI:getCommandWindow()
    return self._windowCommand
end

return Ui.Define("Title", SceneTitleUI)
