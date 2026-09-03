local Ui = require("Source.UI.Ui")

local WindowBaseUI = {}

function WindowBaseUI:init(model, windowSkin, repeated)
    self._windowSkin = windowSkin
    if repeated == nil then
        repeated = false
    end
    self._repeated = repeated
    super(WindowBaseUI, self).init(model, nil)
end

function WindowBaseUI:bind()
    self._window = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._returnButton = self:requireControl("ReturnButton")
    self._pauseMark = self:requireControl("PauseMark")
    self._window:setWindowSkin(self._windowSkin, self._repeated)
    self._pauseMarkTexture = sf.Texture.new(self._windowSkin, false, self.model._PAUSE_MARK_ATLAS_RECT)
    self._pauseMarkTexture:setSmooth(false)
    self._pauseMark:setTexture(self._pauseMarkTexture, true)
    self._pauseMark:setTextureRect(self.model._PAUSE_MARK_FRAME_RECTS[1])
    self._pauseMark:setVisible(false)
    self._returnButton:setVisible(false)
    self._returnButton:setActive(false)
end

function WindowBaseUI:attachTo(parent, logicalSize)
    self:prepare(logicalSize)
    parent:addChild(self.root)
    parent:_setUiController(self)
end

function WindowBaseUI:getWindow()
    return self._window
end

function WindowBaseUI:getContent()
    return self._content
end

function WindowBaseUI:getReturnButton()
    return self._returnButton
end

function WindowBaseUI:getPauseMark()
    return self._pauseMark
end

function WindowBaseUI:getPauseMarkTexture()
    return self._pauseMarkTexture
end

return Ui.Define("Parts/Shared/WindowBase", WindowBaseUI)
