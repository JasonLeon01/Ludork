local Engine = require("Engine")
local Save = require("Source.Save")
local Logging = require("Global.Utils.Logging")
local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowSaveLoad.WindowSaveDetail")
local WindowBase = require("Source.Windows.Base.WindowBase")

local _DETAIL_THUMB_WIDTH = 224
local _DETAIL_THUMB_HEIGHT = 168
---@class Source.Windows.WindowSaveDetail.Controller
local Controller = {}

Controller.windowOptions = { focusable = false, hidden = true }

function Controller:init()
    self._currentSlot = nil
    self._reader = Engine.SavePreviewReader.new()
    self._previewEnabled = false
    self._refreshElapsed = 0
    self._thumbTexture = nil
    self._thumbImage = nil
    self._requestClock = sf.Clock.new()
    self._reportTiming = false
    self._lastError = ""
end

function Controller:setPreviewEnabled(enabled)
    self._previewEnabled = enabled
    self._reader:cancel()
    if enabled then
        self:refresh()
    end
end

function Controller:setSlot(slot)
    if slot == self._currentSlot then
        return
    end
    self._currentSlot = slot
    self:refresh()
end

function Controller:refresh()
    self._reader:cancelPreview()
    self._thumbImage = nil
    self._thumbTexture = nil
    self.ui.controls["Thumbnail"]:setVisible(false)
    if not self._previewEnabled or self._currentSlot == nil then
        self:setTimestamp("")
        return
    end
    self:_showStatus("loading")
    self._requestClock:restart()
    self._reportTiming = true
    self:_requestPreview()
end

function Controller:_requestPreview()
    if self._currentSlot == nil then
        return
    end
    local slotNumber = self._currentSlot + 1
    ---@cast slotNumber integer
    self._reader:requestPreview(Save.GetSavePath(slotNumber), _DETAIL_THUMB_WIDTH, _DETAIL_THUMB_HEIGHT)
    self._refreshElapsed = 0
end

function Controller:onTick(deltaTime)
    WindowBase.onTick(self.host, deltaTime)
    if not self._previewEnabled then
        return
    end
    if self._reader:pollPreview() then
        self:_displayPreview()
    end
    self._refreshElapsed = self._refreshElapsed + deltaTime
    if self._refreshElapsed >= 1 and self._reader:getState() ~= "loading" then
        self:_requestPreview()
    end
end

function Controller:_displayPreview()
    local state = self._reader:getState()
    if state == "ready" then
        local image = self._reader:getImage()
        if image ~= self._thumbImage then
            self._thumbImage = image
            self._thumbTexture = nil
            if image ~= nil then
                local texture = sf.Texture.new(image)
                texture:setSmooth(true)
                self._thumbTexture = texture
                self.ui.controls["Thumbnail"]:setTexture(texture, true)
                local size = image:getSize()
                local scale = math.min(_DETAIL_THUMB_WIDTH / size.x, _DETAIL_THUMB_HEIGHT / size.y)
                self.ui.controls["Thumbnail"]:setScale(sf.Vector2f.new(scale, scale))
                self.ui.controls["Thumbnail"]:setPosition(sf.Vector2f.new(0, 0))
            end
        end
        self.ui.controls["Thumbnail"]:setVisible(image ~= nil)
        self:setModificationTime(self._reader:getModificationTime())
        self._lastError = ""
    else
        self.ui.controls["Thumbnail"]:setVisible(false)
        self:_showStatus(state)
        local error = self._reader:getError()
        if error ~= "" and error ~= self._lastError then
            Logging.warning("Save preview failed: %s", error)
        end
        self._lastError = error
    end
    if self._reportTiming then
        Logging.info(
            "Save preview slot %s ready in %.2f ms (%s)", tostring(self._currentSlot),
            self._requestClock:getElapsedTime():asMicroseconds() / 1000, state
        )
        self._reportTiming = false
    end
end

function Controller:_showStatus(state)
    local chinese = LocaleCore.GetLanguage() == "zh_CN"
    if state == "loading" then
        self:setTimestamp(chinese and "正在读取存档…" or "Loading save…")
    elseif state == "empty" then
        self:setTimestamp(chinese and "空存档槽" or "Empty slot")
    else
        self:setTimestamp(chinese and "存档读取失败" or "Unable to read save")
    end
end

function Controller:dispose()
    self._reader:cancel()
    self._previewEnabled = false
    self._thumbImage = nil
    self._thumbTexture = nil
    super(Controller, self).dispose()
end

function Controller:setTimestamp(text)
    if text == self._timestamp then
        return
    end
    self._timestamp = text
    self:setText("TimestampText", text)
    self.ui.controls["TimestampText"]:setVisible(text ~= "")
    self.ui:prepare()
end

function Controller:setModificationTime(modificationTime)
    self:setTimestamp(os.date("%Y-%m-%d %H:%M:%S", math.floor(modificationTime)))
end

return Ui.DefineWindow(View, Controller, WindowBase)
