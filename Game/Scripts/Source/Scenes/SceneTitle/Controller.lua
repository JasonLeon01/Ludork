local Engine = require("Engine")
local SourceSystem = require("Source.System")
local EventKeys = require("Source.Configs.EventKeys")
local WindowCommand = require("Source.Windows.WindowCommand")
local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Title")

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
    self._windowCommand = self:createChild("CommandPanel", WindowCommand, self._commandModels)
    local savePath = Engine.getSavePath()
    self._windowCommand:selectIndex(os.path.isdir(savePath) and bool(os.listdir(savePath)) and 1 or 0)
end

function SceneTitleController:refresh()
    self:setProperty("Background", "texture", SourceSystem.GetTitleBackgroundFile())
    self._windowCommand:refreshRows()
end

function SceneTitleController:getCommandWindow()
    return self._windowCommand
end

return Ui.Define(View, SceneTitleController)
