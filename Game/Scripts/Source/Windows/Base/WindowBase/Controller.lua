local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.Shared.WindowBase")

local WindowBaseController = {}

function WindowBaseController:init(model, windowSkin, repeated, pauseMarkAtlasRect, pauseMarkFrameRect)
    self._pauseMarkAtlasRect = pauseMarkAtlasRect
    self._pauseMarkFrameRect = pauseMarkFrameRect
    self._windowSkin = windowSkin
    if repeated == nil then
        repeated = false
    end
    self._repeated = repeated
    super(WindowBaseController, self).init(model, nil)
end

function WindowBaseController:bind()
    self.ui.controls["WindowFrame"]:setWindowSkin(self._windowSkin, self._repeated)
    self._pauseMarkTexture = sf.Texture.new(self._windowSkin, false, self._pauseMarkAtlasRect)
    self._pauseMarkTexture:setSmooth(false)
    self.ui.controls["PauseMark"]:setTexture(self._pauseMarkTexture, true)
    self.ui.controls["PauseMark"]:setTextureRect(self._pauseMarkFrameRect)
    self.ui.controls["PauseMark"]:setVisible(false)
    self.ui.controls["ReturnButton"]:setVisible(false)
    self.ui.controls["ReturnButton"]:setActive(false)
end

function WindowBaseController:attachTo(parent, logicalSize)
    self:prepare(logicalSize)
    parent:attachPreparedView(self, {
        root = self.root,
        windowFrame = self.ui.controls["WindowFrame"],
        content = self.ui.controls["Content"],
        chromeRoot = self.ui.controls["Content"],
        returnButton = self.ui.controls["ReturnButton"],
        pauseMark = self.ui.controls["PauseMark"],
        pauseMarkTexture = self._pauseMarkTexture,
        nested = false
    })
end

function WindowBaseController:getWindow()
    return self.ui.controls["WindowFrame"]
end

function WindowBaseController:getContent()
    return self.ui.controls["Content"]
end

function WindowBaseController:getReturnButton()
    return self.ui.controls["ReturnButton"]
end

function WindowBaseController:getPauseMark()
    return self.ui.controls["PauseMark"]
end

function WindowBaseController:getPauseMarkTexture()
    return self._pauseMarkTexture
end

return Ui.Define(View, WindowBaseController)
