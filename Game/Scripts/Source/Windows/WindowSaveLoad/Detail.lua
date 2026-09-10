local Save = require("Source.Save")
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
    self._cachedFilePath = ""
    self._cachedFileMTime = -1.0
    self._thumbTexture = nil
end

function Controller:setSlot(slot)
    if slot == self._currentSlot then
        self:_refreshIfFileChanged()
        return
    end
    self._currentSlot = slot
    self._cachedFilePath = ""
    self._cachedFileMTime = -1.0
    self:_refreshContent()
end

function Controller:refresh()
    self._cachedFilePath = ""
    self._cachedFileMTime = -1.0
    self:_refreshContent()
end

function Controller:onTick(deltaTime)
    WindowBase.onTick(self.host, deltaTime)
    self:_refreshIfFileChanged()
end

function Controller:_refreshIfFileChanged()
    if self._currentSlot == nil then
        return
    end
    local slotNumber = self._currentSlot + 1
    ---@cast slotNumber integer
    local filePath = Save.GetSavePath(slotNumber)
    if not os.path.isfile(filePath) then
        if bool(self._cachedFilePath) or self.ui.controls["Thumbnail"]:getVisible() then
            self._cachedFilePath = ""
            self._cachedFileMTime = -1.0
            self:_hideContent()
        end
        return
    end
    local modificationTime = os.path.getmtime(filePath)
    if filePath == self._cachedFilePath and modificationTime == self._cachedFileMTime then
        return
    end
    self._cachedFilePath = filePath
    self._cachedFileMTime = modificationTime
    self:_loadAndDisplay(filePath, modificationTime)
end

function Controller:_refreshContent()
    if self._currentSlot == nil then
        self:_hideContent()
        return
    end
    local slotNumber = self._currentSlot + 1
    ---@cast slotNumber integer
    local filePath = Save.GetSavePath(slotNumber)
    if not os.path.isfile(filePath) then
        self:_hideContent()
        return
    end
    local modificationTime = os.path.getmtime(filePath)
    self._cachedFilePath = filePath
    self._cachedFileMTime = modificationTime
    self:_loadAndDisplay(filePath, modificationTime)
end

---@param filePath         string
---@param modificationTime number
function Controller:_loadAndDisplay(filePath, modificationTime)
    local instance = Save.LoadGame(filePath)
    if instance == nil then
        self:_hideContent()
        return
    end
    local screenshot = instance:getScreenshot()
    if not self:_applyScreenshot(screenshot) then
        self.ui.controls["Thumbnail"]:setVisible(false)
    end
    self:setModificationTime(modificationTime)
    self.ui.controls["TimestampText"]:setVisible(true)
end

---@param screenshot integer[] | nil
---@return boolean
function Controller:_applyScreenshot(screenshot)
    if not bool(screenshot) then
        return false
    end
    local image = sf.Image.new(screenshot)
    local imageSize = image:getSize()
    assert(imageSize.x > 0 and imageSize.y > 0, "Save screenshot image has invalid dimensions")
    local texture = sf.Texture.new(image)
    texture:setSmooth(true)
    self._thumbTexture = texture
    self.ui.controls["Thumbnail"]:setTexture(texture, true)
    local scaleX = _DETAIL_THUMB_WIDTH / imageSize.x
    local scaleY = _DETAIL_THUMB_HEIGHT / imageSize.y
    self.ui.controls["Thumbnail"]:setScale(sf.Vector2f.new(scaleX, scaleY))
    self.ui.controls["Thumbnail"]:setPosition(sf.Vector2f.new(0.0, 0.0))
    self.ui.controls["Thumbnail"]:setVisible(true)
    return true
end

function Controller:_hideContent()
    self.ui.controls["Thumbnail"]:setVisible(false)
    self.ui.controls["TimestampText"]:setVisible(false)
    self:setTimestamp("")
end

function Controller:dispose()
    self._thumbTexture = nil
    super(Controller, self).dispose()
end

function Controller:setTimestamp(text)
    self:setText("TimestampText", text)
    self.ui:prepare()
end

function Controller:setModificationTime(modificationTime)
    self:setTimestamp(os.date("%Y-%m-%d %H:%M:%S", math.floor(modificationTime)))
end

return Ui.DefineWindow(View, Controller, WindowBase)
