local CoreSystem = require("CoreSystem")
local Save = require("Source.Save")
local WindowSaveDetailUI = require("Source.UI.Parts.WindowSaveLoad.WindowSaveDetail")
local WindowBase = require("Source.Windows.Base.WindowBase")

local _DETAIL_THUMB_WIDTH = 224
local _DETAIL_THUMB_HEIGHT = 168
local WindowSaveDetail = {}

function WindowSaveDetail:init(rect)
    super(WindowSaveDetail, self).init(rect, nil, nil, true)
    self._currentSlot = nil
    self._cachedFilePath = ""
    self._cachedFileMTime = -1.0
    self._thumbTexture = nil
    self._ui = WindowSaveDetailUI.new(self, rect.size)
    self._ui:attach()
    self._thumbnail = self._ui:getThumbnail()
    self._timestampText = self._ui:getTimestampText()
    self._thumbnail:setVisible(false)
    self._timestampText:setVisible(false)
end

function WindowSaveDetail:setSlot(slot)
    if slot == self._currentSlot then
        self:_refreshIfFileChanged()
        return
    end
    self._currentSlot = slot
    self._cachedFilePath = ""
    self._cachedFileMTime = -1.0
    self:_refreshContent()
end

function WindowSaveDetail:refresh()
    self._cachedFilePath = ""
    self._cachedFileMTime = -1.0
    self:_refreshContent()
end

function WindowSaveDetail:onTick(deltaTime)
    super(WindowSaveDetail, self).onTick(deltaTime)
    self:_refreshIfFileChanged()
end

function WindowSaveDetail:_refreshIfFileChanged()
    if self._currentSlot == nil then
        return
    end
    local slotNumber = self._currentSlot + 1
    ---@cast slotNumber integer
    local filePath = Save.GetSavePath(slotNumber)
    if not CoreSystem.exists(filePath) then
        if bool(self._cachedFilePath) or self._thumbnail:getVisible() then
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

function WindowSaveDetail:_refreshContent()
    if self._currentSlot == nil then
        self:_hideContent()
        return
    end
    local slotNumber = self._currentSlot + 1
    ---@cast slotNumber integer
    local filePath = Save.GetSavePath(slotNumber)
    if not CoreSystem.exists(filePath) then
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
function WindowSaveDetail:_loadAndDisplay(filePath, modificationTime)
    local instance = Save.LoadGame(filePath)
    if instance == nil then
        self:_hideContent()
        return
    end
    local screenshot = instance:getScreenshot()
    if not self:_applyScreenshot(screenshot) then
        self._thumbnail:setVisible(false)
    end
    self._ui:setModificationTime(modificationTime)
    self._timestampText:setVisible(true)
end

---@param screenshot integer[] | nil
---@return boolean
function WindowSaveDetail:_applyScreenshot(screenshot)
    if not bool(screenshot) then
        return false
    end
    local image = sf.Image.new(screenshot)
    local imageSize = image:getSize()
    assert(imageSize.x > 0 and imageSize.y > 0, "Save screenshot image has invalid dimensions")
    local texture = sf.Texture.new(image)
    texture:setSmooth(true)
    self._thumbTexture = texture
    self._thumbnail:setTexture(texture, true)
    local scaleX = _DETAIL_THUMB_WIDTH / imageSize.x
    local scaleY = _DETAIL_THUMB_HEIGHT / imageSize.y
    self._thumbnail:setScale(sf.Vector2f.new(scaleX, scaleY))
    self._thumbnail:setPosition(sf.Vector2f.new(0.0, 0.0))
    self._thumbnail:setVisible(true)
    return true
end

function WindowSaveDetail:_hideContent()
    self._thumbnail:setVisible(false)
    self._timestampText:setVisible(false)
    self._ui:setTimestamp("")
end

return class(WindowSaveDetail, WindowBase)
