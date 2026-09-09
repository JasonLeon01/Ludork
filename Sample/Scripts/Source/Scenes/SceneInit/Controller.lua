local SourceSystem = require("Source.System")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Init")

local SceneInitController = {}

function SceneInitController:init(model, logicalSize)
    self._logicalSize = logicalSize
    self._progress = 0.0
    super(SceneInitController, self).init(model, nil)
end

function SceneInitController:refresh()
    self:setProperty("Background", "texture", SourceSystem.GetTitleBackgroundFile())
    self.ui.controls["ProgressBar"]:setProgress(self._progress)
end

function SceneInitController:onViewUpdate(payload)
    self:setProgress(payload.progress)
end

function SceneInitController:prepare(logicalSize)
    if logicalSize ~= nil then
        self._logicalSize = logicalSize
    end
    return super(SceneInitController, self).prepare(self._logicalSize)
end

function SceneInitController:getBackground()
    return self.ui.controls["Background"]
end

function SceneInitController:setProgress(value)
    self._progress = math.clamp(value, 0.0, 1.0)
end

return Ui.Define(View, SceneInitController)
