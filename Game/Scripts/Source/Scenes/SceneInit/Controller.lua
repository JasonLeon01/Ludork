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

function SceneInitController:bind()
    self:setProperty("Background", "texture", SourceSystem.GetTitleBackgroundFile())
end

function SceneInitController:refresh()
    local hasPhase = self._phaseKey ~= ""
    self.ui.controls["Splash"]:setVisible(not hasPhase)
    self.ui.controls["Background"]:setVisible(hasPhase)
    self.ui.controls["ProgressBar"]:setVisible(hasPhase)
    self.ui.controls["OperationText"]:setVisible(hasPhase)
    self.ui.controls["ProgressBar"]:setProgress(self._progress)
    if hasPhase then
        self:setText("OperationText", LOC(self._phaseKey))
    end
end

function SceneInitController:onViewUpdate(payload)
    if payload.progress ~= nil then
        self:setProgress(payload.progress)
    end
    if payload.phaseKey ~= nil then
        self._phaseKey = payload.phaseKey
    end
end

---@param image Engine.Image
---@param size  sf.Vector2u
local function fitSplashImage(image, size)
    local pixels = image:getTextureRect().size
    local scale = math.min(size.x / pixels.x, size.y / pixels.y)
    image:setScale(sf.Vector2f.new(scale, scale))
    image:setPosition(sf.Vector2f.new((size.x - pixels.x * scale) / 2, (size.y - pixels.y * scale) / 2))
end

function SceneInitController:prepare(logicalSize)
    if logicalSize ~= nil then
        self._logicalSize = logicalSize
    end
    local root = super(SceneInitController, self).prepare(self._logicalSize)
    if self._phaseKey == "" then
        fitSplashImage(self.ui.controls["SplashImage"], self._logicalSize)
    end
    return root
end

function SceneInitController:getBackground()
    return self.ui.controls["Background"]
end

function SceneInitController:setProgress(value)
    self._progress = math.clamp(value, 0.0, 1.0)
end

return Ui.Define(View, SceneInitController)
