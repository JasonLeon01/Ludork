local Engine = require("Engine")
local SourceSystem = require("Source.System")
local EventKeys = require("Source.Configs.EventKeys")
local WindowCommand = require("Source.Windows.WindowCommand")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Title")

---@class Source.Scenes.SceneTitle.Controller
local SceneTitleController = {}

SceneTitleController.refreshEvents = { EventKeys.LocaleChanged }

function SceneTitleController:bind()
    self._commandModels = {
        {
            localeKey = "TITLE_START",
            callback = function ()
                self.model:startGame()
            end
        },
        {
            localeKey = "TITLE_CONTINUE",
            callback = function ()
                self.model:openLoad()
            end
        },
        {
            localeKey = "TITLE_CONFIG",
            callback = function ()
                self.model:toggleConfig()
            end
        },
        {
            localeKey = "TITLE_EXIT",
            callback = function ()
                self.model:exitGame()
            end
        }
    }
    self._windowCommand = WindowCommand.new(
        Engine.ToIntRect(192, 240, 256, 160),
        self._commandModels,
        224,
        32,
        nil,
        nil,
        1,
        {
            windowFrame = self.ui.controls["CommandWindowFrame"],
            content = self.ui.controls["CommandContent"],
            scrollBox = self.ui.controls["CommandScrollBox"],
            listView = self.ui.controls["CommandList"]
        }
    )
    self.ui:own(self._windowCommand)
end

function SceneTitleController:refresh()
    self:setProperty("Background", "texture", SourceSystem.GetTitleBackgroundFile())
    self._windowCommand:refreshRows()
end

function SceneTitleController:getCommandWindow()
    return self._windowCommand
end

return Ui.Define(View, SceneTitleController)
