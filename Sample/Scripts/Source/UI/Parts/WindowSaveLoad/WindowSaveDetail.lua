local Ui = require("Source.UI.Ui")

local WindowSaveDetailUI = {}
---@type function
local formatTimestamp

function WindowSaveDetailUI:init(model, size, instance)
    self._size = size
    super(WindowSaveDetailUI, self).init(model, instance)
end

function WindowSaveDetailUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._thumbnail = self:requireControl("Thumbnail")
    self._timestampText = self:requireControl("TimestampText")
end

function WindowSaveDetailUI:refresh()
    self:setText("TimestampText", "")
end

function WindowSaveDetailUI:prepare()
    return super(WindowSaveDetailUI, self).prepare(sf.Vector2u.new(self._size.x, self._size.y))
end

function WindowSaveDetailUI:attach(nested)
    if nested == true then
        self:attachNestedWindowView(self.model)
    else
        self:attachWindowView(self.model)
    end
end

function WindowSaveDetailUI:getWindowFrame()
    return self._windowFrame
end

function WindowSaveDetailUI:getContent()
    return self._content
end

function WindowSaveDetailUI:getThumbnail()
    return self._thumbnail
end

function WindowSaveDetailUI:getTimestampText()
    return self._timestampText
end

function WindowSaveDetailUI:setTimestamp(text)
    self:setText("TimestampText", text)
    self.view:reflow(sf.Vector2u.new(self._size.x, self._size.y))
end

function WindowSaveDetailUI:setModificationTime(modificationTime)
    self:setTimestamp(formatTimestamp(modificationTime))
end

function formatTimestamp(modificationTime)
    return os.date("%Y-%m-%d %H:%M:%S", math.floor(modificationTime))
end

return Ui.Define("Parts/WindowSaveLoad/WindowSaveDetail", WindowSaveDetailUI)
