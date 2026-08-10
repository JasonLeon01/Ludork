local Engine = require("Engine")
local SourceSystem = require("Source.System")
local Ui = require("Source.UI.Ui")

local SceneInitUI = {}

function SceneInitUI:init(model, logicalSize)
    self._logicalSize = logicalSize
    self._progress = 0.0
    super(SceneInitUI, self).init(model, nil)
end

function SceneInitUI:bind()
    self._background = self:requireControl("Background")
    self._progressBar = self:requireControl("ProgressBar")
end

function SceneInitUI:refresh()
    self:setProperty("Background", "texture", "Assets/System/" .. SourceSystem.getTitleBackgroundFile())
    self._progressBar:setProgress(self._progress)
end

function SceneInitUI:onViewUpdate(payload)
    self:setProgress(payload.progress)
end

function SceneInitUI:prepare(logicalSize)
    if logicalSize ~= nil then
        self._logicalSize = logicalSize
    end
    return super(SceneInitUI, self).prepare(self._logicalSize)
end

function SceneInitUI:getBackground()
    return self._background
end

function SceneInitUI:setProgress(value)
    self._progress = Engine.Clamp(value, 0.0, 1.0)
end

return Ui.define("Init", SceneInitUI)
