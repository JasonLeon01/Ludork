---@meta Source.UI.Parts.WindowSaveLoad.WindowSaveDetail

---@class Source.UI.Parts.WindowSaveLoad.WindowSaveDetail: Source.UI.UiController
---@field new fun(model: table, size: sf.Vector2i, instance?: Engine.AssetInstance): Source.UI.Parts.WindowSaveLoad.WindowSaveDetail
local WindowSaveDetailUI = {}

---@param model Source.Windows.WindowSaveDetail
---@param size  sf.Vector2i
function WindowSaveDetailUI:init(model, size, instance) end

function WindowSaveDetailUI:bind() end

function WindowSaveDetailUI:refresh() end

---@return Engine.Canvas
function WindowSaveDetailUI:prepare() end

function WindowSaveDetailUI:attach(nested) end

---@return Engine.Window
function WindowSaveDetailUI:getWindowFrame() end

---@return Engine.Canvas
function WindowSaveDetailUI:getContent() end

---@return Engine.Image
function WindowSaveDetailUI:getThumbnail() end

---@return Engine.FunctionalPlainText
function WindowSaveDetailUI:getTimestampText() end

---@param text string
function WindowSaveDetailUI:setTimestamp(text) end

---@param modificationTime number
function WindowSaveDetailUI:setModificationTime(modificationTime) end

return WindowSaveDetailUI
