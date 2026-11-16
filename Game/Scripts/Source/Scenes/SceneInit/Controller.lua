local Locale = require("Source.Locale.Core")
local EventKey = require("Enums.EventKey")
local SourceSystem = require("Source.System")
local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Init")

local LOC = Locale.ApplyStringLocaleFormat

local SceneInitController = {}

SceneInitController.refreshEvents = { EventKey.LocaleChanged }

function SceneInitController:init(model, logicalSize)
    self._logicalSize = logicalSize
    self._progress = 0.0
    self._phaseKey = model.phaseKey
    super(SceneInitController, self).init(model, nil)
end

function SceneInitController:refresh()
    self:setProperty("Background", "texture", SourceSystem.GetTitleBackgroundFile())
    self.ui.controls["ProgressBar"]:setProgress(self._progress)
    self:setText("OperationText", LOC(self._phaseKey))
end

function SceneInitController:onViewUpdate(payload)
    if payload.progress ~= nil then
        self:setProgress(payload.progress)
    end
    if payload.phaseKey ~= nil then
        self._phaseKey = payload.phaseKey
    end
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
